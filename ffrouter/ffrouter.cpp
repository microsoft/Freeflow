// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "ffrouter.h"
#include "rdma_api.h"
#include "verbs_cmd.h"
#include <ifaddrs.h>


void mem_flush(const void *p, int allocation_size)
{
    const size_t cache_line = 64;
    const char *cp = (const char *)p;
    size_t i = 0;

    if (p == NULL || allocation_size <= 0)
            return;

    for (i = 0; i < allocation_size; i += cache_line) {
            asm volatile("clflush (%0)\n\t"
                         : 
                         : "r"(&cp[i])
                         : "memory");
    }

    asm volatile("sfence\n\t"
                 :
                 :
                 : "memory");
}

FreeFlowRouter::~FreeFlowRouter()
{
    for(std::map<int, std::vector<ShmPiece*> >::iterator it = this->shm_pool.begin(); it != this->shm_pool.end(); it++)
    {
        for (int i = 0; i < it->second.size(); i++)
        {
            delete it->second[i];
        }
    }

    for(std::map<std::string, ShmPiece* >::iterator it = this->shm_map.begin(); it != this->shm_map.end(); it++)
    {
        delete it->second;
    }    
}

ShmPiece* FreeFlowRouter::addShmPiece(int client_id, int mem_size)
{
    pthread_mutex_lock(&this->shm_mutex);
    if (this->shm_pool.find(client_id) == this->shm_pool.end())
    {
        std::vector<ShmPiece*> v;
        this->shm_pool[client_id] = v;
    }

    int count = this->shm_pool[client_id].size();

    std::stringstream ss;
    ss << "client-" << client_id << "-memsize-" << mem_size << "-index-" << count;

    ShmPiece *sp = new ShmPiece(ss.str().c_str(), mem_size);
    this->shm_pool[client_id].push_back(sp);
    if (!sp->open())
    {
        sp = NULL;
    }

    pthread_mutex_unlock(&this->shm_mutex);
    return sp;
}

ShmPiece* FreeFlowRouter::addShmPiece(std::string shm_name, int mem_size)
{
    pthread_mutex_lock(&this->shm_mutex);
    if (this->shm_map.find(shm_name) != this->shm_map.end())
    {
        pthread_mutex_unlock(&this->shm_mutex);
        return this->shm_map[shm_name];
    }

    ShmPiece *sp = new ShmPiece(shm_name.c_str(), mem_size);
    if (!sp->open())
    {
        sp = NULL;
    }

    this->shm_map[shm_name] = sp;
    pthread_mutex_unlock(&this->shm_mutex);
    return sp;
}

ShmPiece* FreeFlowRouter::initCtrlShm(const char* tag)
{
    std::stringstream ss;
    ss << "ctrlshm-" << tag;

    ShmPiece *sp = new ShmPiece(ss.str().c_str(), sizeof(struct CtrlShmPiece));
    if (!sp->open())
    {
        sp = NULL;
    }

    if (!sp)
        LOG_ERROR("Failed to create control shm for tag  " << tag);

    memset(sp->ptr, 0, sizeof(struct CtrlShmPiece));

    struct CtrlShmPiece *csp = (struct CtrlShmPiece *)(sp->ptr);
    csp->state = IDLE;
    return sp;
}

FreeFlowRouter::FreeFlowRouter(const char* name) {
    LOG_INFO("FreeFlowRouter Init");
    
    this->name = name;
    this->pathname = "/freeflow/";
    this->pathname.append(this->name);

    this->host_ip = 0;

    if (getenv("HOST_IP_PREFIX")) {
        const char* prefix = getenv("HOST_IP_PREFIX");
        uint32_t prefix_ip = 0, prefix_mask = 0;
        uint8_t a, b, c, d, bits;
        if (sscanf(prefix, "%hhu.%hhu.%hhu.%hhu/%hhu", &a, &b, &c, &d, &bits) == 5) {
            if (bits <= 32) {
                prefix_ip = htonl(
                    (a << 24UL) |
                    (b << 16UL) |
                    (c << 8UL) |
                    (d));
                prefix_mask = htonl((0xFFFFFFFFUL << (32 - bits)) & 0xFFFFFFFFUL);
            }
        }
        if (prefix_ip != 0 || prefix_mask != 0) {
            struct ifaddrs *ifaddr, *ifa;
            getifaddrs(&ifaddr);
            ifa = ifaddr;   
            while (ifa) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
                {
                    struct sockaddr_in *pAddr = (struct sockaddr_in *)ifa->ifa_addr;
                    if ((pAddr->sin_addr.s_addr & prefix_mask) == (prefix_ip & prefix_mask)) {
                        this->host_ip = pAddr->sin_addr.s_addr;
                        break;
                    }
                }
                ifa = ifa->ifa_next;
            }
            freeifaddrs(ifaddr);
        }
    }

    if (getenv("HOST_IP")) {
        this->host_ip = inet_addr(getenv("HOST_IP"));
    }

    if (!this->host_ip) {
        LOG_ERROR("Missing HOST_IP or HOST_IP_PREFIX. Socket may not work.");
    }
    else {
        struct in_addr addr_tmp;
        addr_tmp.s_addr = this->host_ip;
        LOG_INFO("Socket binding address:" << inet_ntoa(addr_tmp));
    }

    if (!getenv("RDMA_POLLING_INTERVAL_US")) {
        this->rdma_polling_interval = 0;
    }
    else {
        this->rdma_polling_interval = atoi(getenv("RDMA_POLLING_INTERVAL_US"));
    }

    if (!getenv("DISABLE_RDMA")) {
        this->disable_rdma = 0;
    }
    else {
        this->disable_rdma = atoi(getenv("DISABLE_RDMA"));
    }


    LOG_DEBUG("Pathname for Unix domain socket: " << this->pathname);

    for (int i = 0; i < MAP_SIZE; i++)
    {
        this->pd_map[i] = NULL;
        this->cq_map[i] = NULL;
        this->qp_map[i] = NULL;
        this->mr_map[i] = NULL;
        this->ah_map[i] = NULL;
        this->srq_map[i] = NULL;
        this->shmr_map[i] = NULL;
        this->qp_shm_map[i] = NULL;
        this->cq_shm_map[i] = NULL;
        this->srq_shm_map[i] = NULL;
        this->channel_map[i] = NULL;
        this->event_channel_map[i] = NULL;
        this->cm_id_map[i] = NULL;
    }

    pthread_mutex_init(&this->qp_shm_vec_mtx, NULL);
    pthread_mutex_init(&this->cq_shm_vec_mtx, NULL);
    pthread_mutex_init(&this->srq_shm_vec_mtx, NULL);
    pthread_mutex_init(&this->rkey_mr_shm_mtx, NULL);
    pthread_mutex_init(&this->lkey_ptr_mtx, NULL);
    pthread_mutex_init(&this->shm_mutex, NULL);

    if (!this->disable_rdma) {
        setup_ib(&this->rdma_data);
        LOG_DEBUG("RDMA Dev: dev.name=" << this->rdma_data.ib_device->name << ", " <<  "dev.dev_name=" << this->rdma_data.ib_device->dev_name);    
    }

    this->vip_map["10.47.0.4"] = "192.168.2.13";
    this->vip_map["10.47.0.6"] = "192.168.2.13";
    this->vip_map["10.47.0.7"] = "192.168.2.13";
    this->vip_map["10.47.0.8"] = "192.168.2.13";
    this->vip_map["10.44.0.3"] = "192.168.2.15";
    this->vip_map["10.44.0.4"] = "192.168.2.15";
    this->vip_map["10.44.0.6"] = "192.168.2.15";
}

void FreeFlowRouter::start()
{
    LOG_INFO("FreeFlowRouter Starting... ");

    if (!disable_rdma) {
        start_udp_server();

        pthread_t ctrl_th; //the fast data path thread
        struct HandlerArgs ctrl_args;
        ctrl_args.ffr = this;
        pthread_create(&ctrl_th, NULL, (void* (*)(void*))CtrlChannelLoop, &ctrl_args); 
        sleep(1.0);
    }

    char c;
    // FILE *fp;
    register int i, len;
    struct sockaddr_un saun;

    if ((this->sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("Cannot create Unix domain socket.");
        exit(1);
    }

    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, this->pathname.c_str());

    unlink(this->pathname.c_str());
    len = sizeof(saun.sun_family) + strlen(saun.sun_path);

    if (bind(this->sock, (const sockaddr*)&saun, len) < 0) {
        LOG_ERROR("Cannot bind Unix domain socket.");
        exit(1);
    }

    if (listen(this->sock, 128) < 0) {
        LOG_ERROR("Cannot listen Unix domain socket.");
        exit(1);
    } 
 
    int client_sock;
    int fromlen = sizeof(struct sockaddr_un);
    struct sockaddr_un fsaun;
    memset(&fsaun, 0, sizeof fsaun);

    int count = 0;

    LOG_DEBUG("Accepting new clients... ");
    
    while (1)
    {
        if ((client_sock = accept(this->sock, (sockaddr*)&fsaun, (socklen_t*)&fromlen)) < 0) {
            LOG_ERROR("Failed to accept." << errno);
            exit(1);
        }
        LOG_TRACE("New client with sock " << client_sock << ".");
    
        // Start a thread to handle the request.     
        pthread_t *pth = (pthread_t *) malloc(sizeof(pthread_t));
        struct HandlerArgs *args = (struct HandlerArgs *) malloc(sizeof(struct HandlerArgs));
        args->ffr = this;
        args->client_sock = client_sock;
        int ret = pthread_create(pth, NULL, (void* (*)(void*))HandleRequest, args);
    LOG_TRACE("result of pthread_create --> " << ret);
        count ++;
    }
}

