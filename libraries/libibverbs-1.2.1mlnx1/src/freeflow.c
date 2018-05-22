#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <alloca.h>
#include <string.h>

#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>
//#include <asm/cachectl.h>

#include "ibverbs.h"
#include "infiniband/freeflow-types.h"
#include "infiniband/freeflow.h"
#include "infiniband/arch.h"

int comp_channel_map[MAP_SIZE];

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

void* map_lkey_to_mrshm;
struct wr_queue* map_cq_to_wr_queue[MAP_SIZE];
uint32_t map_cq_to_srq[MAP_SIZE];
struct wr_queue* map_srq_to_wr_queue[MAP_SIZE];

/*struct sock_with_lock control_sock[PARALLEL_SIZE];
struct sock_with_lock write_sock[PARALLEL_SIZE];
struct sock_with_lock read_sock[PARALLEL_SIZE];
struct sock_with_lock poll_sock[PARALLEL_SIZE];
struct sock_with_lock event_sock[PARALLEL_SIZE];*/

struct CtrlShmPiece* qp_shm_map[MAP_SIZE];
struct CtrlShmPiece* cq_shm_map[MAP_SIZE];
struct CtrlShmPiece* srq_shm_map[MAP_SIZE];
pthread_mutex_t qp_shm_mtx_map[MAP_SIZE];
pthread_mutex_t cq_shm_mtx_map[MAP_SIZE];
int cq_event_sock_map[MAP_SIZE];
struct sock_with_lock comp_channel_sock_map[MAP_SIZE];

int sock_initialized = 0;
int ffr_client_id;

void init_sock()
{
	if (sock_initialized)
		return;

	sock_initialized = 1;
	const char* cid = getenv("FFR_ID");

	if (!cid)
	{
		perror("Please setup environment variable FFR_ID a unique interger.");
		exit(1);
	}
	ffr_client_id = atoi(cid);

	int i = 0;
        /*for (i = 0; i < PARALLEL_SIZE; i++)
        {
		control_sock[i].sock = -1;
		write_sock[i].sock = -1;
		read_sock[i].sock = -1;
		poll_sock[i].sock = -1;
		event_sock[i].sock = -1;

		pthread_mutex_init(&(control_sock[i].mutex), NULL);
		pthread_mutex_init(&(write_sock[i].mutex), NULL);
		pthread_mutex_init(&(read_sock[i].mutex), NULL);
		pthread_mutex_init(&(poll_sock[i].mutex), NULL);
		pthread_mutex_init(&(event_sock[i].mutex), NULL);

		connect_router(&control_sock[i]);
		connect_router(&write_sock[i]);
		connect_router(&read_sock[i]);
		connect_router(&poll_sock[i]);
		connect_router(&event_sock[i]);
        }*/

	for (i = 0; i < MAP_SIZE; i++)
	{
		comp_channel_sock_map[i].sock = -1;
		pthread_mutex_init(&(comp_channel_sock_map[i].mutex), NULL);
		comp_channel_map[i] = -1;
		map_cq_to_srq[i] = MAP_SIZE + 1;
	}

	map_lkey_to_mrshm = mempool_create();
}

