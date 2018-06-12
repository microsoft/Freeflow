/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <errno.h>

#include "indexer.h"
#include "types.h"

struct socket_calls {
    int (*socket)(int domain, int type, int protocol);
    int (*bind)(int socket, const struct sockaddr *addr, socklen_t addrlen);
    int (*listen)(int socket, int backlog);
    int (*accept)(int socket, struct sockaddr *addr, socklen_t *addrlen);
    int (*accept4)(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
    int (*connect)(int socket, const struct sockaddr *addr, socklen_t addrlen);
    int (*getpeername)(int socket, struct sockaddr *addr, socklen_t *addrlen);
    int (*getsockname)(int socket, struct sockaddr *addr, socklen_t *addrlen);
    int (*setsockopt)(int socket, int level, int optname,
              const void *optval, socklen_t optlen);
    int (*getsockopt)(int socket, int level, int optname,
              void *optval, socklen_t *optlen);
    int (*fcntl)(int socket, int cmd, ... /* arg */);
    int (*close)(int socket);
};

static struct socket_calls real;

static struct index_map idm;
static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

static char* unix_socket_path;
static int client_id;
static uint32_t prefix_ip = 0;
static uint32_t prefix_mask = 0;
static uint8_t debug_flag = 0;

enum fd_type {
    fd_normal,
    fd_fsocket
};

struct fd_info {
    enum fd_type type;
    int overlay_fd;
    int host_fd;
};

static void fd_store(int app_fd, int overlay_fd, int host_fd, enum fd_type type)
{
    struct fd_info *fdi = idm_at(&idm, app_fd);
    if (overlay_fd >= 0) {
        fdi->overlay_fd = overlay_fd;
    }
    if (host_fd >= 0) {
        fdi->host_fd = host_fd;
    }
    if (fdi->type >= 0) {
        fdi->type = type;
    }
    if (debug_flag) {
        printf("%d: fd_store(%d, %d, %d, %d)\n", getpid(), app_fd, overlay_fd, host_fd, type);
        fflush(stdout);
    }
}

static inline void log_error(char* info) {
    if (debug_flag) {
        printf("ERROR %d: %s", getpid(), info);
        printf("errno: %d\n", errno);
        fflush(stdout);
    }
    return;
}

static inline enum fd_type fd_get_type(int app_fd)
{
    struct fd_info *fdi;
    fdi = idm_lookup(&idm, app_fd);
    if (fdi) {
        return fdi->type;
    } 
    return fd_normal;
}

static inline int fd_get_host_fd(int app_fd)
{
    struct fd_info *fdi;
    fdi = idm_lookup(&idm, app_fd);
    if (fdi) {
        return fdi->host_fd;
    } 
    return -1;
}

static inline int fd_get_overlay_fd(int app_fd)
{
    struct fd_info *fdi;
    fdi = idm_lookup(&idm, app_fd);
    if (fdi) {
        return fdi->overlay_fd;
    }
    return -1;
}

static inline int fd_get_host_bind_addr(int app_fd, struct sockaddr_in* addr)
{
    int host_fd = fd_get_host_fd(app_fd);
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int ret = real.getsockname(host_fd, (struct sockaddr*)addr, &addrlen);

    if (debug_flag) {
        printf("%d: host bind address: %s:%hu\n", getpid(), inet_ntoa(addr->sin_addr), htons(addr->sin_port));
    }

    return ret;
}

static inline bool is_on_overlay(const struct sockaddr_in* addr) {

    if (debug_flag) {
        struct in_addr addr_tmp;
        addr_tmp.s_addr = prefix_ip;
        struct in_addr mask_tmp;
        mask_tmp.s_addr = prefix_mask;
        printf("%d: is %s ", getpid(), inet_ntoa(addr->sin_addr));
        printf("on overlay %s", inet_ntoa(addr_tmp));
        printf("/%s?\n", inet_ntoa(mask_tmp));
        fflush(stdout);
    }
    if (addr->sin_family == AF_INET6) {
        const uint8_t *bytes = ((const struct sockaddr_in6 *)addr)->sin6_addr.s6_addr;
        bytes += 12;
        struct in_addr addr4 = { *(const in_addr_t *)bytes };
        return ((addr4.s_addr & prefix_mask) == (prefix_ip & prefix_mask));;
    }
    return ((addr->sin_addr.s_addr & prefix_mask) == (prefix_ip & prefix_mask));
}

void getenv_options(void)
{
    const char* path = "/freeflow/";
    const char* router_name = getenv("FFR_NAME");
    if (!router_name) {
        // printf("INFO: FFR_NAME is not set. Using \"ffrouter\".\n");
        router_name = "ffrouter";
    }
    unix_socket_path = (char*)malloc(strlen(path)+strlen(router_name));
    strcpy(unix_socket_path, path);
    strcat(unix_socket_path, router_name);

    const char* prefix = getenv("VNET_PREFIX");
    if (prefix) {
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
    }
    if (prefix_ip == 0 && prefix_mask == 0) {
        printf("WARNING: VNET_PREFIX is not set. Using 0.0.0.0/0.\n");
        printf("All connections are treated as virtual network connections.\n");
    }

    const char* cid = getenv("FFR_ID");
    if (!cid) {
        client_id = -1;
    }
    else {
        client_id = atoi(cid);
    }

    const char* debug_env = getenv("DEBUG_FREEFLOW");
    if (debug_env) {
        debug_flag = atoi(debug_env);
    }

    return;
}

static void init_preload(void)
{
    static int init;
    // quick check without lock
    if (init) {
        return;
    }

    pthread_mutex_lock(&mut);
    if (init) {
        goto out;
    }
        
    real.socket = dlsym(RTLD_NEXT, "socket");
    real.bind = dlsym(RTLD_NEXT, "bind");
    real.listen = dlsym(RTLD_NEXT, "listen");
    real.accept = dlsym(RTLD_NEXT, "accept");
    real.accept4 = dlsym(RTLD_NEXT, "accept4");
    real.connect = dlsym(RTLD_NEXT, "connect");
    real.getpeername = dlsym(RTLD_NEXT, "getpeername");
    real.getsockname = dlsym(RTLD_NEXT, "getsockname");
    real.setsockopt = dlsym(RTLD_NEXT, "setsockopt");
    real.getsockopt = dlsym(RTLD_NEXT, "getsockopt");
    real.fcntl = dlsym(RTLD_NEXT, "fcntl");
    real.close = dlsym(RTLD_NEXT, "close");
    
    getenv_options();
    init = 1;
out:
    pthread_mutex_unlock(&mut);
}

int send_fd(int unix_sock, int fd)
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
        int *fd_p = (int *) CMSG_DATA(cmsg);
        *fd_p = fd;
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        //printf ("not passing fd\n");
    }

    size = sendmsg(unix_sock, &msg, 0);

    if (size < 0) {
        log_error ("recvmsg error");
    }
    return size;
}

