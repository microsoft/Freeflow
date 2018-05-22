#ifndef FFROUTER_H
#define FFROUTER_H

#include "constant.h"
#include "shared_memory.h"
#include "rdma_api.h"
#include "types.h"
#include "log.h"
#include "kern-abi.h"
#include "tokenbucket.h"

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <malloc.h>
#include <errno.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <rdma/rdma_cma.h>
#include <netdb.h>
#include <netinet/tcp.h>

#define UDP_PORT 11232
#define HOST_NUM 2

#if defined(SCM_CREDS)          /* BSD interface */
#define CREDSTRUCT      cmsgcred
#define SCM_CREDTYPE    SCM_CREDS
#elif defined(SCM_CREDENTIALS)  /* Linux interface */
#define CREDSTRUCT      ucred
#define SCM_CREDTYPE    SCM_CREDENTIALS
#else
#error passing credentials is unsupported!
#endif


/* size of control buffer to send/recv one file descriptor */
#define RIGHTSLEN   CMSG_LEN(sizeof(int))
#define CREDSLEN    CMSG_LEN(sizeof(struct CREDSTRUCT))
#define CONTROLLEN  (RIGHTSLEN + CREDSLEN)

void mem_flush(const void *p, int allocation_size);
const char HOST_LIST[HOST_NUM][16] = {
    "192.168.2.13",
    "192.168.2.15"
};

struct MR_SHM {
    char* mr_ptr;
    char* shm_ptr;
};

struct HandlerArgs {
    struct FreeFlowRouter *ffr;
    int client_sock;
    int count;
};

class FreeFlowRouter {
public:
    int sock;
    std::string name;
    std::string pathname;
    int pid_count;
    struct ib_data rdma_data;
    struct ibv_pd* pd_map[MAP_SIZE];
    struct ibv_cq* cq_map[MAP_SIZE];
    struct ibv_qp* qp_map[MAP_SIZE];
    struct ibv_mr* mr_map[MAP_SIZE];
    struct ibv_ah* ah_map[MAP_SIZE];
    struct ibv_srq* srq_map[MAP_SIZE];
    struct ibv_comp_channel* channel_map[MAP_SIZE];
    struct rdma_event_channel* event_channel_map[MAP_SIZE];
    struct rdma_cm_id* cm_id_map[MAP_SIZE];
    ShmPiece* shmr_map[MAP_SIZE];
    ShmPiece* qp_shm_map[MAP_SIZE];
    ShmPiece* cq_shm_map[MAP_SIZE];
    ShmPiece* srq_shm_map[MAP_SIZE];
    std::vector<uint32_t> qp_shm_vec;
    pthread_mutex_t qp_shm_vec_mtx;
    std::vector<uint32_t> cq_shm_vec;
    pthread_mutex_t cq_shm_vec_mtx;
    std::vector<uint32_t> srq_shm_vec;
    pthread_mutex_t srq_shm_vec_mtx;

    std::map<uintptr_t, uintptr_t> uid_map;

    // clientid --> shared memmory piece vector
    std::map<int, std::vector<ShmPiece*> > shm_pool;
    std::map<std::string, ShmPiece* > shm_map;
    pthread_mutex_t shm_mutex;

    // lkey --> ptr of shm piece buffer
    std::map<uint32_t, void*> lkey_ptr;
    pthread_mutex_t lkey_ptr_mtx;

    // rkey --> MR and SHM pointers
    std::map<uint32_t, struct MR_SHM> rkey_mr_shm;
    pthread_mutex_t rkey_mr_shm_mtx;

    // qp_handle -> tokenbucket
    std::map<uint32_t, TokenBucket*> tokenbucket;

    // fsocket bind address
    uint32_t host_ip;

    FreeFlowRouter(const char* name);
    ~FreeFlowRouter();    
    void start();
    void start_udp_server();
    void map_vip(void* addr);

    ShmPiece* addShmPiece(int cliend_id, int mem_size);
    ShmPiece* addShmPiece(std::string shm_name, int mem_size);

    ShmPiece* initCtrlShm(const char* tag);

    uint32_t rdma_polling_interval;
    uint8_t disable_rdma;

    std::map<std::string, std::string> vip_map;
};

void HandleRequest(struct HandlerArgs *args);
void CtrlChannelLoop(struct HandlerArgs *args);
void UDPServer();

void *get_in_addr(struct sockaddr *sa);
int send_fd(int sock, int fd);
int recv_fd(int sock);

#if !defined(RDMA_CMA_H_FREEFLOW)
int rdma_bind_addr2(struct rdma_cm_id *id, struct sockaddr *addr, socklen_t addrlen);
int rdma_resolve_addr2(struct rdma_cm_id *id, struct sockaddr *src_addr,
                               socklen_t src_len, struct sockaddr *dst_addr,
                               socklen_t dst_len, int timeout_ms);
int rdma_create_id_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_bind_addr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_bind_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int ucma_query_route_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_listen_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_resolve_addr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_resolve_addr2_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int ucma_query_addr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int ucma_query_gid_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int ucma_process_conn_resp_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int ucma_destroy_kern_id_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_resolve_route_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int ucma_query_path_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_connect_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_accept_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_set_option_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out, void *optval);
int rdma_migrate_id_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_disconnect_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_init_qp_attr_resp(struct rdma_event_channel* channel, void* cmd_in, void* resp_out);
int rdma_get_cm_event_resp(struct rdma_event_channel *channel, void* cmd_in, void* resp_out);
#endif


#endif /* FFROUTER_H */

