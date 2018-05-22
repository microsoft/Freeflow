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

#include <infiniband/freeflow-types.h>
#include "rdma/freeflow.h"

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

static struct cmsghdr   *cmptr = NULL;      /* malloc'ed first time */

struct sock_with_lock event_channel_sock_map[MAP_SIZE];
int event_channel_map[MAP_SIZE];

void init_sock()
{
	if (sock_initialized)
		return;

	sock_initialized = 1;

	int i = 0;
        for (i = 0; i < PARALLEL_SIZE; i++)
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
        }

	for (i = 0; i < MAP_SIZE; i++)
	{
		event_channel_sock_map[i].sock = -1;
		pthread_mutex_init(&(event_channel_sock_map[i].mutex), NULL);
		event_channel_map[i] = -1;
	}
}

struct sock_with_lock* get_unix_sock(RDMA_FUNCTION_CALL req)
{
    struct sock_with_lock* unix_sock = NULL;
    int index = rand() % PARALLEL_SIZE;
    
    switch (req)
	{
		case CM_CREATE_EVENT_CHANNEL:
		case CM_DESTROY_EVENT_CHANNEL:
		case CM_CREATE_ID:
		case CM_BIND_IP:
		case CM_BIND:
		case CM_QUERY_ROUTE:
		case CM_LISTEN:
		case CM_RESOLVE_IP:
		case CM_RESOLVE_ADDR:
		case CM_UCMA_QUERY_ADDR:
		case CM_UCMA_QUERY_GID:
		case CM_DESTROY_ID:
		case CM_RESOLVE_ROUTE:
		case CM_UCMA_QUERY_PATH:
		case CM_UCMA_PROCESS_CONN_RESP:
		case CM_INIT_QP_ATTR:
		case CM_CONNECT:
		case CM_ACCEPT:
		case CM_SET_OPTION:
		case CM_MIGRATE_ID:
		case CM_DISCONNECT:
			unix_sock = &control_sock[index];
			break;

		case CM_GET_EVENT:
			unix_sock = &event_sock[index];
			break;
		
		default:
			printf("unrecognized request type %d.", req);
			fflush(stdout);
			return NULL;
	}

	if (unix_sock->sock < 0)
	{
		connect_router(unix_sock);
	}

	return unix_sock;
}

struct sock_with_lock* get_unix_sock_event_channel(int event_channel_fd)
{
    struct sock_with_lock* unix_sock = NULL;
    if (event_channel_fd >= MAP_SIZE)
    {
        printf("[Error] comp_channel_fd (%d) is larger than MAP_SIZE (%d).\n", event_channel_fd, MAP_SIZE);
	fflush(stdout);
        return NULL;
    }

    unix_sock = &(event_channel_sock_map[event_channel_fd]);
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