int recv_fd(int unix_sock)
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
    size = recvmsg (unix_sock, &msg, 0);
    if (size < 0) {
        log_error ("recvmsg error");
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

int connect_router() {
    if (debug_flag) {
        printf("%d: connect router...\n", getpid());
        fflush(stdout);
    }
    int unix_sock = real.socket(AF_UNIX, SOCK_STREAM, 0);
    if (unix_sock < 0) {
        log_error("Cannot create unix socket.\n");
        return -1;
    }
    struct sockaddr_un saun;
    memset(&saun, 0, sizeof(saun));
    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, unix_socket_path);
    int len = sizeof(saun.sun_family) + strlen(saun.sun_path);
    if (real.connect(unix_sock, (struct sockaddr*)&saun, len) < 0) {
        log_error("Cannot connect router.\n");
        real.close(unix_sock);
        return -1;
    }
    return unix_sock;
}

int socket(int domain, int type, int protocol)
{
    init_preload();
    struct fd_info *fdi = calloc(1, sizeof(*fdi));

    int overlay_fd = real.socket(domain, type, protocol);
    
    if (debug_flag) {
        printf("%d: socket(%d, %d, %d) --> %d\n", getpid(), domain, type, protocol, overlay_fd);
        fflush(stdout);
    }

    if ((domain == AF_INET || domain == AF_INET6) && (type & SOCK_STREAM) && (!protocol || protocol == IPPROTO_TCP)) {
        int n, unix_sock = connect_router();
        if (unix_sock < 0) {
            log_error("socket() fails.\n");
            goto normal;
        }

        struct FfrRequestHeader req_header;
        req_header.client_id = client_id; // we don't need this for now
        req_header.func = SOCKET_SOCKET;
        req_header.body_size = sizeof(struct SOCKET_SOCKET_REQ);
        if ((n = write(unix_sock, &req_header, sizeof(req_header))) < sizeof(req_header)) {
            printf("%d: socket() write header fails.\n",  getpid());
            real.close(unix_sock);
            goto normal;
        }

        struct SOCKET_SOCKET_REQ req_body;
        req_body.domain = domain;
        req_body.type = type;
        req_body.protocol = protocol;
        if ((n = write(unix_sock, &req_body, req_header.body_size)) < req_header.body_size) {
            log_error("socket() write body fails.\n");
            real.close(unix_sock);
            goto normal;
        }

        int bytes = 0, rsp_size = sizeof(struct SOCKET_SOCKET_RSP);
        struct SOCKET_SOCKET_RSP rsp;
        while(bytes < rsp_size) {
            n = read(unix_sock, (char*)&rsp + bytes, rsp_size - bytes);
            if (n < 0) {
                log_error("socket() read fails.\n");
                real.close(unix_sock);
                goto normal;
            }
            bytes = bytes + n;
        }        
        if (rsp.ret < 0) {
            log_error("socket() fails on router.\n");
            goto normal;
        }

        int host_fd = recv_fd(unix_sock);
        real.close(unix_sock);

        idm_set(&idm, overlay_fd, fdi);
        fd_store(overlay_fd, overlay_fd, host_fd, fd_fsocket);
        return overlay_fd;
    }

normal:
    // normal socket
    idm_set(&idm, overlay_fd, fdi);
    fd_store(overlay_fd, overlay_fd, -1, fd_normal);
    return overlay_fd;
}


