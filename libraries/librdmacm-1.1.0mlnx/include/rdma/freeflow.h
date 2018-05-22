#ifndef CM_FREEFLOW_H
#define CM_FREEFLOW_H

#include <infiniband/verbs.h>
#include <infiniband/freeflow-types.h>
#include <pthread.h>

struct sock_with_lock
{
	int sock;
	pthread_mutex_t mutex;
};

void init_sock();
struct sock_with_lock* get_unix_sock(RDMA_FUNCTION_CALL req);
void connect_router(struct sock_with_lock *unix_sock);
void request_router(RDMA_FUNCTION_CALL req, void* req_body, void *rsp, int *rsp_size);

/**
 * FreeFlow Function
 * Connect to router via unix socket
 */

static struct sock_with_lock control_sock[PARALLEL_SIZE];
static struct sock_with_lock write_sock[PARALLEL_SIZE];
static struct sock_with_lock read_sock[PARALLEL_SIZE];
static struct sock_with_lock poll_sock[PARALLEL_SIZE];
static struct sock_with_lock event_sock[PARALLEL_SIZE];

extern struct sock_with_lock event_channel_sock_map[MAP_SIZE];
extern int event_channel_map[MAP_SIZE];

static int sock_initialized = 0;

int recv_fd(struct sock_with_lock *unix_sock);

#endif /* CM_FREEFLOW_H */