struct sock_with_lock* get_unix_sock(RDMA_FUNCTION_CALL req)
{
    struct sock_with_lock* unix_sock = malloc(sizeof(struct sock_with_lock));
    unix_sock->sock = -1;
    pthread_mutex_init(&(unix_sock->mutex), NULL);
    /*int index = rand() % PARALLEL_SIZE;
    
    switch (req)
	{
		case IBV_GET_CONTEXT:
		case IBV_QUERY_DEV:
		case IBV_EXP_QUERY_DEV:
		case IBV_QUERY_PORT:
		case IBV_ALLOC_PD:
		case IBV_DEALLOC_PD:
		case IBV_CREATE_CQ:
		case IBV_DESTROY_CQ:
		case IBV_CREATE_QP:
		case IBV_DESTROY_QP:
		case IBV_CREATE_SRQ:
		case IBV_MODIFY_SRQ:
		case IBV_DESTROY_SRQ:
		case IBV_REG_MR:
		case IBV_REG_MR_MAPPING:
		case IBV_DEREG_MR:
		case IBV_MODIFY_QP:
		case IBV_QUERY_QP:
		case IBV_CREATE_COMP_CHANNEL:
		case IBV_DESTROY_COMP_CHANNEL:
		case IBV_CREATE_AH:
		case IBV_DESTROY_AH:
		case IBV_CREATE_FLOW:
		case IBV_DESTROY_FLOW:
			unix_sock = &control_sock[index];
			break;
		
		case IBV_POST_SEND:
			unix_sock = &write_sock[index];
			break;

		case IBV_POST_RECV:
			unix_sock = &read_sock[index];
			break;

		case IBV_POST_SRQ_RECV:
			unix_sock = &read_sock[index];
			break;

		case IBV_POLL_CQ:
			unix_sock = &poll_sock[index];
			break;

		case IBV_GET_CQ_EVENT:
		case IBV_ACK_CQ_EVENT:
		case IBV_REQ_NOTIFY_CQ:
			unix_sock = &event_sock[index];
			break;

		default:
			printf("unrecognized request type %d.", req);
			fflush(stdout);
			return NULL;
	}*/

	if (unix_sock->sock < 0)
	{
		connect_router(unix_sock);
	}

	return unix_sock;
}

struct sock_with_lock* get_unix_sock_comp_channel(int comp_channel_fd)
{
    struct sock_with_lock* unix_sock = NULL;
    if (comp_channel_fd >= MAP_SIZE)
    {
        printf("[Error] comp_channel_fd (%d) is larger than MAP_SIZE (%d).\n", comp_channel_fd, MAP_SIZE);
	fflush(stdout);
        return NULL;
    }

    unix_sock = &comp_channel_sock_map[comp_channel_fd];
    if (unix_sock->sock < 0)
    {
        connect_router(unix_sock);
    }

    return unix_sock;
}