int bind(int socket, const struct sockaddr *addr, socklen_t addrlen)
{
    if (debug_flag) {
        printf("%d: bind(%d, %s, %hu)\n", getpid(), socket, inet_ntoa(((struct sockaddr_in*)addr)->sin_addr), htons(((struct sockaddr_in*)addr)->sin_port));
        fflush(stdout);
    }

    if (fd_get_type(socket) == fd_normal) {
        return real.bind(socket, addr, addrlen);
    }

    int overlay_fd = fd_get_overlay_fd(socket);
    int ret = real.bind(overlay_fd, addr, addrlen);
    if (ret < 0) {
        return ret;
    }

    if (!is_on_overlay((struct sockaddr_in*)addr) && ((struct sockaddr_in*)addr)->sin_addr.s_addr != INADDR_ANY) {
        // bind on non-overlay address, no need to contact router
        return ret;
    }

    // communicate with router
    int n, unix_sock = connect_router();
    if (unix_sock < 0) {
        log_error("bind() fails.\n");
        return -1;
    }

    struct FfrRequestHeader req_header;
    req_header.client_id = client_id; // we don't need this for now
    req_header.func = SOCKET_BIND;
    req_header.body_size = 0;
    if ((n = write(unix_sock, &req_header, sizeof(req_header))) < sizeof(req_header)) {
        printf("%d: bind() write header fails.\n", getpid());
        real.close(unix_sock);
        return -1;
    }

    if (send_fd(unix_sock, fd_get_host_fd(socket)) < 0) {
        log_error("bind() send fd fails.\n");
        real.close(unix_sock);
        return -1;
    }

    int bytes = 0, rsp_size = sizeof(struct SOCKET_BIND_RSP);
    struct SOCKET_BIND_RSP rsp;
    while(bytes < rsp_size) {
        n = read(unix_sock, (char*)&rsp + bytes, rsp_size - bytes);
        if (n < 0) {
            log_error("bind() read fails.\n");
            real.close(unix_sock);
            return -1;
        }
        bytes = bytes + n;
    }        
    if (rsp.ret < 0) {
        log_error("bind() fails on router.\n");
        return -1;
    }
    real.close(unix_sock);

    return ret;
}

int listen(int socket, int backlog)
{
    if (debug_flag) {
        printf("%d: listen(%d, %d)\n", getpid(), socket, backlog);
        fflush(stdout);
    }

    if (fd_get_type(socket) == fd_normal) {
        return real.listen(socket, backlog);
    }

    int overlay_fd = fd_get_overlay_fd(socket);
    int host_fd = fd_get_host_fd(socket);
    int ret = real.listen(overlay_fd, backlog);
    if (ret < 0) {
        return ret;
    }
    real.listen(host_fd, backlog);
    return ret;
}

