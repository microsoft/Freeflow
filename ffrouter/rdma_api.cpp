// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "rdma_api.h"

unsigned long
gettime_ms()
{
  struct timeval cur_time; 
  gettimeofday (&cur_time, NULL); 
  return (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000); 
}

unsigned long
gettime_us()
{
  struct timeval cur_time; 
  gettimeofday (&cur_time, NULL); 
  return (cur_time.tv_sec * 1000 * 1000) + cur_time.tv_usec; 
}

void
move_qp_to_init(struct ibv_qp *qp)
{
  struct ibv_qp_attr attr;
  int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
  int ret;

  memset(&attr, 0, sizeof(struct ibv_qp_attr)); 
  attr.qp_state        = IBV_QPS_INIT; 
  attr.pkey_index      = 0;
  attr.port_num         = 1;
  attr.qp_access_flags   = IBV_ACCESS_REMOTE_WRITE; 
  ret = ibv_modify_qp(qp, &attr, flags);
  if (ret != 0) {
    fprintf(stderr,"failed to init qp. ret=%d\n", ret);
    ////exit(-1);
  }
}

void move_qp_to_rtr(struct ibv_qp *qp, struct ib_conn_data *dest)
{
  int flags = 0;
  struct ibv_qp_attr attr;
  int ret;

  memset(&attr, 0, sizeof attr);

  attr.qp_state = IBV_QPS_RTR;
  attr.path_mtu = IBV_MTU_1024;
  attr.dest_qp_num = dest->qpn;
  attr.rq_psn = dest->psn;
  attr.max_dest_rd_atomic = 1;
  attr.min_rnr_timer = 0x12;

  attr.ah_attr.dlid = dest->lid;
  attr.ah_attr.sl = 1;
  attr.ah_attr.src_path_bits = 0;
  attr.ah_attr.port_num = IB_PORT_NUM;
  attr.ah_attr.is_global  = 1;
  attr.ah_attr.static_rate = 7;
  attr.ah_attr.grh.dgid = dest->gid;
  attr.ah_attr.grh.sgid_index = 1;
  attr.ah_attr.grh.flow_label = 0;
  attr.ah_attr.grh.hop_limit = 10;
  attr.ah_attr.grh.traffic_class = 1;

  attr.max_dest_rd_atomic = dest->out_reads;

  flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

  ret = ibv_modify_qp(qp, &attr, flags);
  if (ret != 0) {
    fprintf(stderr, "failed to move qp to rtr. ret=%d\n", ret);
    ////exit(-1);
  }
}

void move_qp_to_rts(struct ibv_qp *qp)
{
  int flags = 0;
  struct ibv_qp_attr attr;
  int ret;

  memset(&attr, 0, sizeof attr);

  attr.qp_state = IBV_QPS_RTS;
  attr.timeout = 0x12;
  attr.retry_cnt = 6;
  attr.rnr_retry = 7;
  attr.sq_psn = 0;
  attr.max_rd_atomic = 1;

  flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
  IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;

  ret = ibv_modify_qp(qp, &attr, flags);
  if (ret != 0) {
    fprintf(stderr, "failed to move qp to rts. ret=%d\n", ret);
    ////exit(-1);
  }
}

void
post_receive(struct ib_data *myib)
{
  struct ibv_recv_wr *rr; 
  struct ibv_recv_wr *bad_wr; 
  int ret; 
  
  rr = create_recv_request(myib);
  ret = ibv_post_recv(myib->ib_qp, rr, &bad_wr); 
  if (ret !=0) 
    fprintf (stderr, "failed to post RR. ret=%d\n", ret); 
}

void
post_send(struct ib_data *myib, struct ib_conn_data *dest, int opcode)
{
  struct ibv_send_wr *bad_wr = NULL;
  int ret;
  struct ibv_send_wr *sr;
  struct ibv_sge sge;

  // prepare the scatter/gather entry
  fill_sge(&sge, myib);

  // create sr
  sr = (struct ibv_send_wr *)calloc(1, sizeof(struct ibv_send_wr));
  if (sr == NULL)
  {
    fprintf(stderr, "could not allocate memory for sr\n");
    //exit(-1);
  }
  sr->next = NULL;
  sr->wr_id = 0;
  sr->sg_list = &sge;
  sr->num_sge = 1;
  sr->opcode = (ibv_wr_opcode)opcode;
  sr->send_flags = IBV_SEND_SIGNALED;
  if (opcode != IBV_WR_SEND) {
    sr->wr.rdma.remote_addr = dest->vaddr; 
    sr->wr.rdma.rkey = dest->rkey;
  }

  ret = ibv_post_send (myib->ib_qp, sr, &bad_wr); 

  if (ret) {
   fprintf (stderr, "failed to post SR. ret=%d\n", ret);
   //exit(-1); 
  }
}

