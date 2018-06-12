/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>

typedef enum RDMA_FUNCTION_CALL
{
        IBV_GET_DEVICE_LIST,
        IBV_GET_CONTEXT,
        IBV_QUERY_DEV,
        IBV_EXP_QUERY_DEV,
        IBV_QUERY_PORT,
        IBV_ALLOC_PD,
        IBV_DEALLOC_PD,
        IBV_CREATE_QP,
        IBV_MODIFY_QP,
	IBV_QUERY_QP,
        IBV_DESTROY_QP,

        IBV_CREATE_SRQ,
        IBV_MODIFY_SRQ,
        IBV_POST_SRQ_RECV,
        IBV_DESTROY_SRQ,

        IBV_CREATE_CQ,
        IBV_DESTROY_CQ,
        IBV_POLL_CQ,
        IBV_GET_CQ_EVENT,
        IBV_ACK_CQ_EVENT,
        IBV_REQ_NOTIFY_CQ,
        
        IBV_REG_MR,
        IBV_REG_MR_MAPPING,
        IBV_DEREG_MR,
        
        IBV_REG_CM,
        IBV_POST_SEND,
        IBV_POST_RECV,

        IBV_CREATE_COMP_CHANNEL,
        IBV_DESTROY_COMP_CHANNEL,

        IBV_CREATE_AH,
        IBV_DESTROY_AH,

	IBV_CREATE_FLOW,
        IBV_DESTROY_FLOW,

        IBV_MALLOC,
        IBV_FREE,

        // [TODO]
        IBV_RESIZE_CQ,

        CM_CREATE_EVENT_CHANNEL,
        CM_DESTROY_EVENT_CHANNEL,
        CM_CREATE_ID,
        CM_BIND_IP,
        CM_BIND,
        CM_GET_EVENT,
        CM_QUERY_ROUTE,
        CM_LISTEN,
        CM_RESOLVE_IP,
        CM_RESOLVE_ADDR,
        CM_UCMA_QUERY_ADDR,
        CM_UCMA_QUERY_GID,
        CM_UCMA_PROCESS_CONN_RESP,
        CM_DESTROY_ID,
        CM_RESOLVE_ROUTE,
        CM_UCMA_QUERY_PATH,
        CM_INIT_QP_ATTR,
        CM_CONNECT,
        CM_ACCEPT,
        CM_SET_OPTION,
        CM_MIGRATE_ID,
        CM_DISCONNECT,

        // Freeflow Socket
        SOCKET_SOCKET,
        SOCKET_BIND,
        SOCKET_ACCEPT,
        SOCKET_ACCEPT4,
        SOCKET_CONNECT,

} RDMA_FUNCTION_CALL;

struct FfrRequestHeader
{
        int client_id;
        RDMA_FUNCTION_CALL func;
        uint32_t body_size;
};

struct FfrResponseHeader
{
        int rsp_size;
};

struct SOCKET_SOCKET_REQ
{
        int domain;
        int type;
        int protocol;
};

struct SOCKET_SOCKET_RSP
{
        int ret; // host_index or errno
};

struct SOCKET_BIND_RSP
{
        int ret;
};

struct SOCKET_ACCEPT_RSP
{
        int ret; // host_index or errno
};

struct SOCKET_ACCEPT4_REQ
{
        int flags;
};

struct SOCKET_ACCEPT4_RSP
{
        int ret; // host_index or errno
};

struct SOCKET_CONNECT_REQ
{
        struct sockaddr_in host_addr;
        socklen_t host_addrlen;
};

struct SOCKET_CONNECT_RSP
{
        int ret;
};

#endif /* TYPES_H */