int accept(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    if (debug_flag) {
        printf("%d: accept(%d)\n", getpid(), socket);
        fflush(stdout);
    }

    if (fd_get_type(socket) == fd_normal) {
        int overlay_fd = real.accept(socket, addr, addrlen);
        if (overlay_fd < 0) {
            return overlay_fd;
        }
        struct fd_info *fdi = calloc(1, sizeof(*fdi));
        idm_set(&idm, overlay_fd, fdi);
        fd_store(overlay_fd, overlay_fd, -1, fd_normal);
        return overlay_fd;
    }

    int overlay_fd;
    struct sockaddr_in cli_addr;
    int cli_addr_len = sizeof(cli_addr);
    if (addr) {
        overlay_fd = real.accept(fd_get_overlay_fd(socket), addr, addrlen);
    }
    else {
        bzero((char *)&cli_addr, cli_addr_len);
        overlay_fd = real.accept(fd_get_overlay_fd(socket), (struct sockaddr *)&cli_addr, (socklen_t *)&cli_addr_len);
        addr = (struct sockaddr *)&cli_addr; // this won't affect app behavior
    }
    if (overlay_fd < 0) {
        log_error("accept() fails on overlay.\n");
        return overlay_fd;
    }

    if (!is_on_overlay((struct sockaddr_in*)addr)) {
        struct fd_info *fdi = calloc(1, sizeof(*fdi));
        idm_set(&idm, overlay_fd, fdi);
        fd_store(overlay_fd, overlay_fd, -1, fd_normal);
        return overlay_fd;   
    }

    // The other side is on overlay, tell it my host binding address
    int original_flags = real.fcntl(overlay_fd, F_GETFL);
    real.fcntl(overlay_fd, F_SETFL, original_flags & ~O_NONBLOCK);

    char my_addr[32];
    struct sockaddr_in host_bind_addr;
    fd_get_host_bind_addr(socket, &host_bind_addr);
    sprintf(my_addr, "%u:%u", host_bind_addr.sin_addr.s_addr , host_bind_addr.sin_port);
    if (send(overlay_fd, my_addr, 31, 0) < 31) {
        log_error("accept() fails to send host binding.\n");
        return -1;
    }

    real.fcntl(overlay_fd, F_SETFL, original_flags);

    int host_fd;
    struct fd_info *fdi = calloc(1, sizeof(*fdi));

    // communicate with router
    int n, unix_sock = connect_router();
    if (unix_sock < 0) {
        log_error("accept() fails connecting to router.\n");
        return -1;
    }

    struct FfrRequestHeader req_header;
    req_header.client_id = client_id;
    req_header.func = SOCKET_ACCEPT;
    req_header.body_size = 0;
    if ((n = write(unix_sock, &req_header, sizeof(req_header))) < sizeof(req_header)) {
        printf("%d: accept() write header fails.\n", getpid());
        real.close(unix_sock);
        return -1;
    }

    if (send_fd(unix_sock, fd_get_host_fd(socket)) < 0) {
        log_error("accept() send fd fails.\n");
        real.close(unix_sock);
        return -1;
    }

    int bytes = 0, rsp_size = sizeof(struct SOCKET_ACCEPT_RSP);
    struct SOCKET_ACCEPT_RSP rsp;
    while(bytes < rsp_size) {
        n = read(unix_sock, (char*)&rsp + bytes, rsp_size - bytes);
        if (n < 0) {
            log_error("accept() read fails.\n");
            real.close(unix_sock);
            return -1;
        }
        bytes = bytes + n;
    }        
    if (rsp.ret < 0) {
        errno = -rsp.ret;
        log_error("accept() fails on router.\n");
        return -1;
    }
    host_fd = recv_fd(unix_sock);
    real.close(unix_sock);
    
    // get host_fd, host_index, addr and addrlen

    if (debug_flag) {
        printf("%d: finish accepting from %d\n", getpid(), socket);
        fflush(stdout);
    }

    idm_set(&idm, host_fd, fdi);
    fd_store(host_fd, overlay_fd, host_fd, fd_fsocket);
    return host_fd;
}

