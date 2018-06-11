// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef RDMA_API_H
#define RDMA_API_H

#include "constant.h"

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/arch.h>
#include <infiniband/driver.h>
#include <infiniband/driver_exp.h>
#include <infiniband/kern-abi.h>
#include <infiniband/kern-abi_exp.h>
//#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <sys/time.h>
#include <netinet/in.h>
#include <malloc.h>
#include <errno.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>
#include <pthread.h>
#include <sys/shm.h>

struct ib_conn_data { 
  int         lid;
  int         out_reads;
  int         qpn;
  int         psn;
  unsigned      rkey;
  unsigned long long    vaddr;
  union ibv_gid     gid;
  unsigned      srqn;
  int       gid_index;
};

struct ib_data {
  struct ibv_device *ib_device;
  struct ibv_context *ib_context;
  union ibv_gid ib_gid;
  struct ibv_device_attr ib_dev_attr;
  struct ibv_port_attr ib_port_attr;

  /* not used */
  struct ibv_pd *ib_pd;
  struct ibv_qp *ib_qp;
  struct ibv_cq *ib_cq;
  struct ibv_mr *ib_mr;
  int msg_size;
  char   *ib_buffer;
};

void move_qp_to_init(struct ibv_qp *qp);

void move_qp_to_rtr(struct ibv_qp *qp, struct ib_conn_data *dest);

void move_qp_to_rts(struct ibv_qp *qp);

void post_receive(struct ib_data *myib);

void post_send(struct ib_data *myib, struct ib_conn_data *dest, int opcode);

void poll_completion(struct ib_data *myib);

struct ibv_device* get_first_device();

void setup_ib(struct ib_data *myib);

void fill_ib_conn_data(struct ib_data *myib, struct ib_conn_data *my_ib_conn_data);

int receiver_accept_connection();

int sender_make_connection(char *receiver_ip);

void exchange_conn_data(int socket, struct ib_conn_data *my, struct ib_conn_data *remote);

void wait_for_nudge(int socket);

void send_nudge(int socket);

struct ibv_recv_wr * create_recv_request(struct ib_data *myib);

struct ibv_send_wr * create_send_request(struct ib_data *myib, struct ib_conn_data *dest, int opcode);

void fill_sge(struct ibv_sge *sge, struct ib_data *myib);

#endif /* RDMA_API_H */