void CtrlChannelLoop(struct HandlerArgs *args)
{
    LOG_INFO("Start the control channel loop.");

    FreeFlowRouter *ffr = args->ffr;
    cpu_set_t cpuset; 

    //the CPU we want to use
    int cpu = 2;

    CPU_ZERO(&cpuset);       //clears the cpuset
    CPU_SET( cpu , &cpuset); //set CPU 2 on cpuset


    /*
     * cpu affinity for the calling thread 
     * first parameter is the pid, 0 = calling thread
     * second parameter is the size of your cpuset
     * third param is the cpuset in which your thread will be
     * placed. Each bit represents a CPU
     */
    sched_setaffinity(0, sizeof(cpuset), &cpuset);

    unsigned int count = 0;
    ShmPiece *qp_sp = NULL;
    ShmPiece *cq_sp = NULL;
    ShmPiece *srq_sp = NULL;
    struct CtrlShmPiece *qp_csp = NULL;
    struct CtrlShmPiece *cq_csp = NULL;
    struct CtrlShmPiece *srq_csp = NULL;


    void *req_body, *rsp;
    struct FfrRequestHeader *req_header;
    struct FfrResponseHeader *rsp_header;

    struct ibv_qp *qp = NULL;
    TokenBucket *tb = NULL;
    struct ibv_cq *cq = NULL;
    struct ibv_srq *srq = NULL;
    struct ibv_wc *wc_list = NULL;

    while (1)
    {
        pthread_mutex_lock(&ffr->qp_shm_vec_mtx);
        for (int i = 0; i < ffr->qp_shm_vec.size(); i ++)
        {
        qp_sp = ffr->qp_shm_map[ffr->qp_shm_vec[i]];

        // QP operations.
        if (qp_sp)
        {
            qp_csp = (struct CtrlShmPiece*)qp_sp->ptr;
            //rmb();
            if (qp_csp->state == REQ_DONE)
            {
                //clock_gettime(CLOCK_REALTIME, &st);

                req_header = (struct FfrRequestHeader *)qp_csp->req;
                req_body = qp_csp->req + sizeof(struct FfrRequestHeader);
                rsp_header = (struct FfrResponseHeader *)qp_csp->rsp;
                rsp = qp_csp->rsp + sizeof(struct FfrResponseHeader);
                switch(req_header->func)
                {
                    case IBV_POST_SEND:
                    {
                    // Now recover the qp and wr
                    struct ibv_post_send *post_send = (struct ibv_post_send*)req_body;
                    if (post_send->qp_handle >= MAP_SIZE)
                    {
                        LOG_ERROR("[Warning] QP handle (" << post_send->qp_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                    }
                    else
                    {
                        qp = ffr->qp_map[post_send->qp_handle];
                        tb = ffr->tokenbucket[post_send->qp_handle];
                    }

                    struct ibv_send_wr *wr = (struct ibv_send_wr*)((char*)req_body + sizeof(struct ibv_post_send));
                    struct ibv_sge *sge = (struct ibv_sge*)((char*)req_body + sizeof(struct ibv_post_send) + post_send->wr_count * sizeof(struct ibv_send_wr));

                    uint32_t *ah = NULL; 
                    if (qp->qp_type == IBV_QPT_UD) {
                        //LOG_INFO("POST_SEND_UD!!!");
                        ah = (uint32_t*)(sge + post_send->sge_count);
                    }

                    uint32_t wr_success = 0;
                    int count = 0;
                    for (int i = 0; i < post_send->wr_count; i++)
                    {
            //LOG_DEBUG("wr[i].wr_id=" << wr[i].wr_id << " opcode=" << wr[i].opcode <<  " imm_data==" << wr[i].imm_data);

                        if (wr[i].opcode == IBV_WR_RDMA_WRITE || wr[i].opcode == IBV_WR_RDMA_WRITE_WITH_IMM || wr[i].opcode == IBV_WR_RDMA_READ) {
                            
                            while(1)
                            {
                                pthread_mutex_lock(&ffr->rkey_mr_shm_mtx);
                                if (ffr->rkey_mr_shm.find(wr[i].wr.rdma.rkey) == ffr->rkey_mr_shm.end()) {
                                    if (count > 4)
                                    {
                                         LOG_ERROR("One sided opertaion: can't find remote MR. rkey --> " << wr[i].wr.rdma.rkey << "  addr --> " << wr[i].wr.rdma.remote_addr);
                                         pthread_mutex_unlock(&ffr->rkey_mr_shm_mtx);
                                         break;
                                    }
                                }
                                else {
                                    ;//LOG_DEBUG("shm:" << (uint64_t)(ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].shm_ptr) << " app:" << (uint64_t)(wr[i].wr.rdma.remote_addr) << " mr:" << (uint64_t)(ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].mr_ptr));
                                    wr[i].wr.rdma.remote_addr = (uint64_t)(ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].shm_ptr) + (uint64_t)wr[i].wr.rdma.remote_addr - (uint64_t)ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].mr_ptr;
                                    pthread_mutex_unlock(&ffr->rkey_mr_shm_mtx);
                                    break;
                                }

                                pthread_mutex_unlock(&ffr->rkey_mr_shm_mtx);
                                sleep(0.5);
                                count ++;
                            }
                        }

                        // fix the link list pointer
                        if (i >= post_send->wr_count - 1) {
                            wr[i].next = NULL;
                        }
                        else {
                            wr[i].next = &(wr[i+1]);
                        }
                        if (wr[i].num_sge > 0) {
                            // fix the sg list pointer
                            wr[i].sg_list = sge;
                            pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                            for (int j = 0; j < wr[i].num_sge; j++) {
                                /*while (!(tb->consume(sge[j].length))) {
                                    uint32_t stime = sge[j].length * 1000000 / MAX_QP_RATE_LIMIT;
                                    if (stime) {
                                        usleep(stime);
                                    }
                                    else {
                                        usleep(1);
                                    }
                                    //wr[i-1].next = NULL;
                                    //break;
                                }*/
                                //LOG_DEBUG("wr[i].wr_id=" << wr[i].wr_id << " qp_num=" << qp->qp_num << " sge.addr=" << sge[j].addr << " sge.length" << sge[j].length << " opcode=" << wr[i].opcode);
                                sge[j].addr = (uint64_t)((char*)(ffr->lkey_ptr[sge[j].lkey]) + sge[j].addr);
                                //LOG_DEBUG("data=" << ((char*)(sge[j].addr))[0] << ((char*)(sge[j].addr))[1] << ((char*)(sge[j].addr))[2]);
                                //LOG_DEBUG("imm_data==" << wr[i].imm_data);

                            }
                            pthread_mutex_unlock(&ffr->lkey_ptr_mtx);

                            sge += wr[i].num_sge;
                        }
                else {
                wr[i].sg_list = NULL;
                }

                        // fix ah
                        if (qp->qp_type == IBV_QPT_UD) {
                            wr[i].wr.ud.ah = ffr->ah_map[*ah];
                        }

                        wr_success++;
                    }

                    struct ibv_send_wr *bad_wr = NULL;
                    //rsp = malloc(sizeof(struct IBV_POST_SEND_RSP));
                    rsp_header->rsp_size = sizeof(struct IBV_POST_SEND_RSP);

                    ((struct IBV_POST_SEND_RSP*)rsp)->ret_errno = ibv_post_send(qp, wr, &bad_wr);
                    if (((struct IBV_POST_SEND_RSP*)rsp)->ret_errno != 0) {
                        LOG_ERROR("[Error] Post send (" << qp->handle << ") fails.");
                    }

                    //LOG_DEBUG("post_send success.");

                    if (bad_wr == NULL) {
                        // this IF is not needed right now, but left here for future use
                        if (post_send->wr_count == wr_success) {
                            ((struct IBV_POST_SEND_RSP*)rsp)->bad_wr = 0;
                        }
                        else {
                            ((struct IBV_POST_SEND_RSP*)rsp)->bad_wr = post_send->wr_count - wr_success;
                            ((struct IBV_POST_SEND_RSP*)rsp)->ret_errno = ENOMEM;
                        }
                    }
                    else{
                        LOG_ERROR("bad_wr is not NULL.");
                        ((struct IBV_POST_SEND_RSP*)rsp)->bad_wr = bad_wr - wr;
                    }

                    }
                    break;

                    case IBV_POST_RECV:
                    {
                   // LOG_DEBUG("IBV_POST_RECV");

                    // Now recover the qp and wr
                    struct ibv_post_recv *post_recv = (struct ibv_post_recv*)req_body;
                    if (post_recv->qp_handle >= MAP_SIZE)
                    {
                        LOG_ERROR("[Warning] QP handle (" << post_recv->qp_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                    }
                    else
                    {
                        qp = ffr->qp_map[post_recv->qp_handle];
                    }

                    struct ibv_recv_wr *wr = (struct ibv_recv_wr*)((char*)req_body + sizeof(struct ibv_post_recv));
                    struct ibv_sge *sge = (struct ibv_sge*)((char*)req_body + sizeof(struct ibv_post_recv) + post_recv->wr_count * sizeof(struct ibv_recv_wr));

                    for (int i = 0; i < post_recv->wr_count; i++)
                    {
                        // fix the link list pointer
                        if (i >= post_recv->wr_count - 1) {
                            wr[i].next = NULL;
                        }
                        else {
                            wr[i].next = &(wr[i+1]);
                        }
                        if (wr[i].num_sge > 0) {
                            // fix the sg list pointer
                            wr[i].sg_list = sge;
                            pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                            for (int j = 0; j < wr[i].num_sge; j++) {
                                sge[j].addr = (uint64_t)(ffr->lkey_ptr[sge[j].lkey]) + (uint64_t)(sge[j].addr);
                            }
                            pthread_mutex_unlock(&ffr->lkey_ptr_mtx);
                            sge += wr[i].num_sge;
                        }
                        else {
                            wr[i].sg_list = NULL;	
                        }
                        //LOG_ERROR("wr[i].sg_list=" << wr[i].sg_list << " wr[i].num_sge=" << wr[i].num_sge);
                    }

                    struct ibv_recv_wr *bad_wr = NULL;
                    //rsp = malloc(sizeof(struct IBV_POST_RECV_RSP));
                    rsp_header->rsp_size = sizeof(struct IBV_POST_RECV_RSP);
                    ((struct IBV_POST_RECV_RSP*)rsp)->ret_errno = ibv_post_recv(qp, wr, &bad_wr);
                    if (((struct IBV_POST_RECV_RSP*)rsp)->ret_errno != 0) {
                        LOG_ERROR("[Error] Post recv (" << qp->handle << ") fails. error=" << ((struct IBV_POST_RECV_RSP*)rsp)->ret_errno);
                    }
                    if (bad_wr == NULL) 
                    {
                        ((struct IBV_POST_RECV_RSP*)rsp)->bad_wr = 0;
                    }
                    else
                    {
                        ((struct IBV_POST_RECV_RSP*)rsp)->bad_wr = bad_wr - wr;
                    }

                    }
                    break;

                    default:
                    break;
               }
               
               wmb();
               qp_csp->state = RSP_DONE;
               //wc_wmb();
               //mem_flush((void*)&qp_csp->state, sizeof(enum CtrlChannelState));
           /*clock_gettime(CLOCK_REALTIME, &et);
               LOG_ERROR("REQ_DONE tv_sec=" << st.tv_sec << ", tv_nsec=" << st.tv_nsec);
               LOG_SRQROR("RSP_DONE tv_sec=" << et.tv_sec << ", tv_nsec=" << et.tv_nsec);*/
           }
        }
        }
        
        pthread_mutex_unlock(&ffr->qp_shm_vec_mtx);

        // CQ operations.
        pthread_mutex_lock(&ffr->cq_shm_vec_mtx);

        for (int i = 0; i < ffr->cq_shm_vec.size(); i ++)
        {

            cq_sp = ffr->cq_shm_map[ffr->cq_shm_vec[i]];

            if (cq_sp)
            {
                cq_csp = (struct CtrlShmPiece*)cq_sp->ptr;
                //rmb();
                switch (cq_csp->state)
                {
                    case REQ_DONE:
                    { 

                        req_header = (struct FfrRequestHeader *)cq_csp->req;
                        req_body = cq_csp->req + sizeof(struct FfrRequestHeader);
                        rsp_header = (struct FfrResponseHeader *)cq_csp->rsp;
                        rsp = cq_csp->rsp + sizeof(struct FfrResponseHeader);

                        if (((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle >= MAP_SIZE)
                        {
                            LOG_ERROR("CQ handle (" << ((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                        }
                        else
                        {
                            cq = ffr->cq_map[((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle];
                        }

                        if (cq == NULL)
                        {
                            LOG_ERROR("cq pointer is NULL cq_handle -->" << ((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle);
                            break;
                        }

                        wc_list = (struct ibv_wc*)((char *)rsp);
                    
                        count = ibv_poll_cq(cq, ((struct IBV_POLL_CQ_REQ *)req_body)->ne, wc_list);
                        if (count <= 0)
                        {
                            rsp_header->rsp_size = 0;
                        }
                        else
                        {
                            rsp_header->rsp_size = count * sizeof(struct ibv_wc);
                        }

                for (i = 0; i < count; i++)
                {
                    if (wc_list[i].status == 0)
                    {
                        LOG_DEBUG("======== wc =========");
                        LOG_DEBUG("wr_id=" << wc_list[i].wr_id); 
                        LOG_DEBUG("status=" << wc_list[i].status); 
                        LOG_DEBUG("opcode=" << wc_list[i].opcode); 
                        LOG_DEBUG("vendor_err=" << wc_list[i].vendor_err); 
                        LOG_DEBUG("byte_len=" << wc_list[i].byte_len); 
                        LOG_DEBUG("imm_data=" << wc_list[i].imm_data); 
                        LOG_DEBUG("qp_num=" << wc_list[i].qp_num); 
                        LOG_DEBUG("src_qp=" << wc_list[i].src_qp); 
                        LOG_DEBUG("wc_flags=" << wc_list[i].wc_flags); 
                        LOG_DEBUG("pkey_index=" << wc_list[i].pkey_index); 
                        LOG_DEBUG("slid=" << wc_list[i].slid); 
                        LOG_DEBUG("sl=" << wc_list[i].sl); 
                        LOG_DEBUG("dlid_path_bits=" << wc_list[i].dlid_path_bits);                         
                    }
                    else
                    {
                        LOG_DEBUG("########## wc ############");
                        LOG_DEBUG("wr_id=" << wc_list[i].wr_id); 
                        LOG_DEBUG("status=" << wc_list[i].status); 
                        LOG_DEBUG("opcode=" << wc_list[i].opcode); 
                        LOG_DEBUG("vendor_err=" << wc_list[i].vendor_err); 
                        LOG_DEBUG("byte_len=" << wc_list[i].byte_len); 
                        LOG_DEBUG("imm_data=" << wc_list[i].imm_data); 
                        LOG_DEBUG("qp_num=" << wc_list[i].qp_num); 
                        LOG_DEBUG("src_qp=" << wc_list[i].src_qp); 
                        LOG_DEBUG("wc_flags=" << wc_list[i].wc_flags); 
                        LOG_DEBUG("pkey_index=" << wc_list[i].pkey_index); 
                        LOG_DEBUG("slid=" << wc_list[i].slid); 
                        LOG_DEBUG("sl=" << wc_list[i].sl); 
                        LOG_DEBUG("dlid_path_bits=" << wc_list[i].dlid_path_bits);                         
                    }
                }

                        wmb();
                        cq_csp->state = RSP_DONE;

                        //wc_wmb();
                        //mem_flush((void*)&cq_csp->state, 4);

                    }
                    break;

                    default: break;	
                }
            }
         }

        pthread_mutex_unlock(&ffr->cq_shm_vec_mtx);

        // SRQ operations
        pthread_mutex_lock(&ffr->srq_shm_vec_mtx);

        for (int i = 0; i < ffr->srq_shm_vec.size(); i ++)
        {

            srq_sp = ffr->srq_shm_map[ffr->srq_shm_vec[i]];

            if (srq_sp)
            {
                srq_csp = (struct CtrlShmPiece*)srq_sp->ptr;
                if (srq_csp->state == REQ_DONE)
                {
                    // Now recover the qp and wr
                    req_header = (struct FfrRequestHeader *)srq_csp->req;
                    req_body = srq_csp->req + sizeof(struct FfrRequestHeader);
                    rsp_header = (struct FfrResponseHeader *)srq_csp->rsp;
                    rsp = srq_csp->rsp + sizeof(struct FfrResponseHeader);

                    struct ibv_post_srq_recv *post_recv = (struct ibv_post_srq_recv*)req_body;

                    if (post_recv->srq_handle >= MAP_SIZE)
                    {
                        LOG_ERROR("[Warning] SRQ handle (" << post_recv->srq_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                    }
                    else
                    {
                        srq = ffr->srq_map[post_recv->srq_handle];
                    }

                    struct ibv_recv_wr *wr = (struct ibv_recv_wr*)((char*)req_body + sizeof(struct ibv_post_srq_recv));
                    struct ibv_sge *sge = (struct ibv_sge*)((char*)req_body + sizeof(struct ibv_post_srq_recv) + post_recv->wr_count * sizeof(struct ibv_recv_wr));

                    for (int i = 0; i < post_recv->wr_count; i++)
                    {
                        // fix the link list pointer
                        if (i >= post_recv->wr_count - 1) {
                            wr[i].next = NULL;
                        }
                        else {
                            wr[i].next = &(wr[i+1]);
                        }
                        if (wr[i].num_sge > 0) {
                            // fix the sg list pointer
                            wr[i].sg_list = sge;
                            pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                            for (int j = 0; j < wr[i].num_sge; j++) {
                                sge[j].addr = (uint64_t)(ffr->lkey_ptr[sge[j].lkey]) + (uint64_t)(sge[j].addr);
                            }
                            pthread_mutex_unlock(&ffr->lkey_ptr_mtx);
                            sge += wr[i].num_sge;
                        }
                        else {
                            wr[i].sg_list = NULL;	
                        }
                    }

                    struct ibv_recv_wr *bad_wr = NULL;
                    //rsp = malloc(sizeof(struct IBV_POST_SRQ_RECV_RSP));
                    rsp_header->rsp_size = sizeof(struct IBV_POST_SRQ_RECV_RSP);
                    ((struct IBV_POST_SRQ_RECV_RSP*)rsp)->ret_errno = ibv_post_srq_recv(srq, wr, &bad_wr);
                    if (((struct IBV_POST_SRQ_RECV_RSP*)rsp)->ret_errno != 0) {
                        LOG_ERROR("[Error] Srq post recv (" << srq->handle << ") fails.");
                    }
                    if (bad_wr == NULL) 
                    {
                        ((struct IBV_POST_SRQ_RECV_RSP*)rsp)->bad_wr = 0;
                    }
                    else
                    {
                        ((struct IBV_POST_SRQ_RECV_RSP*)rsp)->bad_wr = bad_wr - wr;
                    }

                    wmb();
                    srq_csp->state = RSP_DONE;
                }
            }
        }

        pthread_mutex_unlock(&ffr->srq_shm_vec_mtx);

        if (ffr->rdma_polling_interval > 0) {
            usleep(ffr->rdma_polling_interval);
        }
    }
}


void HandleRequest(struct HandlerArgs *args)
{
    LOG_TRACE("Start to handle the request from client sock " << args->client_sock << ".");

    FreeFlowRouter *ffr = args->ffr;
    int client_sock = args->client_sock;

    // Speed up
    char *req_body = NULL;
    char *rsp = NULL;

    if (ffr->disable_rdma) {
        req_body = (char*)malloc(0xff);
        rsp = (char*)malloc(0xff);
    }
    else {
        req_body = (char*)malloc(0xfffff);
        rsp = (char*)malloc(0xfffff);
    }

    while(1)
    {
        int n = 0, size = 0, count = 0, i = 0, ret = 0, host_fd = -1;
        //void *req_body = NULL;
        //void *rsp = NULL;
        void *context = NULL;
        struct ibv_cq *cq = NULL;
        struct ibv_qp *qp = NULL;
        struct ibv_pd *pd = NULL;
        struct ibv_mr *mr = NULL;
        struct ibv_ah *ah = NULL;
        struct ibv_srq *srq = NULL;
        struct ibv_comp_channel *channel = NULL;
        struct rdma_event_channel *event_channel = NULL;
        struct rdma_cm_id *cm_id = NULL;
        ShmPiece *sp = NULL;
        struct ibv_wc *wc_list = NULL;
        TokenBucket *tb = NULL;
        struct FfrRequestHeader header;
    
    LOG_TRACE("Start to read from sock " << client_sock);
        
        if ((n = read(client_sock, &header, sizeof(header))) < sizeof(header))
        {
            if (n < 0)
                LOG_ERROR("Failed to read the request header. Read bytes: " << n << " Size of Header: " << sizeof(header));

            goto kill;
        }
    else
    {
        LOG_TRACE("Get request cmd " << header.func);
    }

        switch(header.func)
        {
            case IBV_GET_CONTEXT:
            {
                LOG_DEBUG("GET_CONTEXT");
                //rsp = malloc(sizeof(struct IBV_GET_CONTEXT_RSP));
                size = sizeof(struct IBV_GET_CONTEXT_RSP);
                ((struct IBV_GET_CONTEXT_RSP *)rsp)->async_fd = ffr->rdma_data.ib_context->async_fd;
                ((struct IBV_GET_CONTEXT_RSP *)rsp)->num_comp_vectors = ffr->rdma_data.ib_context->num_comp_vectors;

            }
            break;

            case IBV_QUERY_DEV:
            {
                LOG_DEBUG("QUERY_DEV client_id=" << client_sock);
                //rsp = malloc(sizeof(struct IBV_QUERY_DEV_RSP));
                size = sizeof(struct IBV_QUERY_DEV_RSP);
                memcpy(&((struct IBV_QUERY_DEV_RSP *)rsp)->dev_attr, &ffr->rdma_data.ib_dev_attr, sizeof(struct ibv_device_attr));
                
                /*((struct IBV_QUERY_DEV_RSP *)rsp)->dev_attr.max_srq = 0;
                ((struct IBV_QUERY_DEV_RSP *)rsp)->dev_attr.max_srq_wr = 0;
                ((struct IBV_QUERY_DEV_RSP *)rsp)->dev_attr.max_srq_sge = 0;*/

                LOG_DEBUG("Finished QUERY_DEV client_id=" << client_sock);
            }
            break;

            case IBV_EXP_QUERY_DEV:
            {
                LOG_DEBUG("EXP_QUERY_DEV client_id=" << client_sock << " cmd_fd=" << ffr->rdma_data.ib_context->cmd_fd);

                if (read(client_sock, req_body, sizeof(struct IBV_EXP_QUERY_DEV_REQ)) < sizeof(struct IBV_EXP_QUERY_DEV_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                //rsp = malloc(sizeof(struct IBV_QUERY_DEV_RSP));
                size = sizeof(struct IBV_EXP_QUERY_DEV_RSP);

                ((struct IBV_EXP_QUERY_DEV_RSP *)rsp)->ret_errno = ibv_exp_cmd_query_device_resp(
                    ffr->rdma_data.ib_context->cmd_fd,
                    &((IBV_EXP_QUERY_DEV_REQ*)req_body)->cmd,
                    &((IBV_EXP_QUERY_DEV_RSP*)rsp)->resp);
                
                size = sizeof(struct IBV_EXP_QUERY_DEV_RSP);
        if (((struct IBV_EXP_QUERY_DEV_RSP *)rsp)->ret_errno != 0)
            LOG_ERROR("Return error (" << ((struct IBV_EXP_QUERY_DEV_RSP *)rsp)->ret_errno  << ") in EXP_QUERY_DEV");
            }
            break;
 
        
            case IBV_QUERY_PORT:
            {
                LOG_DEBUG("QUERY_PORT client_id=" << client_sock);
                //req_body = malloc(sizeof(struct IBV_QUERY_PORT_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_QUERY_PORT_REQ)) < sizeof(struct IBV_QUERY_PORT_REQ))
                {
                    LOG_ERROR("Failed to read request body.");
                    goto kill;
                }

                //rsp = malloc(sizeof(struct IBV_QUERY_PORT_RSP));
                size = sizeof(struct IBV_QUERY_PORT_RSP);
                if (ibv_query_port(ffr->rdma_data.ib_context, 
                    ((IBV_QUERY_PORT_REQ*)req_body)->port_num, &((struct IBV_QUERY_PORT_RSP *)rsp)->port_attr) < 0)
                {
                    LOG_ERROR("Cannot query port" << ((IBV_QUERY_PORT_REQ*)req_body)->port_num);
                }
            }
            break;

            case IBV_ALLOC_PD:
            {
                LOG_DEBUG("ALLOC_PD");

                //rsp = malloc(sizeof(struct IBV_ALLOC_PD_RSP));
                size = sizeof(struct IBV_ALLOC_PD_RSP);
                pd = ibv_alloc_pd(ffr->rdma_data.ib_context); 

                if (pd->handle >= MAP_SIZE)
                {
                    LOG_INFO("PD handle is no less than MAX_QUEUE_MAP_SIZE. pd_handle=" << pd->handle); 
                }
                else
                {
                    ffr->pd_map[pd->handle] = pd;
                }

                ((struct IBV_ALLOC_PD_RSP *)rsp)->pd_handle = pd->handle;
                LOG_DEBUG("Return pd_handle " << pd->handle << "for client_id " << header.client_id); 
                
            }
            break;

            case IBV_DEALLOC_PD:
            {
                LOG_DEBUG("DEALLOC_PD");

                //req_body = malloc(sizeof(struct IBV_DEALLOC_PD_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_DEALLOC_PD_REQ)) < sizeof(struct IBV_DEALLOC_PD_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Dealloc PD: " << ((IBV_DEALLOC_PD_REQ*)req_body)->pd_handle); 

                pd = ffr->pd_map[((struct IBV_DEALLOC_PD_REQ *)req_body)->pd_handle];
                if (pd == NULL)
                {
                    LOG_ERROR("Failed to get pd with pd_handle " << ((struct IBV_DEALLOC_PD_REQ *)req_body)->pd_handle);
                    goto end;
                }

                ffr->pd_map[((struct IBV_DEALLOC_PD_REQ *)req_body)->pd_handle] = NULL;
                ret = ibv_dealloc_pd(pd);
                //rsp = malloc(sizeof(struct IBV_DEALLOC_PD_RSP));
                ((struct IBV_DEALLOC_PD_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_DEALLOC_PD_RSP);
            }
            break;

            case IBV_CREATE_CQ:
            {
                LOG_INFO("CREATE_CQ, body_size=" << header.body_size);
                //req_body = malloc(sizeof(struct IBV_CREATE_CQ_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_CREATE_CQ_REQ)) < sizeof(struct IBV_CREATE_CQ_REQ))
                {
                    LOG_ERROR("DESTROY_CQ: Failed to read the request body."); 
                    goto kill;
                }

                if (((struct IBV_CREATE_CQ_REQ *)req_body)->channel_fd < 0)
                {
                    channel = NULL;
                }
                else
                {
                    channel = ffr->channel_map[((struct IBV_CREATE_CQ_REQ *)req_body)->channel_fd];
                    if (channel == NULL)
                    {
                        LOG_ERROR("Failed to get channel with fd " << ((struct IBV_CREATE_CQ_REQ *)req_body)->channel_fd);
                        goto end;
                    }
                }

                cq = ibv_create_cq(ffr->rdma_data.ib_context, ((struct IBV_CREATE_CQ_REQ *)req_body)->cqe, NULL, channel, ((struct IBV_CREATE_CQ_REQ *)req_body)->comp_vector);
                if (cq->handle >= MAP_SIZE)
                {
                    LOG_INFO("CQ handle (" << cq->handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    ffr->cq_map[cq->handle] = cq;
                }

                //rsp = malloc(sizeof(struct IBV_CREATE_CQ_RSP));
                ((struct IBV_CREATE_CQ_RSP *)rsp)->cqe = cq->cqe;
                ((struct IBV_CREATE_CQ_RSP *)rsp)->handle = cq->handle;
                size = sizeof(struct IBV_CREATE_CQ_RSP);

                LOG_DEBUG("Create CQ: cqe=" << cq->cqe << " handle=" << cq->handle);

                std::stringstream ss;
                ss << "cq" << cq->handle;
                ShmPiece* sp = ffr->initCtrlShm(ss.str().c_str());
                ffr->cq_shm_map[cq->handle] = sp;
                strcpy(((struct IBV_CREATE_CQ_RSP *)rsp)->shm_name, sp->name.c_str());
                pthread_mutex_lock(&ffr->cq_shm_vec_mtx);
                ffr->cq_shm_vec.push_back(cq->handle);
                pthread_mutex_unlock(&ffr->cq_shm_vec_mtx);
            }
            break;

            case IBV_DESTROY_CQ:
            {
                LOG_DEBUG("DESTROY_CQ, body_size=" << header.body_size);

                //req_body = malloc(sizeof(struct IBV_DESTROY_CQ_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_DESTROY_CQ_REQ)) < sizeof(struct IBV_DESTROY_CQ_REQ))
                {
                    LOG_ERROR("DESTROY_CQ: Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("cq_handle in request: " << ((struct IBV_DESTROY_CQ_REQ *)req_body)->cq_handle);

                cq = ffr->cq_map[((struct IBV_DESTROY_CQ_REQ *)req_body)->cq_handle];
                if (cq == NULL)
                {
                    LOG_ERROR("Failed to get cq with cq_handle " << ((struct IBV_DESTROY_CQ_REQ *)req_body)->cq_handle);
                    goto end;
                }
 
                LOG_DEBUG("found cq from cq_map");
        
                ffr->cq_map[((struct IBV_DESTROY_CQ_REQ *)req_body)->cq_handle] = NULL;
                ret = ibv_destroy_cq(cq);

                //rsp = malloc(sizeof(struct IBV_DESTROY_CQ_RSP));
                ((struct IBV_DESTROY_CQ_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_DESTROY_CQ_RSP);

                pthread_mutex_lock(&ffr->cq_shm_vec_mtx);
                std::vector<uint32_t>::iterator position = std::find(ffr->cq_shm_vec.begin(), ffr->cq_shm_vec.end(), ((struct IBV_DESTROY_CQ_REQ *)req_body)->cq_handle);
                if (position != ffr->cq_shm_vec.end()) // == myVector.end() means the element was not found
                    ffr->cq_shm_vec.erase(position);
                pthread_mutex_unlock(&ffr->cq_shm_vec_mtx); 

                ShmPiece* sp = ffr->cq_shm_map[((struct IBV_DESTROY_CQ_REQ *)req_body)->cq_handle];
                if (sp)
                    delete sp;
                ffr->cq_shm_map[((struct IBV_DESTROY_CQ_REQ *)req_body)->cq_handle] = NULL;

           }
            break;

            case IBV_REQ_NOTIFY_CQ:
            {
                //LOG_DEBUG("REQ_NOTIFY_CQ");

                //req_body = malloc(sizeof(struct IBV_REQ_NOTIFY_CQ_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_REQ_NOTIFY_CQ_REQ)) < sizeof(struct IBV_REQ_NOTIFY_CQ_REQ))
                {
                    LOG_ERROR("DESTROY_CQ: Failed to read the request body."); 
                    goto kill;
                }

                cq = ffr->cq_map[((struct IBV_REQ_NOTIFY_CQ_REQ *)req_body)->cq_handle];
                if (cq == NULL)
                {
                    LOG_ERROR("Failed to get cq with cq_handle " << ((struct IBV_REQ_NOTIFY_CQ_REQ *)req_body)->cq_handle);
                    goto end;
                }

                ret = ibv_req_notify_cq(cq, ((struct IBV_REQ_NOTIFY_CQ_REQ *)req_body)->solicited_only);

                //rsp = malloc(sizeof(struct IBV_REQ_NOTIFY_CQ_RSP));
                ((struct IBV_REQ_NOTIFY_CQ_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_REQ_NOTIFY_CQ_RSP);
            }
            break;
            
            case IBV_CREATE_QP:
            {
                LOG_DEBUG("CREATE_QP");

                //req_body = malloc(sizeof(struct IBV_CREATE_QP_REQ));
                if ((n = read(client_sock, req_body, sizeof(struct IBV_CREATE_QP_REQ))) < sizeof(struct IBV_CREATE_QP_REQ))
                {
                    LOG_ERROR("CREATE_CQ: Failed to read the request body."); 
                    goto kill;
                }
 
                struct ibv_qp_init_attr init_attr;
                bzero(&init_attr, sizeof(init_attr));
                init_attr.qp_type = ((struct IBV_CREATE_QP_REQ *)req_body)->qp_type;
                init_attr.sq_sig_all = ((struct IBV_CREATE_QP_REQ *)req_body)->sq_sig_all;

                init_attr.srq = ffr->srq_map[((struct IBV_CREATE_QP_REQ *)req_body)->srq_handle];
                init_attr.send_cq = ffr->cq_map[((struct IBV_CREATE_QP_REQ *)req_body)->send_cq_handle];
                init_attr.recv_cq = ffr->cq_map[((struct IBV_CREATE_QP_REQ *)req_body)->recv_cq_handle];

                init_attr.cap.max_send_wr = ((struct IBV_CREATE_QP_REQ *)req_body)->cap.max_send_wr;
                init_attr.cap.max_recv_wr = ((struct IBV_CREATE_QP_REQ *)req_body)->cap.max_recv_wr;
                init_attr.cap.max_send_sge = ((struct IBV_CREATE_QP_REQ *)req_body)->cap.max_send_sge;
                init_attr.cap.max_recv_sge = ((struct IBV_CREATE_QP_REQ *)req_body)->cap.max_recv_sge;
                init_attr.cap.max_inline_data = ((struct IBV_CREATE_QP_REQ *)req_body)->cap.max_inline_data;

                LOG_TRACE("init_attr.qp_type=" << init_attr.qp_type); 
                LOG_TRACE("init_attr.sq_sig_all=" << init_attr.sq_sig_all); 
                LOG_TRACE("init_attr.srq=" << ((struct IBV_CREATE_QP_REQ *)req_body)->srq_handle); 
                LOG_TRACE("init_attr.send_cq=" << ((struct IBV_CREATE_QP_REQ *)req_body)->send_cq_handle); 
                LOG_TRACE("init_attr.recv_cq=" << ((struct IBV_CREATE_QP_REQ *)req_body)->recv_cq_handle); 
                LOG_TRACE("init_attr.cap.max_send_wr=" << init_attr.cap.max_send_wr); 
                LOG_TRACE("init_attr.cap.max_recv_wr=" << init_attr.cap.max_recv_wr); 
                LOG_TRACE("init_attr.cap.max_send_sge=" << init_attr.cap.max_send_sge); 
                LOG_TRACE("init_attr.cap.max_recv_sge=" << init_attr.cap.max_recv_sge); 
                LOG_TRACE("init_attr.cap.max_inline_data=" << init_attr.cap.max_inline_data); 
                            

                pd = ffr->pd_map[((struct IBV_CREATE_QP_REQ *)req_body)->pd_handle];
                LOG_TRACE("Get pd " << pd << "from pd_handle " << ((struct IBV_CREATE_QP_REQ *)req_body)->pd_handle);             
           
                qp = ibv_create_qp(pd, &init_attr);
                if (qp == NULL)
                {
                    LOG_ERROR("Failed to create a QP.");             
                    goto end;
                }

                if (qp->handle >= MAP_SIZE)
                {
                    LOG_ERROR("[Warning] QP handle (" << qp->handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    ffr->qp_map[qp->handle] = qp;

                    char env_name[32];
                    sprintf(env_name, "RATE_LIMIT_%d", header.client_id);
                    char* rate_env = getenv(env_name);
                    if (!rate_env) {
                        ffr->tokenbucket[qp->handle] = new TokenBucket(MAX_QP_RATE_LIMIT, BURST_PER_QP);
                        LOG_INFO("Create a qp for client=" << header.client_id << " with rate limit=" << MAX_QP_RATE_LIMIT*8/1000000 << "Mbps");
                    }
                    else {
                        std::stringstream ss(rate_env);
                        uint64_t rate_limit;
                        ss >> rate_limit;
                        ffr->tokenbucket[qp->handle] = new TokenBucket(rate_limit / 8, BURST_PER_QP);
                        LOG_INFO("Create a qp for client=" << header.client_id << " with rate limit=" << rate_limit/1000000 << "Mbps");
                    }
                }

                //rsp = malloc(sizeof(struct IBV_CREATE_QP_RSP));
                ((struct IBV_CREATE_QP_RSP *)rsp)->qp_num = qp->qp_num;
                ((struct IBV_CREATE_QP_RSP *)rsp)->handle = qp->handle;

                LOG_TRACE("qp->qp_num=" << qp->qp_num);
                LOG_TRACE("qp->handle=" << qp->handle); 

                ((struct IBV_CREATE_QP_RSP *)rsp)->cap.max_send_wr = init_attr.cap.max_send_wr;
                ((struct IBV_CREATE_QP_RSP *)rsp)->cap.max_recv_wr = init_attr.cap.max_recv_wr;
                ((struct IBV_CREATE_QP_RSP *)rsp)->cap.max_send_sge = init_attr.cap.max_send_sge;
                ((struct IBV_CREATE_QP_RSP *)rsp)->cap.max_recv_sge = init_attr.cap.max_recv_sge;
                ((struct IBV_CREATE_QP_RSP *)rsp)->cap.max_inline_data = init_attr.cap.max_inline_data;
            
                size = sizeof(struct IBV_CREATE_QP_RSP);    

                std::stringstream ss;
                ss << "qp" << qp->handle;
                ShmPiece* sp = ffr->initCtrlShm(ss.str().c_str());
                ffr->qp_shm_map[qp->handle] = sp;
                strcpy(((struct IBV_CREATE_QP_RSP *)rsp)->shm_name, sp->name.c_str());
                pthread_mutex_lock(&ffr->qp_shm_vec_mtx);
                ffr->qp_shm_vec.push_back(qp->handle);
                pthread_mutex_unlock(&ffr->qp_shm_vec_mtx);
            }
            break;

            case IBV_DESTROY_QP:
            {
                LOG_DEBUG("DESTROY_QP");

                //req_body = malloc(sizeof(struct IBV_DESTROY_QP_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_DESTROY_QP_REQ)) < sizeof(struct IBV_DESTROY_QP_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("Destroy QP: " << ((IBV_DESTROY_QP_REQ*)req_body)->qp_handle); 

                qp = ffr->qp_map[((struct IBV_DESTROY_QP_REQ *)req_body)->qp_handle];
                if (qp == NULL)
                {
                    LOG_ERROR("Failed to get qp with qp_handle " << ((struct IBV_DESTROY_QP_REQ *)req_body)->qp_handle);
                    goto end;
                }

                ffr->qp_map[((struct IBV_DESTROY_QP_REQ *)req_body)->qp_handle] = NULL;
                ret = ibv_destroy_qp(qp);
                //rsp = malloc(sizeof(struct IBV_DESTROY_QP_RSP));
                ((struct IBV_DESTROY_QP_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_DESTROY_QP_RSP);

                pthread_mutex_lock(&ffr->qp_shm_vec_mtx);
                std::vector<uint32_t>::iterator position = std::find(ffr->qp_shm_vec.begin(), ffr->qp_shm_vec.end(), ((struct IBV_DESTROY_QP_REQ *)req_body)->qp_handle);
                if (position != ffr->qp_shm_vec.end()) // == myVector.end() means the element was not found
                    ffr->qp_shm_vec.erase(position);
                pthread_mutex_unlock(&ffr->qp_shm_vec_mtx); 

                ShmPiece* sp = ffr->qp_shm_map[((struct IBV_DESTROY_QP_REQ *)req_body)->qp_handle];
                if (sp)
                    delete sp;
                ffr->qp_shm_map[((struct IBV_DESTROY_QP_REQ *)req_body)->qp_handle] = NULL;

            }
            break;

            case IBV_REG_MR:
            {
                LOG_DEBUG("REG_MR");

                //req_body = malloc(sizeof(struct IBV_REG_MR_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_REG_MR_REQ)) < sizeof(struct IBV_REG_MR_REQ))
                {
                    LOG_ERROR("REG_MR: Failed to read request body.");
                    goto kill;
                }

                // create a shm buffer
                LOG_TRACE("Create a shared memory piece for client " << header.client_id << " with size " << ((struct IBV_REG_MR_REQ*)req_body)->mem_size);
                if (((struct IBV_REG_MR_REQ*)req_body)->shm_name[0] == '\0')
                {
                    LOG_TRACE("create shm from client id and count.");
                    sp = ffr->addShmPiece(header.client_id, ((struct IBV_REG_MR_REQ*)req_body)->mem_size);
                }
                else
                {
                    LOG_TRACE("create shm from name: " << ((struct IBV_REG_MR_REQ*)req_body)->shm_name);
                    sp = ffr->addShmPiece(((struct IBV_REG_MR_REQ*)req_body)->shm_name, ((struct IBV_REG_MR_REQ*)req_body)->mem_size);
                }
                
                if (sp == NULL)
                {
                    LOG_ERROR("Failed to the shared memory piece.");
                    goto end;
                }

                LOG_TRACE("Looking for PD with pd_handle " << ((struct IBV_REG_MR_REQ *)req_body)->pd_handle);
                pd = ffr->pd_map[((struct IBV_REG_MR_REQ *)req_body)->pd_handle];
                if (pd == NULL)
                {
                    LOG_ERROR("Failed to get pd with pd_handle " << ((struct IBV_REG_MR_REQ *)req_body)->pd_handle);
                    goto end;
                }

                LOG_DEBUG("Registering a MR ptr="<< sp->ptr << ", size="  << sp->size);            
                mr = ibv_reg_mr(pd, sp->ptr, sp->size, ((struct IBV_REG_MR_REQ *)req_body)->access_flags);
                if (mr == NULL)
                {
                    LOG_ERROR("Failed to regiester the MR. Current shared memory size: " << sp->size);
                    goto end;
                }

                if (mr->handle >= MAP_SIZE)
                {
                    LOG_ERROR("[Warning] MR handle (" << mr->handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    ffr->shmr_map[mr->handle] = sp;
                    ffr->mr_map[mr->handle] = mr;
                }
 
                //rsp = malloc(sizeof(struct IBV_REG_MR_RSP));
                size = sizeof(struct IBV_REG_MR_RSP);
                ((struct IBV_REG_MR_RSP *)rsp)->handle = mr->handle;
                ((struct IBV_REG_MR_RSP *)rsp)->lkey = mr->lkey;
                ((struct IBV_REG_MR_RSP *)rsp)->rkey = mr->rkey;
                strcpy(((struct IBV_REG_MR_RSP *)rsp)->shm_name, sp->name.c_str());
            
                LOG_TRACE("mr->handle=" << mr->handle);
                LOG_TRACE("mr->lkey=" << mr->lkey);
                LOG_TRACE("mr->rkey=" << mr->rkey);
                LOG_TRACE("shm_name=" << sp->name.c_str());

                // store lkey to ptr mapping
                pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                ffr->lkey_ptr[mr->lkey] = sp->ptr;
                pthread_mutex_unlock(&ffr->lkey_ptr_mtx);
            }
            break;

            case IBV_REG_MR_MAPPING:
            {
                LOG_DEBUG("REG_MR_MAPPING");
                //req_body = malloc(sizeof(struct IBV_REG_MR_MAPPING_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_REG_MR_MAPPING_REQ)) < sizeof(struct IBV_REG_MR_MAPPING_REQ))
                {
                    LOG_ERROR("REG_MR_MAPPING: Failed to read request body.");
                    goto kill;
                }

                struct IBV_REG_MR_MAPPING_REQ *p = (struct IBV_REG_MR_MAPPING_REQ*)req_body;
                
                pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                p->shm_ptr = (char*)(ffr->lkey_ptr[p->key]);
                pthread_mutex_unlock(&ffr->lkey_ptr_mtx);

                struct sockaddr_in si_other, si_self;
                struct sockaddr src_addr;
                socklen_t addrlen;
                char recv_buff[1400];
                ssize_t recv_buff_size;

                int s, i, slen=sizeof(si_other);

                srand (client_sock);

                for (int i = 0; i < HOST_NUM; i++) {

                if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
                    LOG_ERROR("Error in creating socket for UDP client");
                    return;
                }

                memset((char *) &si_other, 0, sizeof(si_other));
                si_other.sin_family = AF_INET;
                si_other.sin_port = htons(UDP_PORT);


                memset((char *) &si_self, 0, sizeof(si_self));
                si_self.sin_family = AF_INET;
                int self_p = 0;//2000 + rand() % 40000;
                si_self.sin_port = htons(self_p);

                    if (inet_aton("0.0.0.0", &si_self.sin_addr)==0) {
                        LOG_ERROR("Error in creating socket for UDP client self.");
                        continue;
                    }

                    if (bind(s, (const struct sockaddr *)&si_self, sizeof(si_self)) < 0)
                    {
                        LOG_ERROR("Failed to bind UDP. errno=" << errno);
                        continue;
                    }

                    if (inet_aton(HOST_LIST[i], &si_other.sin_addr)==0) {
                        LOG_ERROR("Error in creating socket for UDP client other.");
                        continue;
                    }

                    if (sendto(s, req_body, sizeof(struct IBV_REG_MR_MAPPING_REQ), 0, (const sockaddr*)&si_other, slen)==-1) {
                        LOG_DEBUG("Error in sending MR mapping to " << HOST_LIST[i]);
                    }
                    else {
                        LOG_TRACE("Sent MR mapping to " << HOST_LIST[i]);
                    }

                    if ((recv_buff_size = recvfrom(s, recv_buff, 1400, 0, (sockaddr*)&si_other, (socklen_t*)&slen)) == -1)
                    {
                        LOG_ERROR("Error in receiving MR mapping ack" << HOST_LIST[i]);
                    }
                    else
                    {

                        char src_str[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET,
                            &si_other.sin_addr,
                            src_str,
                            sizeof src_str);

                        int src_port = ntohs(si_other.sin_port);
                        LOG_INFO("## ACK from " << HOST_LIST[i] << "/" << src_str << ":" << src_port << "ack-rkey=" << recv_buff <<  " rkey= " << p->key);
                    }

                    close(s);
                }                


                size = sizeof(struct IBV_REG_MR_MAPPING_RSP);
                ((struct IBV_REG_MR_MAPPING_RSP *)rsp)->ret = 0;
            }
            break;

            case IBV_DEREG_MR:
            {
                LOG_DEBUG("DEREG_MR");

                //req_body = malloc(sizeof(struct IBV_DEREG_MR_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_DEREG_MR_REQ)) < sizeof(struct IBV_DEREG_MR_REQ))
                {
                    LOG_ERROR("DEREG_MR: Failed to read request body.");
                    goto kill;
                }

                sp = ffr->shmr_map[((struct IBV_DEREG_MR_REQ *)req_body)->handle];
                mr = ffr->mr_map[((struct IBV_DEREG_MR_REQ *)req_body)->handle];

                ffr->shmr_map[((struct IBV_DEREG_MR_REQ *)req_body)->handle] = NULL;
                ffr->mr_map[((struct IBV_DEREG_MR_REQ *)req_body)->handle] = NULL;
                ret = ibv_dereg_mr(mr);
                sp->remove();
 
                //rsp = malloc(sizeof(struct IBV_DEREG_MR_RSP));
                size = sizeof(struct IBV_DEREG_MR_RSP);
                ((struct IBV_DEREG_MR_RSP *)rsp)->ret = ret;
            }
            break;

            case IBV_MODIFY_QP:
            {
                LOG_TRACE("MODIFY_QP");

                //req_body = malloc(sizeof(struct IBV_MODIFY_QP_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_MODIFY_QP_REQ)) < sizeof(struct IBV_MODIFY_QP_REQ))
                {
                    LOG_ERROR("MODIFY_QP: Failed to read request body.");
                    goto kill;
                }

                LOG_TRACE("QP handle to modify: " << ((struct IBV_MODIFY_QP_REQ *)req_body)->handle);
            
                if (((struct IBV_MODIFY_QP_REQ *)req_body)->handle >= MAP_SIZE)
                {
                    LOG_ERROR("QP handle (" << qp->handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    qp = ffr->qp_map[((struct IBV_MODIFY_QP_REQ *)req_body)->handle];
                }

                int ret = 0;
                struct ibv_qp_attr *init_attr = &((struct IBV_MODIFY_QP_REQ *)req_body)->attr;

                /*if (init_attr->qp_state == IBV_QPS_RTR && !ffr->ibv_gid_init && init_attr->ah_attr.grh.dgid.global.subnet_prefix)
                {
                    memcpy(&ffr->gid, &init_attr->ah_attr.grh.dgid, sizeof(union ibv_gid));
                    ffr->ibv_gid_init = 1;
                }
           
                if (init_attr->qp_state == IBV_QPS_RTR && ffr->ibv_gid_init && !init_attr->ah_attr.grh.dgid.global.subnet_prefix)
                {
                    memcpy(&init_attr->ah_attr.grh.dgid, &ffr->gid, sizeof(union ibv_gid));
                    init_attr->ah_attr.grh.hop_limit = 1; 
                }*/

                if ((ret = ibv_modify_qp(qp, &((struct IBV_MODIFY_QP_REQ *)req_body)->attr, ((struct IBV_MODIFY_QP_REQ *)req_body)->attr_mask)) != 0)
                {
                    LOG_ERROR("Modify QP (" << qp->handle << ") fails. ret = " << ret << "errno = " << errno);      
                }

                LOG_DEBUG("---------- QP=" << ((struct IBV_MODIFY_QP_REQ *)req_body)->handle << " -----------");      
                LOG_DEBUG("attr.qp_state=" << init_attr->qp_state); 
                LOG_DEBUG("attr.cur_qp_state=" << init_attr->cur_qp_state); 
                LOG_DEBUG("attr.path_mtu=" << init_attr->path_mtu); 
                LOG_DEBUG("attr.path_mig_state=" << init_attr->path_mig_state); 
                LOG_DEBUG("attr.qkey=" << init_attr->qkey); 
                LOG_DEBUG("attr.rq_psn=" << init_attr->rq_psn); 
                LOG_DEBUG("attr.sq_psn=" << init_attr->sq_psn); 
                LOG_DEBUG("attr.dest_qp_num=" << init_attr->dest_qp_num); 
                LOG_DEBUG("attr.qp_access_flags=" << init_attr->qp_access_flags); 
                LOG_DEBUG("attr.cap.max_send_wr=" << init_attr->cap.max_send_wr); 
                LOG_DEBUG("attr.cap.max_recv_wr=" << init_attr->cap.max_recv_wr); 
                LOG_DEBUG("attr.cap.max_send_sge=" << init_attr->cap.max_send_sge); 
                LOG_DEBUG("attr.cap.max_recv_sge=" << init_attr->cap.max_recv_sge); 
                LOG_DEBUG("attr.cap.max_inline_data=" << init_attr->cap.max_inline_data); 
                LOG_DEBUG("attr.ah_attr.global.subnet_prefix=" << init_attr->ah_attr.grh.dgid.global.subnet_prefix); 
                LOG_DEBUG("attr.ah_attr.global.interface_id=" << init_attr->ah_attr.grh.dgid.global.interface_id); 
                LOG_DEBUG("attr.ah_attr.flow_label=" << init_attr->ah_attr.grh.flow_label); 
                LOG_DEBUG("attr.ah_attr.sgid_index=" << (int)init_attr->ah_attr.grh.sgid_index); 
                LOG_DEBUG("attr.ah_attr.hop_limit=" << (int)init_attr->ah_attr.grh.hop_limit); 
                LOG_DEBUG("attr.ah_attr.traffic_class=" << (int)init_attr->ah_attr.grh.traffic_class); 
 
                //rsp = malloc(sizeof(struct IBV_MODIFY_QP_RSP));
                size = sizeof(struct IBV_MODIFY_QP_RSP);
                ((struct IBV_MODIFY_QP_RSP *)rsp)->ret = ret;
                ((struct IBV_MODIFY_QP_RSP *)rsp)->handle = ((struct IBV_MODIFY_QP_REQ *)req_body)->handle;
            }
            break;

            case IBV_QUERY_QP:
            {
                if (read(client_sock, req_body, sizeof(struct IBV_QUERY_QP_REQ)) < sizeof(struct IBV_QUERY_QP_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_INFO("QUERY_QP client_id=" << client_sock << " cmd_fd=" << ffr->rdma_data.ib_context->cmd_fd);

                //rsp = malloc(sizeof(struct IBV_QUERY_DEV_RSP));
                size = sizeof(struct IBV_QUERY_QP_RSP);

                ((struct IBV_QUERY_QP_RSP *)rsp)->ret_errno = ibv_cmd_query_qp_resp(
                    ffr->rdma_data.ib_context->cmd_fd,
                    ((IBV_QUERY_QP_REQ*)req_body)->cmd,
                    ((IBV_QUERY_QP_REQ*)req_body)->cmd_size,
                    &((IBV_QUERY_QP_RSP*)rsp)->resp);
                
                size = sizeof(struct IBV_QUERY_QP_RSP);
        if (((struct IBV_QUERY_QP_RSP *)rsp)->ret_errno != 0)
            LOG_ERROR("Return error (" << ((struct IBV_QUERY_QP_RSP *)rsp)->ret_errno  << ") in QUERY_QP");
            }
            break;

            case IBV_POST_SEND: 
            {
                LOG_INFO("POST_SEND");
                //req_body = malloc(header.body_size);
                if (read(client_sock, req_body, header.body_size) < header.body_size)
                {
                    LOG_ERROR("POST_SEND: Error in reading in post send.");
                    goto end;
                }

                // Now recover the qp and wr
                struct ibv_post_send *post_send = (struct ibv_post_send*)req_body;
                if (post_send->qp_handle >= MAP_SIZE)
                {
                    LOG_ERROR("[Warning] QP handle (" << post_send->qp_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    qp = ffr->qp_map[post_send->qp_handle];
                    tb = ffr->tokenbucket[post_send->qp_handle];
                }

                struct ibv_send_wr *wr = (struct ibv_send_wr*)((char*)req_body + sizeof(struct ibv_post_send));
                struct ibv_sge *sge = (struct ibv_sge*)((char*)req_body + sizeof(struct ibv_post_send) + post_send->wr_count * sizeof(struct ibv_send_wr));

                uint32_t *ah = NULL; 
                if (qp->qp_type == IBV_QPT_UD) {
                    LOG_INFO("POST_SEND_UD!!!");
                    ah = (uint32_t*)(sge + post_send->sge_count);
                }

                uint32_t wr_success = 0;
                for (int i = 0; i < post_send->wr_count; i++)
                {
                    LOG_INFO("wr[i].wr_id=" << wr[i].wr_id << " opcode=" << wr[i].opcode <<  " imm_data==" << wr[i].imm_data);

                    if (wr[i].opcode == IBV_WR_RDMA_WRITE || wr[i].opcode == IBV_WR_RDMA_WRITE_WITH_IMM || wr[i].opcode == IBV_WR_RDMA_READ) {
                        if (ffr->rkey_mr_shm.find(wr[i].wr.rdma.rkey) == ffr->rkey_mr_shm.end()) {
                            LOG_ERROR("One sided opertaion: can't find remote MR. rkey --> " << wr[i].wr.rdma.rkey << "  addr --> " << wr[i].wr.rdma.remote_addr);
                        }
                        else {
                            LOG_DEBUG("shm:" << (uint64_t)(ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].shm_ptr) << " app:" << (uint64_t)(wr[i].wr.rdma.remote_addr) << " mr:" << (uint64_t)(ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].mr_ptr));
                            wr[i].wr.rdma.remote_addr = (uint64_t)(ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].shm_ptr) + (uint64_t)wr[i].wr.rdma.remote_addr - (uint64_t)ffr->rkey_mr_shm[wr[i].wr.rdma.rkey].mr_ptr;
                        }
                    }

                    // fix the link list pointer
                    if (i >= post_send->wr_count - 1) {
                        wr[i].next = NULL;
                    }
                    else {
                        wr[i].next = &(wr[i+1]);
                    }
                    if (wr[i].num_sge > 0) {
                        // fix the sg list pointer
                        wr[i].sg_list = sge;
                        pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                        for (int j = 0; j < wr[i].num_sge; j++) {
                            while (!(tb->consume(sge[j].length))) {
                                uint32_t stime = sge[j].length * 1000000 / MAX_QP_RATE_LIMIT;
                                if (stime) {
                                    usleep(stime);
                                }
                                else {
                                    usleep(1);
                                }
                                //wr[i-1].next = NULL;
                                //break;
                            }
                            LOG_DEBUG("wr[i].wr_id=" << wr[i].wr_id << " qp_num=" << qp->qp_num << " sge.addr=" << sge[j].addr << " sge.length" << sge[j].length << " opcode=" << wr[i].opcode);
                            sge[j].addr = (uint64_t)((char*)(ffr->lkey_ptr[sge[j].lkey]) + sge[j].addr);
                            LOG_DEBUG("data=" << ((char*)(sge[j].addr))[0] << ((char*)(sge[j].addr))[1] << ((char*)(sge[j].addr))[2]);
                            LOG_DEBUG("imm_data==" << wr[i].imm_data);

                        }
                        pthread_mutex_unlock(&ffr->lkey_ptr_mtx);

                        sge += wr[i].num_sge;
                    }

                    else {
                        wr[i].sg_list = NULL;
                    }

                    // fix ah
                    if (qp->qp_type == IBV_QPT_UD) {
                        wr[i].wr.ud.ah = ffr->ah_map[*ah];
                        ah = ah + 1;
                    }

                    wr_success++;
                }

                struct ibv_send_wr *bad_wr = NULL;
                //rsp = malloc(sizeof(struct IBV_POST_SEND_RSP));
                size = sizeof(struct IBV_POST_SEND_RSP);

                ((struct IBV_POST_SEND_RSP*)rsp)->ret_errno = ibv_post_send(qp, wr, &bad_wr);
                if (((struct IBV_POST_SEND_RSP*)rsp)->ret_errno != 0) {
                    LOG_ERROR("[Error] Post send (" << qp->handle << ") fails.");
                }

                LOG_DEBUG("post_send success.");

                if (bad_wr == NULL) {
                    // this IF is not needed right now, but left here for future use
                    if (post_send->wr_count == wr_success) {
                        ((struct IBV_POST_SEND_RSP*)rsp)->bad_wr = 0;
                    }
                    else {
                        ((struct IBV_POST_SEND_RSP*)rsp)->bad_wr = post_send->wr_count - wr_success;
                        ((struct IBV_POST_SEND_RSP*)rsp)->ret_errno = ENOMEM;
                    }
                }
                else{
                    LOG_ERROR("bad_wr is not NULL.");
                    ((struct IBV_POST_SEND_RSP*)rsp)->bad_wr = bad_wr - wr;
                }
            }
            break;

            case IBV_POST_RECV: 
            {
                LOG_TRACE("IBV_POST_RECV");
                //req_body = malloc(header.body_size);
                if (read(client_sock, req_body, header.body_size) < header.body_size)
                {
                    LOG_ERROR("POST_RECV: Error in reading in post recv.");
                    goto end;
                }

                // Now recover the qp and wr
                struct ibv_post_recv *post_recv = (struct ibv_post_recv*)req_body;
                if (post_recv->qp_handle >= MAP_SIZE)
                {
                    LOG_ERROR("[Warning] QP handle (" << post_recv->qp_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    qp = ffr->qp_map[post_recv->qp_handle];
                }

                struct ibv_recv_wr *wr = (struct ibv_recv_wr*)((char*)req_body + sizeof(struct ibv_post_recv));
                struct ibv_sge *sge = (struct ibv_sge*)((char*)req_body + sizeof(struct ibv_post_recv) + post_recv->wr_count * sizeof(struct ibv_recv_wr));

                for (int i = 0; i < post_recv->wr_count; i++)
                {
                    // fix the link list pointer
                    if (i >= post_recv->wr_count - 1) {
                        wr[i].next = NULL;
                    }
                    else {
                        wr[i].next = &(wr[i+1]);
                    }
                    if (wr[i].num_sge > 0) {
                        // fix the sg list pointer
                        wr[i].sg_list = sge;
                        pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                        for (int j = 0; j < wr[i].num_sge; j++) {
                            sge[j].addr = (uint64_t)(ffr->lkey_ptr[sge[j].lkey]) + (uint64_t)(sge[j].addr);
                        }
                        pthread_mutex_unlock(&ffr->lkey_ptr_mtx);
                        sge += wr[i].num_sge;
                    }
            else {
            wr[i].sg_list = NULL;	
            }
                }

                struct ibv_recv_wr *bad_wr = NULL;
                //rsp = malloc(sizeof(struct IBV_POST_RECV_RSP));
                size = sizeof(struct IBV_POST_RECV_RSP);
                ((struct IBV_POST_RECV_RSP*)rsp)->ret_errno = ibv_post_recv(qp, wr, &bad_wr);
                if (((struct IBV_POST_RECV_RSP*)rsp)->ret_errno != 0) {
                    LOG_ERROR("[Error] Post recv (" << qp->handle << ") fails.");
                }
                if (bad_wr == NULL) 
                {
                    ((struct IBV_POST_RECV_RSP*)rsp)->bad_wr = 0;
                }
                else
                {
                    ((struct IBV_POST_RECV_RSP*)rsp)->bad_wr = bad_wr - wr;
                }
            }
            break;

            case IBV_POLL_CQ:
            {
                LOG_TRACE("IBV_POLL_CQ");

                //req_body = malloc(sizeof(struct IBV_POLL_CQ_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_POLL_CQ_REQ)) < sizeof(struct IBV_POLL_CQ_REQ))
                {
                    LOG_ERROR("POLL_CQ: Failed to read request body.");
                    goto kill;
                }

                LOG_TRACE("CQ handle to poll: " << ((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle);
            
                if (((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle >= MAP_SIZE)
                {
                    LOG_ERROR("CQ handle (" << ((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    cq = ffr->cq_map[((struct IBV_POLL_CQ_REQ *)req_body)->cq_handle];
                }

                if (cq == NULL)
                {
                    LOG_ERROR("cq pointer is NULL.");
                    goto end;
                }

                //rsp = malloc(sizeof(struct FfrResponseHeader) + ((struct IBV_POLL_CQ_REQ *)req_body)->ne * sizeof(struct ibv_wc));
                wc_list = (struct ibv_wc*)((char *)rsp + sizeof(struct FfrResponseHeader));
            
                count = ibv_poll_cq(cq, ((struct IBV_POLL_CQ_REQ *)req_body)->ne, wc_list);

                if (count <= 0)
                {
                    LOG_TRACE("The return of ibv_poll_cq is " << count);
                    size = sizeof(struct FfrResponseHeader);
                    ((struct FfrResponseHeader*)rsp)->rsp_size = 0;    
                }
                else
                {
                    size = sizeof(struct FfrResponseHeader) + count * sizeof(struct ibv_wc);
                    ((struct FfrResponseHeader*)rsp)->rsp_size = count * sizeof(struct ibv_wc);
                }

                for (i = 0; i < count; i++)
                {
                    if (wc_list[i].status == 0)
                    {
                        LOG_DEBUG("======== wc =========");
                        LOG_DEBUG("wr_id=" << wc_list[i].wr_id); 
                        LOG_DEBUG("status=" << wc_list[i].status); 
                        LOG_DEBUG("opcode=" << wc_list[i].opcode); 
                        LOG_DEBUG("vendor_err=" << wc_list[i].vendor_err); 
                        LOG_DEBUG("byte_len=" << wc_list[i].byte_len); 
                        LOG_DEBUG("imm_data=" << wc_list[i].imm_data); 
                        LOG_DEBUG("qp_num=" << wc_list[i].qp_num); 
                        LOG_DEBUG("src_qp=" << wc_list[i].src_qp); 
                        LOG_DEBUG("wc_flags=" << wc_list[i].wc_flags); 
                        LOG_DEBUG("pkey_index=" << wc_list[i].pkey_index); 
                        LOG_DEBUG("slid=" << wc_list[i].slid); 
                        LOG_DEBUG("sl=" << wc_list[i].sl); 
                        LOG_DEBUG("dlid_path_bits=" << wc_list[i].dlid_path_bits);                         
                    }
                    else
                    {
                        LOG_DEBUG("======== wc =========");
                        LOG_DEBUG("wr_id=" << wc_list[i].wr_id); 
                        LOG_DEBUG("status=" << wc_list[i].status); 
                        LOG_DEBUG("opcode=" << wc_list[i].opcode); 
                        LOG_DEBUG("vendor_err=" << wc_list[i].vendor_err); 
                        LOG_DEBUG("byte_len=" << wc_list[i].byte_len); 
                        LOG_DEBUG("imm_data=" << wc_list[i].imm_data); 
                        LOG_DEBUG("qp_num=" << wc_list[i].qp_num); 
                        LOG_DEBUG("src_qp=" << wc_list[i].src_qp); 
                        LOG_DEBUG("wc_flags=" << wc_list[i].wc_flags); 
                        LOG_DEBUG("pkey_index=" << wc_list[i].pkey_index); 
                        LOG_DEBUG("slid=" << wc_list[i].slid); 
                        LOG_DEBUG("sl=" << wc_list[i].sl); 
                        LOG_DEBUG("dlid_path_bits=" << wc_list[i].dlid_path_bits);                         
                    }
                }
            
                break;
            }
                
            case IBV_CREATE_COMP_CHANNEL:
            {
                LOG_DEBUG("IBV_CREATE_COMP_CHANNEL");

                //rsp = malloc(sizeof(struct IBV_CREATE_COMP_CHANNEL_RSP));
                size = sizeof(struct IBV_CREATE_COMP_CHANNEL_RSP);
                channel = ibv_create_comp_channel(ffr->rdma_data.ib_context); 

                if (channel->fd >= MAP_SIZE)
                {
                    LOG_INFO("channel fd is no less than MAX_QUEUE_MAP_SIZE. fd=" << channel->fd); 
                }
                else
                {
                    ffr->channel_map[channel->fd] = channel;
                }

                if (send_fd(client_sock, channel->fd) < 0)
                {
                    LOG_ERROR("failed to send_fd in create_comp_channel.");
                }

                ((struct IBV_CREATE_COMP_CHANNEL_RSP *)rsp)->fd = channel->fd;
                LOG_INFO("Return channel fd " << channel->fd << "for client_id " << header.client_id);     
            }
            break;

            case IBV_DESTROY_COMP_CHANNEL:
            {
                LOG_DEBUG("IBV_DESTROY_COMP_CHANNEL");

                //req_body = malloc(sizeof(struct IBV_DESTROY_COMP_CHANNEL_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_DESTROY_COMP_CHANNEL_REQ)) < sizeof(struct IBV_DESTROY_COMP_CHANNEL_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("Destroy Channel: " << ((IBV_DESTROY_COMP_CHANNEL_REQ*)req_body)->fd); 

                channel = ffr->channel_map[((struct IBV_DESTROY_COMP_CHANNEL_REQ *)req_body)->fd];
                if (channel == NULL)
                {
                    LOG_ERROR("Failed to get channel with fd " << ((struct IBV_DESTROY_COMP_CHANNEL_REQ *)req_body)->fd);
                    goto end;
                }

                ffr->channel_map[((struct IBV_DESTROY_COMP_CHANNEL_REQ *)req_body)->fd] = NULL;
                ret = ibv_destroy_comp_channel(channel);
                //rsp = malloc(sizeof(struct IBV_DESTROY_COMP_CHANNEL_RSP));
                ((struct IBV_DESTROY_COMP_CHANNEL_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_DESTROY_COMP_CHANNEL_RSP);
            }
            break;

            case IBV_GET_CQ_EVENT:
            {
                //LOG_DEBUG("IBV_GET_CQ_EVENT");

                //req_body = malloc(sizeof(struct IBV_GET_CQ_EVENT_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_GET_CQ_EVENT_REQ)) < sizeof(struct IBV_GET_CQ_EVENT_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("GET CQ Event from channel fd: " << ((IBV_GET_CQ_EVENT_REQ*)req_body)->fd); 

                channel = ffr->channel_map[((struct IBV_GET_CQ_EVENT_REQ *)req_body)->fd];
                if (channel == NULL)
                {
                    LOG_ERROR("Failed to get channel with fd " << ((struct IBV_GET_CQ_EVENT_REQ *)req_body)->fd);
                    goto end;
                }

                ibv_get_cq_event(channel, &cq, &context);

                if (cq == NULL)
                {
                    LOG_ERROR("NULL CQ from ibv_get_cq_event ");
                    goto end;    
                }

                //rsp = malloc(sizeof(struct IBV_GET_CQ_EVENT_RSP));
                ((struct IBV_GET_CQ_EVENT_RSP *)rsp)->cq_handle = cq->handle;
                ((struct IBV_GET_CQ_EVENT_RSP *)rsp)->comp_events_completed = cq->comp_events_completed;
                ((struct IBV_GET_CQ_EVENT_RSP *)rsp)->async_events_completed = cq->async_events_completed;
                
                size = sizeof(struct IBV_GET_CQ_EVENT_RSP);
            }
            break;

            case IBV_ACK_CQ_EVENT:
            {
                //LOG_DEBUG("IBV_ACK_CQ_EVENT");

                //req_body = malloc(sizeof(struct IBV_ACK_CQ_EVENT_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_ACK_CQ_EVENT_REQ)) < sizeof(struct IBV_ACK_CQ_EVENT_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("GET CQ Event from cq_handle: " << ((IBV_ACK_CQ_EVENT_REQ*)req_body)->cq_handle); 

                cq = ffr->cq_map[((struct IBV_ACK_CQ_EVENT_REQ *)req_body)->cq_handle];
                if (cq == NULL)
                {
                    LOG_ERROR("Failed to get cq with cq_handle " << ((struct IBV_ACK_CQ_EVENT_REQ *)req_body)->cq_handle);
                    goto end;
                }

                ibv_ack_cq_events(cq, ((struct IBV_ACK_CQ_EVENT_REQ *)req_body)->nevents);

                //rsp = malloc(sizeof(struct IBV_GET_CQ_EVENT_RSP));
                ((struct IBV_GET_CQ_EVENT_RSP *)rsp)->cq_handle = cq->handle;
                ((struct IBV_GET_CQ_EVENT_RSP *)rsp)->comp_events_completed = cq->comp_events_completed;
                ((struct IBV_GET_CQ_EVENT_RSP *)rsp)->async_events_completed = cq->async_events_completed;
                
                size = sizeof(struct IBV_GET_CQ_EVENT_RSP);    
            }
            break;

            case IBV_CREATE_AH:
            {
                LOG_DEBUG("IBV_CREATE_AH");

                //req_body = malloc(sizeof(struct IBV_CREATE_AH_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_CREATE_AH_REQ)) < sizeof(struct IBV_CREATE_AH_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("Looking for PD with pd_handle " << ((struct IBV_CREATE_AH_REQ *)req_body)->pd_handle);
                pd = ffr->pd_map[((struct IBV_CREATE_AH_REQ *)req_body)->pd_handle];
                if (pd == NULL)
                {
                    LOG_ERROR("Failed to get pd with pd_handle " << ((struct IBV_CREATE_AH_REQ *)req_body)->pd_handle);
                    goto end;
                }

                ah = ibv_create_ah(pd, &(((struct IBV_CREATE_AH_REQ *)req_body)->ah_attr));
                if (ah->handle >= MAP_SIZE)
                {
                    LOG_INFO("AH handle is no less than MAX_QUEUE_MAP_SIZE. ah_handle=" << ah->handle); 
                }
                else
                {
                    ffr->ah_map[ah->handle] = ah;
                }

                //rsp = malloc(sizeof(struct IBV_CREATE_AH_RSP));
                ((struct IBV_CREATE_AH_RSP *)rsp)->ah_handle = ah->handle;
                ((struct IBV_CREATE_AH_RSP *)rsp)->ret = 0;
                size = sizeof(struct IBV_CREATE_AH_RSP);
            }
            break;

            case IBV_DESTROY_AH:
            {
                LOG_DEBUG("IBV_DESTROY_AH");

                if (read(client_sock, req_body, sizeof(struct IBV_DESTROY_AH_REQ)) < sizeof(struct IBV_DESTROY_AH_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("Looking for AH with ah_handle " << ((struct IBV_DESTROY_AH_REQ *)req_body)->ah_handle);
                ah = ffr->ah_map[((struct IBV_CREATE_AH_REQ *)req_body)->pd_handle];
                if (ah == NULL)
                {
                    LOG_ERROR("Failed to get ah with ah_handle " << ((struct IBV_DESTROY_AH_REQ *)req_body)->ah_handle);
                    goto end;
                }

                ret = ibv_destroy_ah(ah);

                ((struct IBV_DESTROY_AH_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_DESTROY_AH_RSP);
            }
            break;

            case IBV_CREATE_FLOW:
            {
                if (read(client_sock, req_body, sizeof(struct IBV_CREATE_FLOW_REQ)) < sizeof(struct IBV_CREATE_FLOW_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_INFO("CREATE_FLOW client_id=" << client_sock << " cmd_fd=" << ffr->rdma_data.ib_context->cmd_fd);

                size = sizeof(struct IBV_CREATE_FLOW_RSP);

                ((struct IBV_CREATE_FLOW_RSP *)rsp)->ret_errno = ibv_cmd_create_flow_resp(
                    ffr->rdma_data.ib_context->cmd_fd,
                    ((IBV_CREATE_FLOW_REQ*)req_body)->cmd,
                    ((IBV_CREATE_FLOW_REQ*)req_body)->written_size,
                    ((IBV_CREATE_FLOW_REQ*)req_body)->exp_flow,
                    &((IBV_CREATE_FLOW_RSP*)rsp)->resp);
                
                size = sizeof(struct IBV_CREATE_FLOW_RSP);
        if (((struct IBV_CREATE_FLOW_RSP *)rsp)->ret_errno != 0)
            LOG_ERROR("Return error (" << ((struct IBV_CREATE_FLOW_RSP *)rsp)->ret_errno  << ") in CREATE_FLOW");
            }
            break;

            case IBV_DESTROY_FLOW:
            {
                if (read(client_sock, req_body, sizeof(struct IBV_DESTROY_FLOW_REQ)) < sizeof(struct IBV_DESTROY_FLOW_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_INFO("DESTROY_FLOW client_id=" << client_sock << " cmd_fd=" << ffr->rdma_data.ib_context->cmd_fd);

                size = sizeof(struct IBV_DESTROY_FLOW_RSP);

                ((struct IBV_DESTROY_FLOW_RSP *)rsp)->ret_errno = ibv_cmd_destroy_flow_resp(
                    ffr->rdma_data.ib_context->cmd_fd,
                    &((IBV_DESTROY_FLOW_REQ*)req_body)->cmd);
                
                size = sizeof(struct IBV_DESTROY_FLOW_RSP);
        if (((struct IBV_DESTROY_FLOW_RSP *)rsp)->ret_errno != 0)
            LOG_ERROR("Return error (" << ((struct IBV_DESTROY_FLOW_RSP *)rsp)->ret_errno  << ") in DESTROY_FLOW");
            }
            break;

            
        case IBV_CREATE_SRQ:
            {
                LOG_DEBUG("IBV_CREATE_SRQ");

                //req_body = malloc(sizeof(struct IBV_CREATE_SRQ_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_CREATE_SRQ_REQ)) < sizeof(struct IBV_CREATE_SRQ_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("Looking for PD with pd_handle " << ((struct IBV_CREATE_SRQ_REQ *)req_body)->pd_handle);
                pd = ffr->pd_map[((struct IBV_CREATE_SRQ_REQ *)req_body)->pd_handle];
                if (pd == NULL)
                {
                    LOG_ERROR("Failed to get pd with pd_handle " << ((struct IBV_CREATE_SRQ_REQ *)req_body)->pd_handle);
                    goto end;
                }

                srq = ibv_create_srq(pd, &(((struct IBV_CREATE_SRQ_REQ *)req_body)->attr));
                if (srq->handle >= MAP_SIZE)
                {
                    LOG_INFO("SRQ handle is no less than MAX_QUEUE_MAP_SIZE. srq_handle=" << srq->handle); 
                }
                else
                {
                    ffr->srq_map[srq->handle] = srq;
                }

                //rsp = malloc(sizeof(struct IBV_CREATE_SRQ_RSP));
                ((struct IBV_CREATE_SRQ_RSP *)rsp)->srq_handle = srq->handle;
                size = sizeof(struct IBV_CREATE_SRQ_RSP);
                
                std::stringstream ss;
                ss << "srq" << srq->handle;
                ShmPiece* sp = ffr->initCtrlShm(ss.str().c_str());
                ffr->srq_shm_map[srq->handle] = sp;
                strcpy(((struct IBV_CREATE_SRQ_RSP *)rsp)->shm_name, sp->name.c_str());
                pthread_mutex_lock(&ffr->srq_shm_vec_mtx);
                ffr->srq_shm_vec.push_back(srq->handle);
                pthread_mutex_unlock(&ffr->srq_shm_vec_mtx);
            }
            break;

            case IBV_MODIFY_SRQ:
            {
                LOG_INFO("IBV_MODIFY_SRQ");

                //req_body = malloc(sizeof(struct IBV_MODIFY_SRQ_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_MODIFY_SRQ_REQ)) < sizeof(struct IBV_MODIFY_SRQ_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("Looking for SRQ with srq_handle " << ((struct IBV_MODIFY_SRQ_REQ *)req_body)->srq_handle);
                srq = ffr->srq_map[((struct IBV_MODIFY_SRQ_REQ *)req_body)->srq_handle];
                if (srq == NULL)
                {
                    LOG_ERROR("Failed to get srq with srq_handle " << ((struct IBV_MODIFY_SRQ_REQ *)req_body)->srq_handle);
                    goto end;
                }

                ret = ibv_modify_srq(srq, &(((struct IBV_MODIFY_SRQ_REQ *)req_body)->attr), ((struct IBV_MODIFY_SRQ_REQ *)req_body)->srq_attr_mask);

                //rsp = malloc(sizeof(struct IBV_MODIFY_SRQ_RSP));
                ((struct IBV_MODIFY_SRQ_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_MODIFY_SRQ_RSP);
            }
            break;

            case IBV_DESTROY_SRQ:
            {
                LOG_INFO("IBV_DESTROY_SRQ");

                //req_body = malloc(sizeof(struct IBV_DESTROY_SRQ_REQ));
                if (read(client_sock, req_body, sizeof(struct IBV_DESTROY_SRQ_REQ)) < sizeof(struct IBV_DESTROY_SRQ_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_TRACE("Looking for SRQ with srq_handle " << ((struct IBV_DESTROY_SRQ_REQ *)req_body)->srq_handle);
                srq = ffr->srq_map[((struct IBV_DESTROY_SRQ_REQ *)req_body)->srq_handle];
                if (srq == NULL)
                {
                    LOG_ERROR("Failed to get srq with srq_handle " << ((struct IBV_DESTROY_SRQ_REQ *)req_body)->srq_handle);
                    goto end;
                }

                ffr->srq_map[((struct IBV_DESTROY_SRQ_REQ *)req_body)->srq_handle] = NULL;
                ret = ibv_destroy_srq(srq);

                //rsp = malloc(sizeof(struct IBV_DESTROY_SRQ_RSP));
                ((struct IBV_DESTROY_SRQ_RSP *)rsp)->ret = ret;
                size = sizeof(struct IBV_DESTROY_SRQ_RSP);

                pthread_mutex_lock(&ffr->srq_shm_vec_mtx);
                std::vector<uint32_t>::iterator position = std::find(ffr->srq_shm_vec.begin(), ffr->srq_shm_vec.end(), ((struct IBV_DESTROY_SRQ_REQ *)req_body)->srq_handle);
                if (position != ffr->srq_shm_vec.end()) // == myVector.end() means the element was not found
                    ffr->srq_shm_vec.erase(position);
                pthread_mutex_unlock(&ffr->srq_shm_vec_mtx); 

                ShmPiece* sp = ffr->srq_shm_map[((struct IBV_DESTROY_SRQ_REQ *)req_body)->srq_handle];
                if (sp)
                    delete sp;
                ffr->srq_shm_map[((struct IBV_DESTROY_SRQ_REQ *)req_body)->srq_handle] = NULL;

            }
            break;

            case IBV_POST_SRQ_RECV: 
            {
                LOG_INFO("POST_SRQ_RECV");
                //req_body = malloc(header.body_size);
                if (read(client_sock, req_body, header.body_size) < header.body_size)
                {
                    LOG_ERROR("POST_SRQ_RECV: Error in reading in post recv.");
                    goto end;
                }

                // Now recover the qp and wr
                struct ibv_post_recv *post_recv = (struct ibv_post_recv*)req_body;
                if (post_recv->qp_handle >= MAP_SIZE)
                {
                    LOG_ERROR("[Warning] SRQ handle (" << post_recv->qp_handle << ") is no less than MAX_QUEUE_MAP_SIZE.");
                }
                else
                {
                    srq = ffr->srq_map[post_recv->qp_handle];
                }

                struct ibv_recv_wr *wr = (struct ibv_recv_wr*)((char*)req_body + sizeof(struct ibv_post_recv));
                struct ibv_sge *sge = (struct ibv_sge*)((char*)req_body + sizeof(struct ibv_post_recv) + post_recv->wr_count * sizeof(struct ibv_recv_wr));

                for (int i = 0; i < post_recv->wr_count; i++)
                {
                    // fix the link list pointer
                    if (i >= post_recv->wr_count - 1) {
                        wr[i].next = NULL;
                    }
                    else {
                        wr[i].next = &(wr[i+1]);
                    }
                    if (wr[i].num_sge > 0) {
                        // fix the sg list pointer
                        wr[i].sg_list = sge;
                        pthread_mutex_lock(&ffr->lkey_ptr_mtx);
                        for (int j = 0; j < wr[i].num_sge; j++) {
                            sge[j].addr = (uint64_t)(ffr->lkey_ptr[sge[j].lkey]) + (uint64_t)(sge[j].addr);
                        }
                        pthread_mutex_unlock(&ffr->lkey_ptr_mtx);
                        sge += wr[i].num_sge;
                    }
                }

                struct ibv_recv_wr *bad_wr = NULL;
                //rsp = malloc(sizeof(struct IBV_POST_SRQ_RECV_RSP));
                size = sizeof(struct IBV_POST_SRQ_RECV_RSP);
                ((struct IBV_POST_SRQ_RECV_RSP*)rsp)->ret_errno = ibv_post_srq_recv(srq, wr, &bad_wr);
                if (((struct IBV_POST_SRQ_RECV_RSP*)rsp)->ret_errno != 0) {
                    LOG_ERROR("[Error] Srq post recv (" << srq->handle << ") fails.");
                }
                if (bad_wr == NULL) 
                {
                    ((struct IBV_POST_SRQ_RECV_RSP*)rsp)->bad_wr = 0;
                }
                else
                {
                    ((struct IBV_POST_SRQ_RECV_RSP*)rsp)->bad_wr = bad_wr - wr;
                }
            }
            break;

            case CM_CREATE_EVENT_CHANNEL:
            {
                LOG_DEBUG("CM_CREATE_EVENT_CHANNEL");

                size = sizeof(struct CM_CREATE_EVENT_CHANNEL_RSP);
                event_channel = rdma_create_event_channel(); 

                if (event_channel->fd >= MAP_SIZE)
                {
                    LOG_INFO("channel id is no less than MAX_QUEUE_MAP_SIZE. channel_id=" << event_channel->fd); 
                }
                else
                {
                    ffr->event_channel_map[event_channel->fd] = event_channel;
                }

                if (send_fd(client_sock, event_channel->fd) < 0)
                {
                    LOG_ERROR("failed to send_fd.");
                }

                memcpy(&(((struct CM_CREATE_EVENT_CHANNEL_RSP *)rsp)->ec), event_channel, sizeof(struct rdma_event_channel));
                LOG_DEBUG("Return channel " << event_channel->fd << " for client_id " << header.client_id); 
            }
            break;

            case CM_DESTROY_EVENT_CHANNEL:
            {
                LOG_DEBUG("CM_DESTROY_EVENT_CHANNEL");

                if (read(client_sock, req_body, sizeof(struct CM_DESTROY_EVENT_CHANNEL_REQ)) < sizeof(struct CM_DESTROY_EVENT_CHANNEL_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Destroy event channel: " << ((CM_DESTROY_EVENT_CHANNEL_REQ*)req_body)->ec.fd); 

                event_channel = ffr->event_channel_map[((CM_DESTROY_EVENT_CHANNEL_REQ*)req_body)->ec.fd];
                if (event_channel == NULL)
                {
                    LOG_ERROR("Failed to get event channel with id " << ((CM_DESTROY_EVENT_CHANNEL_REQ*)req_body)->ec.fd);
                    goto end;
                }

                ffr->event_channel_map[((CM_DESTROY_EVENT_CHANNEL_REQ*)req_body)->ec.fd] = NULL;
                rdma_destroy_event_channel(event_channel);
                ((struct CM_DESTROY_EVENT_CHANNEL_RSP *)rsp)->ret_errno = 0;
                size = sizeof(struct CM_DESTROY_EVENT_CHANNEL_RSP);
            }
            break;

            case CM_CREATE_ID:
            {
                LOG_DEBUG("CM_CREATE_ID");

                //req_body = malloc(sizeof(struct CM_CREATE_ID_REQ));
                if (read(client_sock, req_body, sizeof(struct CM_CREATE_ID_REQ)) < sizeof(struct CM_CREATE_ID_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Create ID for event channel: " << ((CM_CREATE_ID_REQ*)req_body)->ec.fd); 

                ((struct CM_CREATE_ID_RSP *)rsp)->ret_errno = rdma_create_id_resp( 
                    &((CM_CREATE_ID_REQ*)req_body)->ec,
                    &((CM_CREATE_ID_REQ*)req_body)->cmd,
                    &((CM_CREATE_ID_RSP *)rsp)->resp);

                LOG_DEBUG("Create ID handle: " << ((struct CM_CREATE_ID_RSP *)rsp)->resp.id); 
                size = sizeof(struct CM_CREATE_ID_RSP);
                if (((struct CM_CREATE_ID_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_CREATE_ID_RSP *)rsp)->ret_errno  << ") in CM_CREATE_ID");
            }
            break;

            case CM_BIND_IP:
            {
                LOG_DEBUG("CM_BIND_IP");

                if (read(client_sock, req_body, sizeof(struct CM_BIND_IP_REQ)) < sizeof(struct CM_BIND_IP_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Bind IP for cm_id: " << ((CM_BIND_IP_REQ*)req_body)->cmd.id); 

                struct sockaddr *addr_str = (struct sockaddr*)&((CM_BIND_IP_REQ*)req_body)->cmd.addr;
                ffr->map_vip(&((struct sockaddr_in *)addr_str)->sin_addr);

                ((struct CM_BIND_IP_RSP *)rsp)->ret_errno = rdma_bind_addr_resp(
                    &((CM_BIND_IP_REQ*)req_body)->ec,
                    &((CM_BIND_IP_REQ*)req_body)->cmd,
                    NULL);

        LOG_ERROR("BIND_IP fd-->" << ((CM_BIND_IP_REQ*)req_body)->ec.fd);
                    
                if (((struct CM_BIND_IP_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_BIND_IP_RSP *)rsp)->ret_errno  << ") in CM_BIND_IP");
            }
            break;

            case CM_BIND:
            {
                LOG_DEBUG("CM_BIND");

                if (read(client_sock, req_body, sizeof(struct CM_BIND_REQ)) < sizeof(struct CM_BIND_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Bind for cm_id: " << ((CM_BIND_REQ*)req_body)->cmd.id); 

                ffr->map_vip(get_in_addr((struct sockaddr *)&(((CM_BIND_REQ*)req_body)->cmd.addr)));
                
                ((struct CM_BIND_RSP *)rsp)->ret_errno = rdma_bind_resp(
                    &((CM_BIND_REQ*)req_body)->ec,
                    &((CM_BIND_REQ*)req_body)->cmd,
                    NULL);
                
                size = sizeof(struct CM_BIND_RSP);
                if (((struct CM_BIND_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_BIND_RSP *)rsp)->ret_errno  << ") in CM_BIND");
            }
            break;

            case CM_GET_EVENT:
            {
                LOG_INFO("CM_GET_EVENT");

                if (read(client_sock, req_body, sizeof(struct CM_GET_EVENT_REQ)) < sizeof(struct CM_GET_EVENT_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Get event from event channel: " << ((CM_GET_EVENT_REQ*)req_body)->ec.fd); 

                ((struct CM_GET_EVENT_RSP *)rsp)->ret_errno = rdma_get_cm_event_resp(
                    &((CM_GET_EVENT_REQ*)req_body)->ec,
                    &((CM_GET_EVENT_REQ*)req_body)->cmd,
                    &((CM_GET_EVENT_RSP *)rsp)->resp);

                size = sizeof(struct CM_GET_EVENT_RSP);
                if (((struct CM_GET_EVENT_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_GET_EVENT_RSP *)rsp)->ret_errno  << ") in CM_GET_EVENT");
            }
            break;

            case CM_QUERY_ROUTE:
            {
                LOG_DEBUG("CM_QUERY_ROUTE");

                if (read(client_sock, req_body, sizeof(struct CM_QUERY_ROUTE_REQ)) < sizeof(struct CM_QUERY_ROUTE_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Query route for cm_id: " << ((CM_QUERY_ROUTE_REQ*)req_body)->cmd.id); 

                ((struct CM_QUERY_ROUTE_RSP *)rsp)->ret_errno = ucma_query_route_resp(
                    &((CM_QUERY_ROUTE_REQ*)req_body)->ec,
                    &((CM_QUERY_ROUTE_REQ*)req_body)->cmd,
                    &((CM_QUERY_ROUTE_RSP*)rsp)->resp);
            
                size = sizeof(struct CM_QUERY_ROUTE_RSP);
                if (((struct CM_QUERY_ROUTE_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_QUERY_ROUTE_RSP *)rsp)->ret_errno  << ") in CM_QUERY_ROUTE");
            }
            break;

            case CM_LISTEN:
            {
                LOG_DEBUG("CM_LISTEN");

                if (read(client_sock, req_body, sizeof(struct CM_LISTEN_REQ)) < sizeof(struct CM_LISTEN_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Listen for cm_id: " << ((CM_LISTEN_REQ*)req_body)->cmd.id); 

                ((struct CM_LISTEN_RSP *)rsp)->ret_errno = rdma_listen_resp(
                    &((CM_LISTEN_REQ*)req_body)->ec,
                    &((CM_LISTEN_REQ*)req_body)->cmd,
                    NULL);
                    
                size = sizeof(struct CM_LISTEN_RSP);
                if (((struct CM_LISTEN_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_LISTEN_RSP *)rsp)->ret_errno  << ") in CM_LISTEN");
            }
            break;

            case CM_RESOLVE_IP:
            {
                LOG_DEBUG("CM_RESOLVE_IP");

                if (read(client_sock, req_body, sizeof(struct CM_RESOLVE_IP_REQ)) < sizeof(struct CM_RESOLVE_IP_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Resolve IP for cm_id: " << ((CM_RESOLVE_IP_REQ*)req_body)->cmd.id); 

                struct sockaddr * src_addr = (struct sockaddr *)&((CM_RESOLVE_IP_REQ*)req_body)->cmd.src_addr;
                struct sockaddr * dst_addr = (struct sockaddr *)&((CM_RESOLVE_IP_REQ*)req_body)->cmd.dst_addr;

                char src_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET,
                    get_in_addr(src_addr),
                    src_str,
                    sizeof src_str);
                    
                char dst_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET,
                    get_in_addr(dst_addr),
                    dst_str,
                    sizeof dst_str);
                    
                LOG_INFO("@@ CM_RESOLVE_IP src : " << src_str << " dst: " << dst_str);

                ffr->map_vip(get_in_addr(src_addr));
                ffr->map_vip(get_in_addr(dst_addr));
                
                ((struct CM_RESOLVE_IP_RSP *)rsp)->ret_errno = rdma_resolve_addr_resp(
                    &((CM_RESOLVE_IP_REQ*)req_body)->ec,
                    &((CM_RESOLVE_IP_REQ*)req_body)->cmd,
                    NULL);
                    
                size = sizeof(struct CM_RESOLVE_IP_RSP);
                if (((struct CM_RESOLVE_IP_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_RESOLVE_IP_RSP *)rsp)->ret_errno  << ") in CM_RESOLVE_IP");
            }
            break;

            case CM_RESOLVE_ADDR:
            {
                LOG_DEBUG("CM_RESOLVE_ADDR");

                if (read(client_sock, req_body, sizeof(struct CM_RESOLVE_ADDR_REQ)) < sizeof(struct CM_RESOLVE_ADDR_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Bind for cm_id: " << ((CM_RESOLVE_ADDR_REQ*)req_body)->cmd.id); 

                struct sockaddr * src_addr = (struct sockaddr *)&((CM_RESOLVE_ADDR_REQ*)req_body)->cmd.src_addr;
                struct sockaddr * dst_addr = (struct sockaddr *)&((CM_RESOLVE_ADDR_REQ*)req_body)->cmd.dst_addr;
                socklen_t src_size = ((CM_RESOLVE_ADDR_REQ*)req_body)->cmd.src_size;		
                socklen_t dst_size = ((CM_RESOLVE_ADDR_REQ*)req_body)->cmd.dst_size;		

                char src_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET,
                    get_in_addr(src_addr),
                    src_str,
                    sizeof src_str);

                char dst_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET,
                    get_in_addr(dst_addr),
                    dst_str,
                    sizeof dst_str);

                LOG_DEBUG("@@ CM_RESOLVE_ADDR src : " << src_str << " dst: " << dst_str);

                ffr->map_vip(get_in_addr(src_addr));
                ffr->map_vip(get_in_addr(dst_addr));
                
                ((struct CM_RESOLVE_ADDR_RSP *)rsp)->ret_errno = rdma_resolve_addr2_resp(
                    &((CM_RESOLVE_ADDR_REQ*)req_body)->ec,
                    &((CM_RESOLVE_ADDR_REQ*)req_body)->cmd,
                    NULL);
                size = sizeof(struct CM_RESOLVE_IP_RSP);
                
                if (((struct CM_RESOLVE_IP_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_RESOLVE_IP_RSP *)rsp)->ret_errno  << ") in CM_RESOLVE_IP");
            }
            break;

            case CM_UCMA_QUERY_ADDR:
            {
                LOG_DEBUG("CM_UCMA_QUERY_ADDR");

                if (read(client_sock, req_body, sizeof(struct CM_UCMA_QUERY_ADDR_REQ)) < sizeof(struct CM_UCMA_QUERY_ADDR_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("UCMA query addr for cm_id: " << ((CM_UCMA_QUERY_ADDR_REQ*)req_body)->cmd.id); 

                ((struct CM_UCMA_QUERY_ADDR_RSP *)rsp)->ret_errno = ucma_query_addr_resp(
                    &((CM_UCMA_QUERY_ADDR_REQ*)req_body)->ec,
                    &((CM_UCMA_QUERY_ADDR_REQ*)req_body)->cmd,
                    &((CM_UCMA_QUERY_ADDR_RSP*)rsp)->resp);
                    
                size = sizeof(struct CM_UCMA_QUERY_ADDR_RSP);
                if (((struct CM_UCMA_QUERY_ADDR_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_UCMA_QUERY_ADDR_RSP *)rsp)->ret_errno  << ") in UCMA_QUERY_ADDR");
            }
            break;

            case CM_UCMA_QUERY_GID:
            {
                LOG_DEBUG("CM_UCMA_QUERY_GID");

                if (read(client_sock, req_body, sizeof(struct CM_UCMA_QUERY_GID_REQ)) < sizeof(struct CM_UCMA_QUERY_GID_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("UCMA query gid for cm_id: " << ((CM_UCMA_QUERY_GID_REQ*)req_body)->cmd.id); 

                ((struct CM_UCMA_QUERY_GID_RSP *)rsp)->ret_errno = ucma_query_gid_resp(
                    &((CM_UCMA_QUERY_GID_REQ*)req_body)->ec,
                    &((CM_UCMA_QUERY_GID_REQ*)req_body)->cmd,
                    &((CM_UCMA_QUERY_GID_RSP*)rsp)->resp);

                size = sizeof(struct CM_UCMA_QUERY_GID_RSP);
                
                if (((struct CM_UCMA_QUERY_GID_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_UCMA_QUERY_GID_RSP *)rsp)->ret_errno  << ") in CM_UCMA_QUERY_GID");
            }
            break;

            case CM_UCMA_PROCESS_CONN_RESP:
            {
                LOG_DEBUG("CM_UCMA_PROCESS_CONN_RESP");

                if (read(client_sock, req_body, sizeof(struct CM_UCMA_PROCESS_CONN_RESP_REQ)) < sizeof(struct CM_UCMA_PROCESS_CONN_RESP_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("UCMA process conn resp for cm_id: " << ((CM_UCMA_PROCESS_CONN_RESP_REQ*)req_body)->cmd.id); 

                ((struct CM_UCMA_PROCESS_CONN_RESP_RSP *)rsp)->ret_errno = ucma_process_conn_resp_resp(
                    &((CM_UCMA_PROCESS_CONN_RESP_REQ*)req_body)->ec,
                    &((CM_UCMA_PROCESS_CONN_RESP_REQ*)req_body)->cmd,
                    NULL);

                size = sizeof(struct CM_UCMA_PROCESS_CONN_RESP_RSP);
                
                if (((struct CM_UCMA_PROCESS_CONN_RESP_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_UCMA_PROCESS_CONN_RESP_RSP *)rsp)->ret_errno  << ") in CM_PROCESS_CONN_RESP");
            }
            break;

            case CM_DESTROY_ID:
            {
                LOG_DEBUG("CM_DESTROY_ID");

                if (read(client_sock, req_body, sizeof(struct CM_DESTROY_ID_REQ)) < sizeof(struct CM_DESTROY_ID_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Destroy cm_id: " << ((CM_DESTROY_ID_REQ*)req_body)->cmd.id << " with channel " << ((CM_DESTROY_ID_REQ*)req_body)->ec.fd); 

                ((struct CM_DESTROY_ID_RSP *)rsp)->ret_errno = ucma_destroy_kern_id_resp(
                    &((CM_DESTROY_ID_REQ*)req_body)->ec,
                    &((CM_DESTROY_ID_REQ*)req_body)->cmd,
                    &((CM_DESTROY_ID_RSP*)rsp)->resp);

                size = sizeof(struct CM_DESTROY_ID_RSP);
                
                if (((struct CM_DESTROY_ID_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_DESTROY_ID_RSP *)rsp)->ret_errno  << ") in CM_DESTROY_ID");
            }
            break;

            case CM_RESOLVE_ROUTE:
            {
                LOG_DEBUG("CM_RESOLVE_ROUTE");

                if (read(client_sock, req_body, sizeof(struct CM_RESOLVE_ROUTE_REQ)) < sizeof(struct CM_RESOLVE_ROUTE_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Bind for cm_id: " << ((CM_RESOLVE_ROUTE_REQ*)req_body)->cmd.id); 

                ((struct CM_RESOLVE_ROUTE_RSP *)rsp)->ret_errno = rdma_resolve_route_resp(
                    &((CM_RESOLVE_ROUTE_REQ*)req_body)->ec,
                    &((CM_RESOLVE_ROUTE_REQ*)req_body)->cmd,
                    NULL);
                size = sizeof(struct CM_RESOLVE_ROUTE_RSP);
                
                if (((struct CM_RESOLVE_ROUTE_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_RESOLVE_ROUTE_RSP *)rsp)->ret_errno  << ") in CM_RESOLVE_ROUTE");
            }
            break;

            case CM_UCMA_QUERY_PATH:
            {
                LOG_DEBUG("CM_UCMA_QUERY_PATH");

                if (read(client_sock, req_body, sizeof(struct CM_UCMA_QUERY_PATH_REQ)) < sizeof(struct CM_UCMA_QUERY_PATH_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("UCMA query path for cm_id: " << ((CM_UCMA_QUERY_PATH_REQ*)req_body)->cmd.id); 

                ((struct CM_UCMA_QUERY_PATH_RSP *)rsp)->ret_errno = ucma_query_path_resp(
                    &((CM_UCMA_QUERY_PATH_REQ*)req_body)->ec,
                    &((CM_UCMA_QUERY_PATH_REQ*)req_body)->cmd,
                    &((CM_UCMA_QUERY_PATH_RSP*)rsp)->resp);

                size = sizeof(struct CM_UCMA_QUERY_PATH_RSP);
                
                if (((struct CM_UCMA_QUERY_PATH_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_UCMA_QUERY_PATH_RSP *)rsp)->ret_errno  << ") in CM_UCMA_QUERY_PATH");
            }
            break;

             case CM_INIT_QP_ATTR:
            {
                LOG_DEBUG("CM_INIT_QP_ATTR");

                if (read(client_sock, req_body, sizeof(struct CM_INIT_QP_ATTR_REQ)) < sizeof(struct CM_INIT_QP_ATTR_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("Init QP attr for cm_id: " << ((CM_INIT_QP_ATTR_REQ*)req_body)->cmd.id); 

                ((struct CM_INIT_QP_ATTR_RSP *)rsp)->ret_errno = rdma_init_qp_attr_resp(
                    &((CM_INIT_QP_ATTR_REQ*)req_body)->ec,
                    &((CM_INIT_QP_ATTR_REQ*)req_body)->cmd,
                    &((CM_INIT_QP_ATTR_RSP*)rsp)->resp);

                size = sizeof(struct CM_INIT_QP_ATTR_RSP);
                
                if (((struct CM_INIT_QP_ATTR_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_INIT_QP_ATTR_RSP *)rsp)->ret_errno  << ") in CM_INIT_QP_ATTR");
            }
            break;

             case CM_CONNECT:
            {
                LOG_DEBUG("CM_CONNECT");

                if (read(client_sock, req_body, sizeof(struct CM_CONNECT_REQ)) < sizeof(struct CM_CONNECT_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("CM connect for cm_id: " << ((struct CM_CONNECT_REQ*)req_body)->cmd.id); 

                ((struct CM_CONNECT_RSP *)rsp)->ret_errno = rdma_connect_resp(
                    &((CM_CONNECT_REQ*)req_body)->ec,
                    &((CM_CONNECT_REQ*)req_body)->cmd,
                    NULL);
                
                size = sizeof(struct CM_CONNECT_RSP);
                if (((struct CM_CONNECT_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_CONNECT_RSP *)rsp)->ret_errno  << ") in CM_CONNECT");
            }
            break;

             case CM_ACCEPT:
            {
                LOG_DEBUG("CM_ACCEPT");

                if (read(client_sock, req_body, sizeof(struct CM_ACCEPT_REQ)) < sizeof(struct CM_ACCEPT_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("CM accept for cm_id: " << ((CM_ACCEPT_REQ*)req_body)->cmd.id); 

                ((struct CM_ACCEPT_RSP *)rsp)->ret_errno = rdma_accept_resp(
                    &((CM_ACCEPT_REQ*)req_body)->ec,
                    &((CM_ACCEPT_REQ*)req_body)->cmd,
                    NULL);
                    
                size = sizeof(struct CM_ACCEPT_RSP);
                
                if (((struct CM_ACCEPT_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_ACCEPT_RSP *)rsp)->ret_errno  << ") in CM_ACCEPT");
            }
            break;

             case CM_SET_OPTION:
            {
                LOG_DEBUG("CM_SET_OPTION");

                if (read(client_sock, req_body, sizeof(struct CM_SET_OPTION_REQ)) < sizeof(struct CM_SET_OPTION_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("CM set option for cm_id: " << ((CM_SET_OPTION_REQ*)req_body)->cmd.id); 

                ((struct CM_SET_OPTION_RSP *)rsp)->ret_errno = rdma_set_option_resp(
                    &((CM_SET_OPTION_REQ*)req_body)->ec,
                    &((CM_SET_OPTION_REQ*)req_body)->cmd,
                    NULL,
                    ((CM_SET_OPTION_REQ*)req_body)->optval);

                size = sizeof(struct CM_SET_OPTION_RSP);
        
            if (((struct CM_SET_OPTION_RSP *)rsp)->ret_errno != 0)
                LOG_ERROR("Return error (" << ((struct CM_SET_OPTION_RSP *)rsp)->ret_errno  << ") in CM_SET_OPTION");
            }
            break;

             case CM_MIGRATE_ID:
            {
                LOG_DEBUG("CM_MIGRATE_ID");

                if (read(client_sock, req_body, sizeof(struct CM_MIGRATE_ID_REQ)) < sizeof(struct CM_MIGRATE_ID_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                ((struct CM_MIGRATE_ID_RSP *)rsp)->ret_errno = rdma_migrate_id_resp( 
                    &((CM_MIGRATE_ID_REQ*)req_body)->ec, 
                    &((CM_MIGRATE_ID_REQ*)req_body)->cmd, 
                    &((struct CM_MIGRATE_ID_RSP *)rsp)->resp);

                size = sizeof(struct CM_MIGRATE_ID_RSP);
                if (((struct CM_MIGRATE_ID_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_MIGRATE_ID_RSP *)rsp)->ret_errno  << ") in CM_MIGRATE_ID");
            }
            break;

             case CM_DISCONNECT:
            {
                LOG_DEBUG("CM_DISCONNECT");

                if (read(client_sock, req_body, sizeof(struct CM_DISCONNECT_REQ)) < sizeof(struct CM_DISCONNECT_REQ))
                {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                LOG_DEBUG("CM disconnect for cm_id: " << ((struct CM_DISCONNECT_REQ*)req_body)->cmd.id); 

                ((struct CM_DISCONNECT_RSP *)rsp)->ret_errno = rdma_disconnect_resp(
                    &((CM_DISCONNECT_REQ*)req_body)->ec, 
                    &((CM_DISCONNECT_REQ*)req_body)->cmd, 
                    NULL);
                    
                size = sizeof(struct CM_DISCONNECT_RSP);
                if (((struct CM_DISCONNECT_RSP *)rsp)->ret_errno != 0)
                    LOG_ERROR("Return error (" << ((struct CM_DISCONNECT_RSP *)rsp)->ret_errno  << ") in CM_DISCONNECT");
            }
            break;

            case SOCKET_SOCKET:
            {
                LOG_DEBUG("SOCKET_SOCKET");

                if (read(client_sock, req_body, sizeof(struct SOCKET_SOCKET_REQ)) < sizeof(struct SOCKET_SOCKET_REQ)) {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                ((struct SOCKET_SOCKET_RSP *)rsp)->ret = socket(
                    // ((SOCKET_SOCKET_REQ*)req_body)->domain,
                    // for now we always use IPv4 for host socket
                    AF_INET, 
                    ((SOCKET_SOCKET_REQ*)req_body)->type, 
                    ((SOCKET_SOCKET_REQ*)req_body)->protocol);
                    
                size = sizeof(struct SOCKET_SOCKET_RSP);
                if (((struct SOCKET_SOCKET_RSP *)rsp)->ret < 0) {
                    LOG_ERROR("Return error (" << ((struct SOCKET_SOCKET_RSP *)rsp)->ret  << ") in SOCKET_SOCKET");
                    ((struct SOCKET_SOCKET_RSP *)rsp)->ret = -errno;
                }
            }
            break;

            case SOCKET_BIND:
            {
                LOG_DEBUG("SOCKET_BIND");

                host_fd = recv_fd(client_sock);
                if (host_fd < 0) {
                    LOG_ERROR("Failed to read host fd."); 
                    goto kill;
                }

                struct sockaddr_in host_addr;
                host_addr.sin_family = AF_INET;
                host_addr.sin_addr.s_addr = ffr->host_ip;
                host_addr.sin_port = 0;

                ((struct SOCKET_BIND_RSP *)rsp)->ret = bind(
                    host_fd, 
                    (struct sockaddr *)&host_addr, 
                    sizeof(host_addr));
                if (((struct SOCKET_BIND_RSP *)rsp)->ret < 0) {
                    LOG_ERROR("Return error (" << ((struct SOCKET_BIND_RSP *)rsp)->ret  << ") in SOCKET_BIND errno:" << errno);
                    ((struct SOCKET_BIND_RSP *)rsp)->ret = -errno;
                }
                size = sizeof(struct SOCKET_BIND_RSP);
            }
            break;

            case SOCKET_ACCEPT:
            {
                LOG_DEBUG("SOCKET_ACCEPT");

                host_fd = recv_fd(client_sock);
                if (host_fd < 0) {
                    LOG_ERROR("Failed to read host fd."); 
                    goto kill;
                }

                struct sockaddr_in host_addr;
                socklen_t host_addrlen = sizeof(host_addr);

                // clear the non-blocking flag
                int original_flags = fcntl(host_fd, F_GETFL);
                fcntl(host_fd, F_SETFL, original_flags & ~O_NONBLOCK);

                ((struct SOCKET_ACCEPT_RSP *)rsp)->ret = accept(
                    host_fd, 
                    (struct sockaddr *)&host_addr, 
                    &host_addrlen);

                if (((struct SOCKET_ACCEPT_RSP *)rsp)->ret < 0) {
                    LOG_ERROR("Return error (" << ((struct SOCKET_ACCEPT_RSP *)rsp)->ret  << ") in SOCKET_ACCEPT");
                    ((struct SOCKET_ACCEPT_RSP *)rsp)->ret = -errno;
                }

                fcntl(host_fd, F_SETFL, original_flags);   
                size = sizeof(struct SOCKET_ACCEPT_RSP);
            }
            break;

            case SOCKET_ACCEPT4:
            {
                LOG_DEBUG("SOCKET_ACCEPT4");

                if (read(client_sock, req_body, sizeof(struct SOCKET_ACCEPT4_REQ)) < sizeof(struct SOCKET_ACCEPT4_REQ)) {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                host_fd = recv_fd(client_sock);
                if (host_fd < 0) {
                    LOG_ERROR("Failed to read host fd."); 
                    goto kill;
                }

                struct sockaddr_in host_addr;
                socklen_t host_addrlen = sizeof(host_addr);

                // clear the non-blocking flag
                int original_flags = fcntl(host_fd, F_GETFL);
                fcntl(host_fd, F_SETFL, original_flags & ~O_NONBLOCK);

                ((struct SOCKET_ACCEPT4_RSP *)rsp)->ret = accept4(
                    host_fd, 
                    (struct sockaddr *)&host_addr, 
                    &host_addrlen,
                    ((SOCKET_ACCEPT4_REQ*)req_body)->flags);

                if (((struct SOCKET_ACCEPT4_RSP *)rsp)->ret < 0) {
                    LOG_ERROR("Return error (" << ((struct SOCKET_ACCEPT4_RSP *)rsp)->ret  << ") in SOCKET_ACCEPT4 errno:" << errno);
                    ((struct SOCKET_ACCEPT4_RSP *)rsp)->ret = -errno;
                }

                fcntl(host_fd, F_SETFL, original_flags);
                size = sizeof(struct SOCKET_ACCEPT4_RSP);
            }
            break;

            case SOCKET_CONNECT:
            {
                LOG_DEBUG("SOCKET_CONNECT");

                if (read(client_sock, req_body, sizeof(struct SOCKET_CONNECT_REQ)) < sizeof(struct SOCKET_CONNECT_REQ)) {
                    LOG_ERROR("Failed to read the request body."); 
                    goto kill;
                }

                host_fd = recv_fd(client_sock);
                if (host_fd < 0) {
                    LOG_ERROR("Failed to read host fd."); 
                    goto kill;
                }

                struct sockaddr_in host_addr;
                int host_addrlen;

                ((struct SOCKET_CONNECT_RSP *)rsp)->ret = connect(
                    host_fd, 
                    (struct sockaddr *)&((SOCKET_CONNECT_REQ*)req_body)->host_addr, 
                    ((SOCKET_CONNECT_REQ*)req_body)->host_addrlen);

                size = sizeof(struct SOCKET_CONNECT_RSP);
                if (((struct SOCKET_CONNECT_RSP *)rsp)->ret < 0) {
                    LOG_ERROR("Return error (" << ((struct SOCKET_CONNECT_RSP *)rsp)->ret  << ") in SOCKET_CONNECT");
                    ((struct SOCKET_CONNECT_RSP *)rsp)->ret = -errno;
                }        
            }
            break;

            default:
                break;
        }


    LOG_TRACE("write rsp " << size << " bytes to sock " << client_sock);
        if ((n = write(client_sock, rsp, size)) < size)
        {
            LOG_ERROR("Error in writing bytes" << n);
            /*if (req_body != NULL)
                free(req_body);
    
            if(rsp != NULL)
                free(rsp);*/

            goto kill;
        }

        if (header.func == SOCKET_SOCKET || header.func == SOCKET_ACCEPT || header.func == SOCKET_ACCEPT4) {
            if (((struct SOCKET_SOCKET_RSP *)rsp)->ret >= 0) {
                if (send_fd(client_sock, ((struct SOCKET_SOCKET_RSP *)rsp)->ret) < 0) {
                    LOG_ERROR("failed to send_fd for socket.");
                }
                close(((struct SOCKET_SOCKET_RSP *)rsp)->ret);
            }
        }


    //memset(rsp, 0, 0xfffff);
    
end:
        if (host_fd >= 0) {
            close(host_fd);
        }
        if (header.func == SOCKET_SOCKET || header.func == SOCKET_BIND || 
            header.func == SOCKET_ACCEPT || header.func == SOCKET_ACCEPT4 ||
            header.func == SOCKET_CONNECT) {
                break;
            }
    }

kill:
    close(client_sock);
    free(args);
    free(rsp);
    free(req_body);
} 

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void FreeFlowRouter::map_vip(void *addr)
{
    char astring[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, addr, astring, sizeof astring);
    LOG_INFO("Map VIP (VIP): " << astring);
                
    std::string vip(astring), dip;
    if (this->vip_map.find(vip) != this->vip_map.end())
    {
        dip = this->vip_map[vip];
        inet_pton(AF_INET, dip.c_str(), addr);
    }
    else
    {
        dip = vip;
    }
    
    inet_ntop(AF_INET, addr, astring, sizeof astring);
    LOG_INFO("Map VIP (DIP): " << dip);
}

void* UDPServer(void* param) {
    struct sockaddr_in si_me, si_other;
    int s, i, slen=sizeof(si_other);
    char buf[1400];
    struct IBV_REG_MR_MAPPING_REQ* p;
    p = (struct IBV_REG_MR_MAPPING_REQ*)buf;
    
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
        LOG_ERROR("Error in creating socket for UDP server");
        return NULL;
    }
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(UDP_PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (const sockaddr*)&si_me, sizeof(si_me))==-1){
        LOG_ERROR("Error in binding UDP port");
        return NULL;
    }
    for (;;) {
        if (recvfrom(s, buf, 1400, 0, (sockaddr*)&si_other, (socklen_t*)&slen)==-1) {
            LOG_DEBUG("Error in receiving UDP packets");
            return NULL;
        }
        else {
            struct MR_SHM mr_shm;
            mr_shm.mr_ptr = p->mr_ptr;
            mr_shm.shm_ptr = p->shm_ptr;

            pthread_mutex_lock(&(((struct HandlerArgs *)param)->ffr->rkey_mr_shm_mtx));
            ((struct HandlerArgs *)param)->ffr->rkey_mr_shm[p->key] = mr_shm;
            pthread_mutex_unlock(&(((struct HandlerArgs *)param)->ffr->rkey_mr_shm_mtx));

                char src_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET,
                    &si_other.sin_addr,
                    src_str,
                    sizeof src_str);

            int self_p = ntohs(si_other.sin_port);
            LOG_DEBUG("Receive MR Mapping: rkey=" << (uint32_t)(p->key) << " mr=" << (uint64_t)(p->mr_ptr) << " shm=" << (uint64_t)(p->shm_ptr) << " from " << src_str << ":" << self_p);

            sprintf(buf, "ack-%u", p->key);
            if (sendto(s, buf, 1400, 0, (const sockaddr*)&si_other, slen)==-1)
            {
                LOG_ERROR("Error in sending MR mapping to " << HOST_LIST[i]);
            }


        }
    }
    return NULL;
}

void FreeFlowRouter::start_udp_server() {
    pthread_t *pth = (pthread_t *) malloc(sizeof(pthread_t));
    struct HandlerArgs *args = (struct HandlerArgs *) malloc(sizeof(struct HandlerArgs));
    args->ffr = this;
    int ret = pthread_create(pth, NULL, (void* (*)(void*))UDPServer, args);
    LOG_DEBUG("result of start_udp_server --> " << ret);
}

int send_fd(int sock, int fd)
{
    ssize_t     size;
    struct msghdr   msg;
    struct iovec    iov;
    union {
        struct cmsghdr  cmsghdr;
        char        control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr  *cmsg;
    char buf[2];

    iov.iov_base = buf;
    iov.iov_len = 2;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (fd != -1) {
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(sizeof (int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        //printf ("passing fd %d\n", fd);
        *((int *) CMSG_DATA(cmsg)) = fd;
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        //printf ("not passing fd\n");
    }

    size = sendmsg(sock, &msg, 0);

    if (size < 0) {
        perror ("sendmsg");
    }
    return size;
}

int recv_fd(int sock)
{
    ssize_t size;
    struct msghdr msg;
    struct iovec iov;
    union {
        struct cmsghdr cmsghdr;
        char control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr *cmsg;
    char buf[2];
    int fd = -1;

    iov.iov_base = buf;
    iov.iov_len = 2;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);
    size = recvmsg (sock, &msg, 0);
    if (size < 0) {
        perror ("recvmsg");
        return -1;
    }
    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
        if (cmsg->cmsg_level != SOL_SOCKET) {
            fprintf (stderr, "invalid cmsg_level %d\n",
                    cmsg->cmsg_level);
            return -1;
        }
        if (cmsg->cmsg_type != SCM_RIGHTS) {
            fprintf (stderr, "invalid cmsg_type %d\n",
                    cmsg->cmsg_type);
            return -1;
        }
        int *fd_p = (int *)CMSG_DATA(cmsg);
        fd = *fd_p;
        // printf ("received fd %d\n", fd);
    } else {
        fd = -1;
    }

    return(fd);  
}

#if !defined(RDMA_CMA_H_FREEFLOW)
int rdma_bind_addr2(struct rdma_cm_id *id, struct sockaddr *addr, socklen_t addrlen) {return 0;}
int rdma_resolve_addr2(struct rdma_cm_id *id, struct sockaddr *src_addr,
                               socklen_t src_len, struct sockaddr *dst_addr,
                               socklen_t dst_len, int timeout_ms) {return 0;}
int rdma_create_id_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_bind_addr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_bind_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int ucma_query_route_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_listen_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_resolve_addr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_resolve_addr2_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int ucma_query_addr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int ucma_query_gid_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int ucma_process_conn_resp_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int ucma_destroy_kern_id_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_resolve_route_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int ucma_query_path_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_connect_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_accept_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_set_option_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out, void *optval) {return 0;}
int rdma_migrate_id_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_disconnect_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_init_qp_attr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out) {return 0;}
int rdma_get_cm_event_resp(struct rdma_event_channel *channel, void* cmd_in, void* resp_out) {return 0;}
#endif