int accept4(int socket, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
    if (debug_flag) {
        printf("%d: accept4(%d)\n", getpid(), socket);
        fflush(stdout);
    }

    if (fd_get_type(socket) == fd_normal) {
        int overlay_fd = real.accept4(socket, addr, addrlen, flags);
        if (overlay_fd < 0) {
            return overlay_fd;
        }
        struct fd_info *fdi = calloc(1, sizeof(*fdi));
        idm_set(&idm, overlay_fd, fdi);
        fd_store(overlay_fd, overlay_fd, -1, fd_normal);
        return overlay_fd;
    }

    int overlay_fd;
    struct sockaddr_in cli_addr;
    int cli_addr_len = sizeof(cli_addr);
    if (addr) {
        overlay_fd = real.accept4(fd_get_overlay_fd(socket), addr, addrlen, flags);
    }
    else {
        bzero((char *)&cli_addr, cli_addr_len);
        overlay_fd = real.accept4(fd_get_overlay_fd(socket), (struct sockaddr *)&cli_addr, (socklen_t *)&cli_addr_len, flags);
        addr = (struct sockaddr *)&cli_addr; // this won't affect app behavior
    }
    if (overlay_fd < 0) {
        log_error("accept4() fails on overlay.\n");
        return overlay_fd;
    }

    if (!is_on_overlay((struct sockaddr_in*)addr)) {
        struct fd_info *fdi = calloc(1, sizeof(*fdi));
        idm_set(&idm, overlay_fd, fdi);
        fd_store(overlay_fd, overlay_fd, -1, fd_normal);
        return overlay_fd;   
    }

    // The other side is on overlay, tell it my host binding address
    int original_flags = real.fcntl(overlay_fd, F_GETFL);
    real.fcntl(overlay_fd, F_SETFL, original_flags & ~O_NONBLOCK);

    char my_addr[32];
    struct sockaddr_in host_bind_addr;
    fd_get_host_bind_addr(socket, &host_bind_addr);
    sprintf(my_addr, "%u:%u", host_bind_addr.sin_addr.s_addr , host_bind_addr.sin_port);
    if (send(overlay_fd, my_addr, 31, 0) < 31) {
        log_error("accept4() fails to send host binding.\n");
        return -1;
    }

    real.fcntl(overlay_fd, F_SETFL, original_flags);

    int host_fd;
    struct fd_info *fdi = calloc(1, sizeof(*fdi));

    // communicate with router
    int n, unix_sock = connect_router();
    if (unix_sock < 0) {
        log_error("accept4() fails connecting to router.\n");
        return -1;
    }

    struct FfrRequestHeader req_header;
    req_header.client_id = client_id;
    req_header.func = SOCKET_ACCEPT4;
    req_header.body_size = sizeof(struct SOCKET_ACCEPT4_REQ);
    if ((n = write(unix_sock, &req_header, sizeof(req_header))) < sizeof(req_header)) {
        printf("%d: accept4() write header fails.\n", getpid());
        real.close(unix_sock);
        return -1;
    }

    struct SOCKET_ACCEPT4_REQ req_body;
    req_body.flags = flags;
    if ((n = write(unix_sock, &req_body, req_header.body_size)) < req_header.body_size) {
        log_error("accept4() write body fails.\n");
        real.close(unix_sock);
        return -1;
    }

    if (send_fd(unix_sock, fd_get_host_fd(socket)) < 0) {
        log_error("accept4() send fd fails.\n");
        real.close(unix_sock);
        return -1;
    }

    int bytes = 0, rsp_size = sizeof(struct SOCKET_ACCEPT4_RSP);
    struct SOCKET_ACCEPT4_RSP rsp;
    while(bytes < rsp_size) {
        n = read(unix_sock, (char*)&rsp + bytes, rsp_size - bytes);
        if (n < 0) {
            log_error("accept4() read fails.\n");
            real.close(unix_sock);
            return -1;
        }
        bytes = bytes + n;
    }        
    if (rsp.ret < 0) {
        errno = -rsp.ret;        
        log_error("accept4() fails on router.\n");
        return -1;
    }
    host_fd = recv_fd(unix_sock);
    real.close(unix_sock);
    
    // get host_fd, host_index, addr and addrlen

    idm_set(&idm, host_fd, fdi);
    fd_store(host_fd, overlay_fd, host_fd, fd_fsocket);
    return host_fd;
}