void
poll_completion(struct ib_data *myib)
{
  struct ibv_wc *wc; 
  unsigned long start_time_msec; 
  unsigned long cur_time_msec; 
  int poll_result; 

  wc = (ibv_wc*)calloc(1, sizeof (struct ibv_wc));
  if (wc == NULL) {
    fprintf(stderr, "could not callocate memory for wc");
    //exit(-1);
  }

  /* poll the completion for a while before giving up of doing it .. */ 
  start_time_msec = gettime_ms(); 
  do {
    poll_result = ibv_poll_cq (myib->ib_cq, 1, wc); 
    cur_time_msec = gettime_ms(); 
  } while ((poll_result == 0) && ((cur_time_msec - start_time_msec) < POLL_TIMEOUT_MS)); 

  if (poll_result < 0) { 
    fprintf (stderr, "poll CQ failed\n"); 
    //exit(-1);
  } 
  else if (poll_result == 0) { 
    fprintf (stderr, "completion wasn't found in the CQ after timeout\n"); 
    //exit(-1);
  }


  if (poll_result > 0) {
    if (wc[0].status != IBV_WC_SUCCESS) { 
      fprintf (stderr, "got bad completion with status: 0x%x, vendor syndrome: 0x%x\n", wc[0].status, wc[0].vendor_err); 
    }
    else {
      fprintf(stderr, "success! %d completions.\n", poll_result);
    }
  }
 
}

struct ibv_device* 
get_first_device()
{
  struct ibv_device **dev_list = NULL;
  int num_devices;
  dev_list = ibv_get_device_list (&num_devices);
  if (!dev_list) {
      fprintf (stderr, "no devices found\n");
      return NULL;
  }
  else {
    return dev_list[0];
  }
}

void 
setup_ib(struct ib_data *myib)
{
  myib->ib_device = get_first_device();
  if (!myib->ib_device) {
    fprintf(stderr, "could not get device\n"); 
    exit(-1);
  };

  myib->ib_context = ibv_open_device(myib->ib_device);
  
  if (!myib->ib_context) {
    fprintf(stderr, "could not get context\n"); 
    exit(-1);
  };
  
  printf("open device cmd_fd is %d \n",myib->ib_context->cmd_fd);
  fflush(stdout);

  if (ibv_query_device(myib->ib_context, &myib->ib_dev_attr) < 0) {
    fprintf(stderr, "could not get dev attr\n");
    exit(-1);
  }

  if (ibv_query_gid(myib->ib_context, IB_PORT_NUM, 1, &myib->ib_gid) < 0) {
    fprintf(stderr, "could not get gid\n");
    exit(-1);
  }
  
  if (ibv_query_port(myib->ib_context, IB_PORT_NUM, &myib->ib_port_attr) < 0) {
    fprintf(stderr, "could not get port attr\n");
    exit(-1);
  }

  /*myib->ib_cq = ibv_create_cq(myib->ib_context, NUM_REPEATS, NULL, NULL, 0);
  if (!myib->ib_cq) {
    fprintf(stderr, "could not get cq\n"); 
    exit(-1);
  };

  // not supported by standardlized lib
  myib->ib_pd = ibv_alloc_pd(myib->ib_context); 
  if (!myib->ib_pd) {
    fprintf(stderr, "could not get pd\n"); 
    exit(-1);
  };

  bzero(&myib->ib_qp_init_attr, sizeof(myib->ib_qp_init_attr));
  myib->ib_qp_init_attr.qp_type = IBV_QPT_RC;
  myib->ib_qp_init_attr.sq_sig_all = 1;
  myib->ib_qp_init_attr.send_cq = myib->ib_cq;
  myib->ib_qp_init_attr.recv_cq = myib->ib_cq;
  myib->ib_qp_init_attr.cap.max_send_wr = 10;
  myib->ib_qp_init_attr.cap.max_recv_wr = 10;
  myib->ib_qp_init_attr.cap.max_send_sge = 10;
  myib->ib_qp_init_attr.cap.max_recv_sge = 10;

  myib->ib_qp = ibv_create_qp (myib->ib_pd, &myib->ib_qp_init_attr);
  if (!myib->ib_qp) {
    fprintf(stderr, "could not get qp\n"); 
    exit(-1);
  };*/

  /*-------------- FreeFlow -------------*/

  /*myib->msg_size = MSG_SIZE;
  int shm_fd = shm_open("Ring0", O_RDWR, 0666);
  ftruncate(shm_fd, MAX_SIZE);
  myib->ib_buffer = (char*)mmap(0, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

  if (!myib->ib_buffer) {
      fprintf(stderr, "could not allocate memory\n"); 
      //exit(-1);
  }

  myib->ib_mr = ibv_reg_mr(myib->ib_pd, myib->ib_buffer, myib->msg_size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
  if (!myib->ib_mr) {
      fprintf(stderr, "could not get mr\n"); 
      exit(-1);
  }*/
}