void connect_router(struct sock_with_lock *unix_sock)
{
    register int len;
    struct sockaddr_un saun;

    pthread_mutex_lock(&(unix_sock->mutex));

    if ((unix_sock->sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("client: socket");
        exit(1);
    }
    
    memset(&saun, 0, sizeof(saun));
    saun.sun_family = AF_UNIX;
    const char* path = "/freeflow/";
    const char* router_name = getenv("FFR_NAME");
    char* pathname = (char*)malloc(strlen(path)+strlen(router_name));
    strcpy(pathname, path);
    strcat(pathname, router_name);
    strcpy(saun.sun_path, pathname);

    len = sizeof(saun.sun_family) + strlen(saun.sun_path);

    if (connect(unix_sock->sock, &saun, len) < 0) {
        perror("Cannot connect client.");
        unix_sock->sock = -1;
    }

    pthread_mutex_unlock(&(unix_sock->mutex));
}

void request_router(RDMA_FUNCTION_CALL req, void* req_body, void *rsp, int *rsp_size)
{
	int mapped_fd = -1;
	struct sock_with_lock* unix_sock = NULL;

	if (req == IBV_GET_CQ_EVENT)
	{
		unix_sock = get_unix_sock_comp_channel(((struct IBV_GET_CQ_EVENT_REQ *)req_body)->fd);
	}
	else
	{
		unix_sock = get_unix_sock(req);
	}

	if (unix_sock == NULL || unix_sock->sock < 0)
	{
		perror("invalid socket.");
		return;
	}

	struct FfrRequestHeader header;
	const char* cid = getenv("FFR_ID");

	if (!cid)
	{
		perror("Please setup environment variable FFR_ID a unique interger.");
		exit(1);
	}

	header.client_id = atoi(cid);
	header.func = req;
	header.body_size = 0;
	int fixed_size_rsp = 1;
        int close_sock = 1;
	
	switch (req)
	{
		case IBV_GET_CONTEXT:
			*rsp_size = sizeof(struct IBV_GET_CONTEXT_RSP);
			break;

		case IBV_QUERY_DEV:
			*rsp_size = sizeof(struct IBV_QUERY_DEV_RSP);
			break;

		case IBV_EXP_QUERY_DEV:
			*rsp_size = sizeof(struct IBV_EXP_QUERY_DEV_RSP);
			header.body_size = sizeof(struct IBV_EXP_QUERY_DEV_REQ);
			break;

		case IBV_QUERY_PORT:
			*rsp_size = sizeof(struct IBV_QUERY_PORT_RSP);
			header.body_size = sizeof(struct IBV_QUERY_PORT_REQ);
			break;

		case IBV_ALLOC_PD:
			*rsp_size = sizeof(struct IBV_ALLOC_PD_RSP);
			break;

		case IBV_DEALLOC_PD:
			*rsp_size = sizeof(struct IBV_DEALLOC_PD_RSP);
			header.body_size = sizeof(struct IBV_DEALLOC_PD_REQ);
			break;

		case IBV_CREATE_CQ:
			*rsp_size = sizeof(struct IBV_CREATE_CQ_RSP);
			header.body_size = sizeof(struct IBV_CREATE_CQ_REQ);
			break;

		case IBV_DESTROY_CQ:
			*rsp_size = sizeof(struct IBV_DESTROY_CQ_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_CQ_REQ);
			break;

		case IBV_REQ_NOTIFY_CQ:
			*rsp_size = sizeof(struct IBV_REQ_NOTIFY_CQ_RSP);
			header.body_size = sizeof(struct IBV_REQ_NOTIFY_CQ_REQ);
			break;

		case IBV_CREATE_QP:
			*rsp_size = sizeof(struct IBV_CREATE_QP_RSP);
			header.body_size = sizeof(struct IBV_CREATE_QP_REQ);
			break;

		case IBV_DESTROY_QP:
			*rsp_size = sizeof(struct IBV_DESTROY_QP_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_QP_REQ);
			break;

		case IBV_REG_MR:
			*rsp_size = sizeof(struct IBV_REG_MR_RSP);
			header.body_size = sizeof(struct IBV_REG_MR_REQ);
			break;

		case IBV_REG_MR_MAPPING:
			*rsp_size = sizeof(struct IBV_REG_MR_MAPPING_RSP);
			header.body_size = sizeof(struct IBV_REG_MR_MAPPING_REQ);
			break;

		case IBV_DEREG_MR:
			*rsp_size = sizeof(struct IBV_DEREG_MR_RSP);
			header.body_size = sizeof(struct IBV_DEREG_MR_REQ);
			break;

		case IBV_MODIFY_QP:
			*rsp_size = sizeof(struct IBV_MODIFY_QP_RSP);
			header.body_size = sizeof(struct IBV_MODIFY_QP_REQ);
			break;

		case IBV_QUERY_QP:
			*rsp_size = sizeof(struct IBV_QUERY_QP_RSP);
			header.body_size = sizeof(struct IBV_QUERY_QP_REQ);
			break;
		
		case IBV_POST_SEND:
			*rsp_size = sizeof(struct IBV_POST_SEND_RSP);
			header.body_size = ((struct IBV_POST_SEND_REQ*)req_body)->wr_size;
			req_body = ((struct IBV_POST_SEND_REQ*)req_body)->wr;
			break;

		case IBV_POST_RECV:
			*rsp_size = sizeof(struct IBV_POST_RECV_RSP);
			header.body_size = ((struct IBV_POST_RECV_REQ*)req_body)->wr_size;
			req_body = ((struct IBV_POST_RECV_REQ*)req_body)->wr;
			break;

		case IBV_POST_SRQ_RECV:
			*rsp_size = sizeof(struct IBV_POST_SRQ_RECV_RSP);
			header.body_size = ((struct IBV_POST_SRQ_RECV_REQ*)req_body)->wr_size;
			req_body = ((struct IBV_POST_SRQ_RECV_REQ*)req_body)->wr;
			break;

		case IBV_POLL_CQ:
			fixed_size_rsp = 0;
			header.body_size = sizeof(struct IBV_POLL_CQ_REQ);
			break;

		case IBV_CREATE_COMP_CHANNEL:
			*rsp_size = sizeof(struct IBV_CREATE_COMP_CHANNEL_RSP);
			break;

		case IBV_DESTROY_COMP_CHANNEL:
			*rsp_size = sizeof(struct IBV_DESTROY_COMP_CHANNEL_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_COMP_CHANNEL_REQ);
			break;

		case IBV_GET_CQ_EVENT:
			*rsp_size = sizeof(struct IBV_GET_CQ_EVENT_RSP);
			header.body_size = sizeof(struct IBV_GET_CQ_EVENT_REQ);
			close_sock = 0;
			break;

		case IBV_ACK_CQ_EVENT:
			*rsp_size = sizeof(struct IBV_ACK_CQ_EVENT_RSP);
			header.body_size = sizeof(struct IBV_ACK_CQ_EVENT_REQ);
			break;

		case IBV_CREATE_AH:
			*rsp_size = sizeof(struct IBV_CREATE_AH_RSP);
			header.body_size = sizeof(struct IBV_CREATE_AH_REQ);
			break;

		case IBV_DESTROY_AH:
			*rsp_size = sizeof(struct IBV_DESTROY_AH_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_AH_REQ);
			break;

		case IBV_CREATE_FLOW:
			*rsp_size = sizeof(struct IBV_CREATE_FLOW_RSP);
			header.body_size = sizeof(struct IBV_CREATE_FLOW_REQ);
			break;

		case IBV_DESTROY_FLOW:
			*rsp_size = sizeof(struct IBV_DESTROY_FLOW_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_FLOW_REQ);
			break;

		case IBV_CREATE_SRQ:
			*rsp_size = sizeof(struct IBV_CREATE_SRQ_RSP);
			header.body_size = sizeof(struct IBV_CREATE_SRQ_REQ);
			break;

		case IBV_MODIFY_SRQ:
			*rsp_size = sizeof(struct IBV_MODIFY_SRQ_RSP);
			header.body_size = sizeof(struct IBV_MODIFY_SRQ_REQ);
			break;

		case IBV_DESTROY_SRQ:
			*rsp_size = sizeof(struct IBV_DESTROY_SRQ_RSP);
			header.body_size = sizeof(struct IBV_DESTROY_SRQ_REQ);
			break;

		default:
			goto end;
	}

	if (!close_sock)
	{
		pthread_mutex_lock(&(unix_sock->mutex));
	}
	int n;
	if ((n = write(unix_sock->sock, &header, sizeof(header))) < sizeof(header))
	{
		if (n < 0)
		{
			printf("router disconnected in writing req header.\n");
			fflush(stdout);
			unix_sock->sock = -1;
		}
		else
		{
			printf("partial write.\n");
		}
	}

	if (header.body_size > 0)
	{
		if ((n = write(unix_sock->sock, req_body, header.body_size)) < header.body_size)
		{
			if (n < 0)
			{
				printf("router disconnected in writing req body.\n");
				fflush(stdout);
				unix_sock->sock = -1;
			}
			else
			{
				printf("partial write.\n");
			}
		}	
	}

	if (req == IBV_CREATE_COMP_CHANNEL)
	{
		mapped_fd = recv_fd(unix_sock);
	}

	if (!fixed_size_rsp)
	{
		struct FfrResponseHeader rsp_hr;
		int bytes = 0;
		while(bytes < sizeof(rsp_hr)) {
			n = read(unix_sock->sock, ((char *)&rsp_hr) + bytes, sizeof(rsp_hr) - bytes);
			if (n < 0)
			{
				printf("router disconnected when reading rsp.\n");
				fflush(stdout);
				unix_sock->sock = -1;
				goto end;
			}
			bytes = bytes + n;
		}

		*rsp_size = rsp_hr.rsp_size;
	}

	int bytes = 0;
	while(bytes < *rsp_size)
	{
		n = read(unix_sock->sock, (char*)rsp + bytes, *rsp_size - bytes);
		if (n < 0)
		{
			printf("router disconnected when reading rsp.\n");
			fflush(stdout);
			unix_sock->sock = -1;
			goto end;
		}

		bytes = bytes + n;
	}

	if (req == IBV_CREATE_COMP_CHANNEL)
	{
		printf("[INFO Comp Channel] rsp.ec.fd = %d, mapped_fd = %d\n", ((struct IBV_CREATE_COMP_CHANNEL_RSP*)rsp)->fd, mapped_fd);
		fflush(stdout);

		comp_channel_map[mapped_fd] = ((struct IBV_CREATE_COMP_CHANNEL_RSP*)rsp)->fd;
		((struct IBV_CREATE_COMP_CHANNEL_RSP*)rsp)->fd = mapped_fd;
	}

end:
        if (close_sock)
	{
		close(unix_sock->sock);
                pthread_mutex_destroy(&unix_sock->mutex);
		free(unix_sock);
	}
	else
		pthread_mutex_unlock(&(unix_sock->mutex));
}

/* Fast datapath: this function itself is not thread safe! */
void request_router_shm(struct CtrlShmPiece *csp)
{
	//struct timespec st, et;
	//clock_gettime(CLOCK_REALTIME, &st);
    //int time_us = 0;

    for (;;)
	{
        //rmb();
		switch (csp->state)
		{
			case IDLE:
                wmb();
				csp->state = REQ_DONE;
                //wc_wmb();
                //mem_flush((void*)&csp->state, sizeof(enum CtrlChannelState));
				//clock_gettime(CLOCK_REALTIME, &st);
				break;

			case REQ_DONE:
				break;

			case RSP_DONE:
				//!!! Must reset the state to IDLE outside this function
                
				//wc_wmb();
                //mem_flush((void*)&csp->state, sizeof(enum CtrlChannelState));
				//clock_gettime(CLOCK_REALTIME, &et);
				//	printf("REQ_DONE tv_sec=%d, tv_nsec=%d\n", st.tv_sec, st.tv_nsec);
				//	fflush(stdout);
				//	printf("RSP_DONE tv_sec=%d, tv_nsec=%d\n", et.tv_sec, et.tv_nsec);
				//	fflush(stdout);
				//goto end;
				return;
		}

		/*clock_gettime(CLOCK_REALTIME, &et);
        	time_us = (et.tv_sec - st.tv_sec) * 1000000 + (et.tv_nsec - st.tv_nsec) / 1000;
		if (time_us > 1000 * 1000)
		{
			printf("request_router_shm time out\n");
			fflush(stdout);
			goto end;
		}*/
	}
//end:
//	pthread_mutex_unlock(csp_mtx);
}


int recv_fd(struct sock_with_lock *unix_sock)
{
        ssize_t     size;
        struct msghdr   msg;
        struct iovec    iov;
        union {
            struct cmsghdr  cmsghdr;
            char        control[CMSG_SPACE(sizeof (int))];
        } cmsgu;
        struct cmsghdr  *cmsg;
	char            buf[2];
        int fd = -1;

        iov.iov_base = buf;
        iov.iov_len = 2;

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);
        size = recvmsg (unix_sock->sock, &msg, 0);
        if (size < 0) {
            perror ("recvmsg");
            exit(1);
        }
        cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
            if (cmsg->cmsg_level != SOL_SOCKET) {
                fprintf (stderr, "invalid cmsg_level %d\n",
                     cmsg->cmsg_level);
                exit(1);
            }
            if (cmsg->cmsg_type != SCM_RIGHTS) {
                fprintf (stderr, "invalid cmsg_type %d\n",
                     cmsg->cmsg_type);
                exit(1);
            }

            fd = *((int *) CMSG_DATA(cmsg));
            printf ("received fd %d\n", fd);
        } else
            fd = -1;

	return(fd);  
}