int connect(int socket, const struct sockaddr *addr, socklen_t addrlen)
{
    if (debug_flag) {
        printf("%d: connect(%d, %s, %hu)\n", getpid(), socket, inet_ntoa(((struct sockaddr_in*)addr)->sin_addr), htons(((struct sockaddr_in*)addr)->sin_port));
        fflush(stdout);
    }

    if (fd_get_type(socket) == fd_normal) {
        return real.connect(socket, addr, addrlen);
    }

    int overlay_fd = fd_get_overlay_fd(socket);

    // connect to outside overlay, simply return
    if (!is_on_overlay((struct sockaddr_in*)addr)) {
        log_error("connect() to outside overlay.\n");        
        return real.connect(overlay_fd, addr, addrlen);
    }

    // connect to overlay, we first clear the non-blocking bit
    int original_flags = real.fcntl(overlay_fd, F_GETFL);
    real.fcntl(overlay_fd, F_SETFL, original_flags & ~O_NONBLOCK);
    int ret = real.connect(overlay_fd, addr, addrlen);
    if (ret < 0) {
        log_error("connect() fails on overlay.\n");
        real.fcntl(overlay_fd, F_SETFL, original_flags);
        return -1;        
    }

    // connect to overlay peer
    struct sockaddr_in host_addr;
    host_addr.sin_family = AF_INET;
    char buffer[32];
    if (recv(overlay_fd, buffer, 31, 0) <= 0) {
        log_error("connect() fails to get host binding.\n");
        real.fcntl(overlay_fd, F_SETFL, original_flags);
        return -1;
    }
    sscanf(buffer, "%u:%hu", &(host_addr.sin_addr.s_addr), &(host_addr.sin_port));

    // set the flags back
    real.fcntl(overlay_fd, F_SETFL, original_flags);

    if (debug_flag) {
        printf("%d: HOST connect(%d, %s, %hu)\n", getpid(), socket, inet_ntoa(host_addr.sin_addr), htons(host_addr.sin_port));
        fflush(stdout);
    }

    // communicate with router
    int n, unix_sock = connect_router();
    if (unix_sock < 0) {
        log_error("connect() fails.\n");
        return -1;
    }

    struct FfrRequestHeader req_header;
    req_header.client_id = client_id; // we don't need this for now
    req_header.func = SOCKET_CONNECT;
    req_header.body_size = sizeof(struct SOCKET_CONNECT_REQ);
    if ((n = write(unix_sock, &req_header, sizeof(req_header))) < sizeof(req_header)) {
        printf("%d connect() write header fails.\n", getpid());
        real.close(unix_sock);
        return -1;
    }

    struct SOCKET_CONNECT_REQ req_body;
    req_body.host_addr = host_addr;
    req_body.host_addrlen = sizeof(host_addr);
    if ((n = write(unix_sock, &req_body, req_header.body_size)) < req_header.body_size) {
        log_error("connect() write body fails.\n");
        real.close(unix_sock);
        return -1;
    }

    if (send_fd(unix_sock, fd_get_host_fd(socket)) < 0) {
        log_error("accept() send fd fails.\n");
        real.close(unix_sock);
        return -1;
    }

    int bytes = 0, rsp_size = sizeof(struct SOCKET_CONNECT_RSP);
    struct SOCKET_CONNECT_RSP rsp;
    while(bytes < rsp_size) {
        n = read(unix_sock, (char*)&rsp + bytes, rsp_size - bytes);
        if (n < 0) {
            log_error("connect() read fails.\n");
            real.close(unix_sock);
            return -1;
        }
        bytes = bytes + n;
    }

    real.close(unix_sock);

    if (rsp.ret < 0) {
        if (rsp.ret != -EINPROGRESS) {
            errno = -rsp.ret;
            log_error("connect() fails on router.\n");
            return -1;
        }
    }

    if (debug_flag) {
        printf("%d: finish connecting %d\n", getpid(), socket);
        fflush(stdout);
    }

    // we overwrite the socket fd mapping
    int host_fd = fd_get_host_fd(socket);
    if (socket != host_fd) {
        int new_overlay_fd = dup(overlay_fd);
        dup2(host_fd, socket);
        real.close(host_fd);
        fd_store(socket, new_overlay_fd, socket, -1);
    }

    if (rsp.ret < 0) {
        errno = -rsp.ret;
        return -1;
    }

    return rsp.ret;
}