void
fill_ib_conn_data(struct ib_data *myib, struct ib_conn_data *my_ib_conn_data)
{
  my_ib_conn_data->lid = myib->ib_port_attr.lid;
  my_ib_conn_data->out_reads = 1;
  my_ib_conn_data->qpn = myib->ib_qp->qp_num;
  my_ib_conn_data->psn = 0;
  my_ib_conn_data->rkey = myib->ib_mr->rkey;
  my_ib_conn_data->vaddr = (uintptr_t)myib->ib_buffer; 
  memcpy(my_ib_conn_data->gid.raw, myib->ib_gid.raw, 16);
  my_ib_conn_data->gid_index = 1;
}

int
receiver_accept_connection()
{
  int sockfd;
  int connfd;
  struct sockaddr_in local;

  /** TCP connection **/
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "could not open socket");
    //exit(-1);
  }
  bzero(&local, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = PORT;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  if (!bind(sockfd, (struct sockaddr *)&local, sizeof(local)) < 0){
    fprintf(stderr, "could not bind socket: %d", errno);
    //exit(-1);
  }
  listen(sockfd, 1); 
  connfd = accept(sockfd, NULL, 0);
  if (connfd < 0) {
    fprintf(stderr, "could not connect: %d", errno);
    //exit(-1);
  }
  else { 
    fprintf(stdout, "connected!\n");
  }
  return connfd;
}

int
sender_make_connection(char *receiver_ip)
{
  int sockfd;
  struct sockaddr_in remote;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "could not open socket");
    //exit(-1);
  }
  bzero(&remote, sizeof(remote));
  remote.sin_family = AF_INET;
  remote.sin_port = PORT;
  remote.sin_addr.s_addr = inet_addr(receiver_ip);
  if (connect(sockfd, (struct sockaddr *)&remote, sizeof(remote)) < 0)
  {
    fprintf(stderr, "could not connect: %d %s\n", errno, strerror(errno));
    //exit(-1);
  }
  else {
    fprintf(stderr, "connected!\n");
  }
  return sockfd;
}

void 
exchange_conn_data(int socket, struct ib_conn_data *my, struct ib_conn_data *remote)
{
  int numWritten = -1; 
  int numRead = -1; 
  
  numWritten = write(socket, my, sizeof(struct ib_conn_data));
  if (numWritten != sizeof(struct ib_conn_data)) {
    fprintf(stderr, "could not write: %d %d %s\n", numWritten, errno, strerror(errno));
    //exit(-1);
  }
  
  numRead = read(socket, remote, sizeof(struct ib_conn_data));
  if (numRead != sizeof(struct ib_conn_data)) {
    fprintf(stderr, "could not read: %d %d %s\n", numRead, errno, strerror(errno));
    //exit(-1);
  }
}

void
wait_for_nudge(int socket)
{
  char buffer;
  if (read(socket, &buffer, sizeof(buffer)) != sizeof(buffer)) {
    fprintf(stderr, "could not get completion message: %d %s\n", errno, strerror(errno));
    //exit(-1);
  }
}

void
send_nudge(int socket)
{
  char buffer;
  if (write(socket, &buffer, sizeof(buffer)) != sizeof(buffer)) {
    fprintf(stderr, "could not send completion message: %d %s\n", errno, strerror(errno));
    //exit(-1);
  }
}

struct ibv_recv_wr *
create_recv_request(struct ib_data *myib)
{
  struct ibv_recv_wr *rr;
  struct ibv_sge sge;

  // prepare the scatter/gather entry
  fill_sge(&sge, myib);

  // creare rr
  rr = (struct ibv_recv_wr *)calloc(1, sizeof(struct ibv_recv_wr));
  if (rr == NULL)
  {
    fprintf(stderr, "could not allocate memory for rr\n");
    //exit(-1);
  }
  rr->next = NULL;
  rr->wr_id = 0;
  rr->sg_list = &sge;
  rr->num_sge = 1;
  return rr;
}

struct ibv_send_wr *
create_send_request(struct ib_data *myib, struct ib_conn_data *dest, int opcode)
{
  struct ibv_send_wr *sr;
  struct ibv_sge sge;

  // prepare the scatter/gather entry
  fill_sge(&sge, myib);

  // create sr
  sr = (struct ibv_send_wr *)calloc(1, sizeof(struct ibv_send_wr));
  if (sr == NULL)
  {
    fprintf(stderr, "could not allocate memory for sr\n");
    //exit(-1);
  }
  sr->next = NULL;
  sr->wr_id = 0;
  sr->sg_list = &sge;
  sr->num_sge = 1;
  sr->opcode = (ibv_wr_opcode)opcode;
  sr->send_flags = IBV_SEND_SIGNALED;
  if (opcode != IBV_WR_SEND) {
    sr->wr.rdma.remote_addr = dest->vaddr; 
    sr->wr.rdma.rkey = dest->rkey;
  }
  
  return sr;
}

void
fill_sge(struct ibv_sge *sge, struct ib_data *myib)
{
  memset (sge, 0, sizeof (struct ibv_sge));
  sge->addr = (uintptr_t) myib->ib_buffer;
  sge->length = myib->msg_size;
  sge->lkey = myib->ib_mr->lkey;
}