	if (req == CM_GET_EVENT)
	{
		unix_sock = get_unix_sock_event_channel(((struct CM_GET_EVENT_REQ *)req_body)->ec.fd);
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
	header.client_id = atoi(cid);
	header.func = req;
	header.body_size = 0;
	int fixed_size_rsp = 1;
	
	switch (req)
	{
		case CM_CREATE_EVENT_CHANNEL:
			*rsp_size = sizeof(struct CM_CREATE_EVENT_CHANNEL_RSP);
			break;

		case CM_DESTROY_EVENT_CHANNEL:
			*rsp_size = sizeof(struct CM_DESTROY_EVENT_CHANNEL_RSP);
			header.body_size = sizeof(struct CM_DESTROY_EVENT_CHANNEL_REQ);
			break;

		case CM_CREATE_ID:
			*rsp_size = sizeof(struct CM_CREATE_ID_RSP);
			header.body_size = sizeof(struct CM_CREATE_ID_REQ);
			break;

		case CM_BIND_IP:
			*rsp_size = sizeof(struct CM_BIND_IP_RSP);
			header.body_size = sizeof(struct CM_BIND_IP_REQ);
			break;

		case CM_BIND:
			*rsp_size = sizeof(struct CM_BIND_RSP);
			header.body_size = sizeof(struct CM_BIND_REQ);
			break;

		case CM_GET_EVENT:
			*rsp_size = sizeof(struct CM_GET_EVENT_RSP);
			header.body_size = sizeof(struct CM_GET_EVENT_REQ);
			break;

		case CM_QUERY_ROUTE:
			*rsp_size = sizeof(struct CM_QUERY_ROUTE_RSP);
			header.body_size = sizeof(struct CM_QUERY_ROUTE_REQ);
			break;

		case CM_LISTEN:
			*rsp_size = sizeof(struct CM_LISTEN_RSP);
			header.body_size = sizeof(struct CM_LISTEN_REQ);
			break;

		case CM_RESOLVE_IP:
			*rsp_size = sizeof(struct CM_RESOLVE_IP_RSP);
			header.body_size = sizeof(struct CM_RESOLVE_IP_REQ);
			break;

		case CM_RESOLVE_ADDR:
			*rsp_size = sizeof(struct CM_RESOLVE_ADDR_RSP);
			header.body_size = sizeof(struct CM_RESOLVE_ADDR_REQ);
			break;

		case CM_UCMA_QUERY_ADDR:
			*rsp_size = sizeof(struct CM_UCMA_QUERY_ADDR_RSP);
			header.body_size = sizeof(struct CM_UCMA_QUERY_ADDR_REQ);
			break;

		case CM_UCMA_QUERY_GID:
			*rsp_size = sizeof(struct CM_UCMA_QUERY_GID_RSP);
			header.body_size = sizeof(struct CM_UCMA_QUERY_GID_REQ);
			break;

		case CM_DESTROY_ID:
			*rsp_size = sizeof(struct CM_DESTROY_ID_RSP);
			header.body_size = sizeof(struct CM_DESTROY_ID_REQ);
			break;

		case CM_RESOLVE_ROUTE:
			*rsp_size = sizeof(struct CM_RESOLVE_ROUTE_RSP);
			header.body_size = sizeof(struct CM_RESOLVE_ROUTE_REQ);
			break;

		case CM_UCMA_QUERY_PATH:
			*rsp_size = sizeof(struct CM_UCMA_QUERY_PATH_RSP);
			header.body_size = sizeof(struct CM_UCMA_QUERY_PATH_REQ);
			break;

		case CM_UCMA_PROCESS_CONN_RESP:
			*rsp_size = sizeof(struct CM_UCMA_PROCESS_CONN_RESP_RSP);
			header.body_size = sizeof(struct CM_UCMA_PROCESS_CONN_RESP_REQ);
			break;

		case CM_INIT_QP_ATTR:
			*rsp_size = sizeof(struct CM_INIT_QP_ATTR_RSP);
			header.body_size = sizeof(struct CM_INIT_QP_ATTR_REQ);
			break;

		case CM_CONNECT:
			*rsp_size = sizeof(struct CM_CONNECT_RSP);
			header.body_size = sizeof(struct CM_CONNECT_REQ);
			break;

		case CM_ACCEPT:
			*rsp_size = sizeof(struct CM_ACCEPT_RSP);
			header.body_size = sizeof(struct CM_ACCEPT_REQ);
			break;

		case CM_SET_OPTION:
			*rsp_size = sizeof(struct CM_SET_OPTION_RSP);
			header.body_size = sizeof(struct CM_SET_OPTION_REQ);
			break;

		case CM_MIGRATE_ID:
			*rsp_size = sizeof(struct CM_MIGRATE_ID_RSP);
			header.body_size = sizeof(struct CM_MIGRATE_ID_REQ);
			break;

		case CM_DISCONNECT:
			*rsp_size = sizeof(struct CM_DISCONNECT_RSP);
			header.body_size = sizeof(struct CM_DISCONNECT_REQ);
			break;

		default:
			goto end;
	}

	pthread_mutex_lock(&(unix_sock->mutex));
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

	if (req == CM_CREATE_EVENT_CHANNEL)
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

	if (req == CM_CREATE_EVENT_CHANNEL)
	{
		printf("[INFO] rsp.ec.fd = %d, mapped_fd = %d\n", ((struct CM_CREATE_EVENT_CHANNEL_RSP*)rsp)->ec.fd, mapped_fd);
		fflush(stdout);

		//event_channel_map[((struct CM_CREATE_EVENT_CHANNEL_RSP*)rsp)->ec.fd] = ((struct CM_CREATE_EVENT_CHANNEL_RSP*)rsp)->ec.fd;

		event_channel_map[mapped_fd] = ((struct CM_CREATE_EVENT_CHANNEL_RSP*)rsp)->ec.fd;
		((struct CM_CREATE_EVENT_CHANNEL_RSP*)rsp)->ec.fd = mapped_fd;
	}
end:
	pthread_mutex_unlock(&(unix_sock->mutex));
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

/*
int send_fd(struct sock_with_lock *unix_sock, int fd)
{	
	struct CREDSTRUCT   *credp;
	struct cmsghdr      *cmp;
	struct iovec        iov[1];
	struct msghdr       msg;
	char                buf[2]; // send_fd/recv_ufd 2-byte protocol

	iov[0].iov_base = buf;
	iov[0].iov_len =  2;
	msg.msg_iov     = iov;
	msg.msg_iovlen =  1;
	msg.msg_name    = NULL;
	msg.msg_namelen = 0;
	msg.msg_flags = 0;

	if (cmptr == NULL && (cmptr = malloc(CONTROLLEN)) == NULL)
		return(-1);

	msg.msg_control    = cmptr;
	msg.msg_controllen = CONTROLLEN;
	cmp = cmptr;
	cmp->cmsg_level =  SOL_SOCKET;
	cmp->cmsg_type   = SCM_RIGHTS;
	cmp->cmsg_len    = RIGHTSLEN;
	*(int *)CMSG_DATA(cmp) = fd;   // the fd to pass

	cmp = CMSG_NXTHDR(&msg, cmp);
	cmp->cmsg_level =  SOL_SOCKET;
	cmp->cmsg_type   = SCM_CREDTYPE;
	cmp->cmsg_len    = CREDSLEN;
	credp = (struct CREDSTRUCT *)CMSG_DATA(cmp);
#if defined(SCM_CREDENTIALS)
	credp->uid = geteuid();
	credp->gid = getegid();
	credp->pid = getpid();
#endif
        buf[1] = 0;     // zero status means OK

    buf[0] = 0;         // null byte flag to recv_ufd()
    if (sendmsg(fd, &msg, 0) != 2)
        return(-1);

    return(0);
}
*/