int getpeername(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    if (fd_get_type(socket) == fd_normal) {
        return real.getpeername(socket, addr, addrlen);
    }

    int overlay_fd = fd_get_overlay_fd(socket);
    return real.getpeername(overlay_fd, addr, addrlen);
}

int getsockname(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
    if (fd_get_type(socket) == fd_normal) {
        return real.getsockname(socket, addr, addrlen);
    }

    int overlay_fd = fd_get_overlay_fd(socket);
    return real.getsockname(overlay_fd, addr, addrlen);
}

int getsockopt(int socket, int level, int optname,
        void *optval, socklen_t *optlen)
{
    if (fd_get_type(socket) == fd_normal) {
        return real.getsockopt(socket, level, optname, optval, optlen);
    }
    int overlay_fd = fd_get_overlay_fd(socket);
    return real.getsockopt(overlay_fd, level, optname, optval, optlen);
}

int setsockopt(int socket, int level, int optname,
        const void *optval, socklen_t optlen)
{
    if (debug_flag) {
        printf("%d: setsockopt(%d, %d, %d)\n", getpid(), socket, level, optname);
        fflush(stdout);
    }

    if (fd_get_type(socket) == fd_normal) {
        return real.setsockopt(socket, level, optname, optval, optlen);
    }

    int overlay_fd = fd_get_overlay_fd(socket);
    int ret = real.setsockopt(overlay_fd, level, optname, optval, optlen);
    if (optname != SO_REUSEPORT && optname != SO_REUSEADDR && level != IPPROTO_IPV6) {
        int host_fd = fd_get_host_fd(socket);
        ret = real.setsockopt(host_fd, level, optname, optval, optlen);
    }
    return ret;
}

int fcntl(int socket, int cmd, ... /* arg */)
{
    va_list args;
    long lparam;
    void *pparam;
    int ret;
    bool normal;
    int overlay_fd = socket, host_fd = socket;

    init_preload();
    if (fd_get_type(socket) == fd_normal) {
        normal = 1;
    }
    else {
        normal = 0;
        overlay_fd = fd_get_overlay_fd(socket);
        host_fd = fd_get_host_fd(socket);
    }

    // TODO: need to check whether it's a socket here

    va_start(args, cmd);
    switch (cmd) {
    case F_GETFD:
    case F_GETFL:
    case F_GETOWN:
    case F_GETSIG:
    case F_GETLEASE:
        if (normal) {
            ret = real.fcntl(socket, cmd);
        }
        else {
            ret = real.fcntl(host_fd, cmd);
            if (overlay_fd != host_fd) {
                real.fcntl(overlay_fd, cmd);
            }
        }
        break;
    case F_DUPFD:
    /*case F_DUPFD_CLOEXEC:*/
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
        lparam = va_arg(args, long);
        if (normal) {
            ret = real.fcntl(socket, cmd, lparam);
        }
        else {
            ret = real.fcntl(host_fd, cmd, lparam);
            if (overlay_fd != host_fd) {
                real.fcntl(overlay_fd, cmd, lparam);
            }
        }
        break;
    default:
        pparam = va_arg(args, void *);
        if (normal) {
            ret = real.fcntl(socket, cmd, pparam);
        }
        else {
            ret = real.fcntl(host_fd, cmd, pparam);
            if (overlay_fd != host_fd) {
                real.fcntl(overlay_fd, cmd, pparam);
            }
        }
        break;
    }
    va_end(args);
    return ret;
}

int close(int socket)
{
    struct fd_info *fdi;
    int ret;

    init_preload();
    if (fd_get_type(socket) == fd_normal) {
        fdi = idm_lookup(&idm, socket);
        if (fdi) {
            free(fdi);
            idm_clear(&idm, socket);
        }
        return real.close(socket);
    }

    int overlay_fd = fd_get_overlay_fd(socket);
    int host_fd = fd_get_host_fd(socket);

    if (debug_flag) {
        printf("%d: close(%d)\n", getpid(), socket);
        fflush(stdout);
    }

    fdi = idm_lookup(&idm, socket);
    idm_clear(&idm, socket);
    free(fdi);
    if (overlay_fd != host_fd) {
        real.close(host_fd);
    }
    ret = real.close(overlay_fd);
    return ret;
}
