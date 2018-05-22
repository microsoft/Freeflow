#ifndef FREEFLOW_TYPES_H
#define FREEFLOW_TYPES_H

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/driver.h>
#include <infiniband/driver_exp.h>
#include <infiniband/kern-abi.h>
#include <infiniband/kern-abi_exp.h>

#include <rdma/rdma_cma.h>
#include <rdma/rdma_cma_abi.h>
#include <rdma/rdma_verbs.h>

#include <pthread.h>

#define CTRL_REQ_SIZE 1024 * 1024 // 1 MB
#define CTRL_RSP_SIZE 1024 * 1024 // 1 MB

#define MAP_SIZE 10240
#define WR_QUEUE_SIZE 102400
#define PRINT_LOG 0
#define PARALLEL_SIZE 10

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

struct IBV_GET_CONTEXT_RSP
{
        int async_fd;
        int num_comp_vectors;
};

struct IBV_QUERY_DEV_RSP
{
        struct ibv_device_attr dev_attr;        
};

struct IBV_EXP_QUERY_DEV_REQ
{
        struct ibv_exp_query_device cmd;
};

struct IBV_EXP_QUERY_DEV_RSP
{
        int ret_errno;
        struct ibv_exp_query_device_resp resp;
};

struct IBV_QUERY_PORT_REQ
{
        uint8_t port_num;        
};

struct IBV_QUERY_PORT_RSP
{
        struct ibv_port_attr port_attr;        
};

struct IBV_ALLOC_PD_RSP
{
        uint32_t pd_handle;
};

struct IBV_DEALLOC_PD_REQ
{
        uint32_t pd_handle;
};

struct IBV_DEALLOC_PD_RSP
{
        int ret;
};

struct IBV_CREATE_CQ_REQ
{
        int channel_fd;
        int comp_vector;
        int cqe;
};

struct IBV_CREATE_CQ_RSP
{
        uint32_t cqe;
        uint32_t handle;
        char shm_name[100];
};

struct IBV_DESTROY_CQ_REQ
{
        uint32_t cq_handle;
};

struct IBV_DESTROY_CQ_RSP
{
        int ret;
};

struct IBV_REQ_NOTIFY_CQ_REQ
{
        uint32_t cq_handle;
        int solicited_only;
};

struct IBV_REQ_NOTIFY_CQ_RSP
{
        int ret;
};

struct IBV_CREATE_QP_REQ
{
        uint32_t pd_handle;
        enum ibv_qp_type qp_type;
        int sq_sig_all;
        uint32_t send_cq_handle;
        uint32_t recv_cq_handle;
        uint32_t srq_handle;
        struct ibv_qp_cap cap;
};

struct IBV_CREATE_QP_RSP
{
        uint32_t qp_num;
        uint32_t handle;
        struct ibv_qp_cap cap;
        char shm_name[100];
};

struct IBV_DESTROY_QP_REQ
{
        uint32_t qp_handle;
};

struct IBV_DESTROY_QP_RSP
{
        int ret;
};

struct IBV_REG_MR_REQ
{
        uint32_t pd_handle;
        uint32_t mem_size;
        uint32_t access_flags;
        char shm_name[100];
};

struct IBV_REG_MR_RSP
{
        uint32_t handle;
        uint32_t lkey;
        uint32_t rkey;
        char shm_name[100];
};

struct IBV_REG_MR_MAPPING_REQ {
    uint32_t key;
    char* mr_ptr;
    char* shm_ptr;
};

struct IBV_REG_MR_MAPPING_RSP {
    int ret;
};

struct IBV_DEREG_MR_REQ
{
        uint32_t handle;
};

struct IBV_DEREG_MR_RSP
{
        int ret;
};

struct IBV_MODIFY_QP_REQ
{
        uint32_t handle;
        struct ibv_qp_attr attr;
        uint32_t attr_mask;
};

struct IBV_MODIFY_QP_RSP
{
	int ret;
        uint32_t handle;
};

struct IBV_QUERY_QP_REQ
{
	int cmd_size;
        char cmd[100];
};

struct IBV_QUERY_QP_RSP
{
	int ret_errno;
        struct ibv_query_qp_resp resp;
};

struct IBV_POST_SEND_REQ
{
        uint32_t wr_size;
        /* handle is the first 4 bytes, wr is the rest */
        char* wr;
};

struct IBV_POST_SEND_RSP
{
        int ret_errno;
        uint32_t bad_wr;
};

struct IBV_POST_RECV_REQ
{
        uint32_t wr_size;
        /* handle is the first 4 bytes, wr is the rest */
        char* wr;
};

struct IBV_POST_RECV_RSP
{
        int ret_errno;
        uint32_t bad_wr;
};

struct IBV_POLL_CQ_REQ
{
        uint32_t cq_handle;
        int ne;
};

struct IBV_POLL_CQ_RSP
{
        int ret_errno; // which is also the count of following wc.
        struct ibv_wc wc;
};

struct IBV_CREATE_COMP_CHANNEL_RSP
{
        int fd;
};

struct IBV_DESTROY_COMP_CHANNEL_REQ
{
        int fd;
};

struct IBV_DESTROY_COMP_CHANNEL_RSP
{
        int ret;
};

struct IBV_GET_CQ_EVENT_REQ
{
        int fd;
};

struct IBV_GET_CQ_EVENT_RSP
{
        uint32_t cq_handle;
        uint32_t comp_events_completed;
        uint32_t async_events_completed;
};

struct IBV_ACK_CQ_EVENT_REQ
{
        uint32_t cq_handle;
        uint32_t nevents;
};

struct IBV_ACK_CQ_EVENT_RSP
{
        uint32_t cq_handle;
        uint32_t comp_events_completed;
        uint32_t async_events_completed;
};

struct IBV_CREATE_AH_REQ
{
        uint32_t pd_handle;
        struct ibv_ah_attr ah_attr;
};

struct IBV_CREATE_AH_RSP
{
        uint32_t ah_handle;
        int ret;
};

struct IBV_DESTROY_AH_REQ
{
        uint32_t ah_handle;
};

struct IBV_DESTROY_AH_RSP
{
        int ret;
};

struct IBV_CREATE_FLOW_REQ
{
	int exp_flow;
        int written_size;
        char cmd[1024];
};

struct IBV_CREATE_FLOW_RSP
{
        int ret_errno;
	struct ibv_create_flow_resp resp;
};

struct IBV_DESTROY_FLOW_REQ
{
        struct ibv_destroy_flow cmd;
};

struct IBV_DESTROY_FLOW_RSP
{
        int ret_errno;
};

struct IBV_MALLOC_REQ
{
        size_t length;
};

struct IBV_MALLOC_RSP
{
        char shm_name[100];
};

struct IBV_FREE_REQ
{
        char shm_name[100];
};

struct IBV_FREE_RSP
{
        int ret;
};

struct IBV_CREATE_SRQ_REQ
{
        uint32_t pd_handle;
        struct ibv_srq_init_attr attr;
};

struct IBV_CREATE_SRQ_RSP
{
        uint32_t srq_handle;
        char shm_name[100];
};

struct IBV_MODIFY_SRQ_REQ
{
        uint32_t srq_handle;
        struct ibv_srq_attr attr;
        int srq_attr_mask;
};

struct IBV_MODIFY_SRQ_RSP
{
        uint32_t ret;
};

struct IBV_DESTROY_SRQ_REQ
{
        uint32_t srq_handle;
};

struct IBV_DESTROY_SRQ_RSP
{
        int ret;
};

struct IBV_POST_SRQ_RECV_REQ
{
        uint32_t wr_size;
        /* handle is the first 4 bytes, wr is the rest */
        char* wr;
};

struct IBV_POST_SRQ_RECV_RSP
{
        int ret_errno;
        uint32_t bad_wr;
};

struct CM_CREATE_EVENT_CHANNEL_RSP
{
        int ret_errno;
        struct rdma_event_channel ec;
};

struct CM_DESTROY_EVENT_CHANNEL_REQ
{
        struct rdma_event_channel ec;
};

struct CM_DESTROY_EVENT_CHANNEL_RSP
{
        int ret_errno;
};

struct CM_CREATE_ID_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_create_id cmd;
};

struct CM_CREATE_ID_RSP
{
        int ret_errno;
        struct ucma_abi_create_id_resp resp;
};

struct CM_BIND_IP_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_bind_ip cmd;
};

struct CM_BIND_IP_RSP
{
        int ret_errno;
};

struct CM_BIND_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_bind cmd;
};

struct CM_BIND_RSP
{
        int ret_errno;
};

struct CM_GET_EVENT_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_get_event cmd;
};

struct CM_GET_EVENT_RSP
{
        int ret_errno;
        struct ucma_abi_event_resp resp;
};

struct CM_QUERY_ROUTE_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_query cmd;
};

struct CM_QUERY_ROUTE_RSP
{
        int ret_errno;
        struct ucma_abi_query_route_resp resp;
};

struct CM_LISTEN_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_listen cmd;
};

struct CM_LISTEN_RSP
{
        int ret_errno;
};

struct CM_RESOLVE_IP_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_resolve_ip cmd;
};

struct CM_RESOLVE_IP_RSP
{
        int ret_errno;
};

struct CM_RESOLVE_ADDR_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_resolve_addr cmd;
};

struct CM_RESOLVE_ADDR_RSP
{
        int ret_errno;
};

struct CM_UCMA_QUERY_ADDR_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_query cmd;
};

struct CM_UCMA_QUERY_ADDR_RSP
{
        int ret_errno;
        struct ucma_abi_query_addr_resp resp;
};

struct CM_UCMA_QUERY_GID_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_query cmd;
};

struct CM_UCMA_QUERY_GID_RSP
{
        int ret_errno;
        struct ucma_abi_query_addr_resp resp;
};

struct CM_UCMA_PROCESS_CONN_RESP_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_accept cmd;
};

struct CM_UCMA_PROCESS_CONN_RESP_RSP
{
        int ret_errno;
};

struct CM_DESTROY_ID_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_destroy_id cmd;
};

struct CM_DESTROY_ID_RSP
{
        int ret_errno;
        struct ucma_abi_destroy_id_resp resp;
};

struct CM_RESOLVE_ROUTE_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_resolve_route cmd;
};

struct CM_RESOLVE_ROUTE_RSP
{
        int ret_errno;
};

struct CM_UCMA_QUERY_PATH_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_query cmd;
};

struct CM_UCMA_QUERY_PATH_RSP
{
        int ret_errno;
        struct ucma_abi_query_path_resp resp;
        struct ibv_path_data path_data[6];
};

struct CM_INIT_QP_ATTR_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_init_qp_attr cmd;
};

struct CM_INIT_QP_ATTR_RSP
{
        int ret_errno;
        struct ibv_kern_qp_attr resp;
};

struct CM_CONNECT_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_connect cmd;
};

struct CM_CONNECT_RSP
{
        int ret_errno;
};

struct CM_ACCEPT_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_accept cmd;
};

struct CM_ACCEPT_RSP
{
        int ret_errno;
};

struct CM_SET_OPTION_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_set_option cmd;
        char optval[100];
};

struct CM_SET_OPTION_RSP
{
        int ret_errno;
};

struct CM_MIGRATE_ID_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_migrate_id cmd;
};

struct CM_MIGRATE_ID_RSP
{
        int ret_errno;
        struct ucma_abi_migrate_resp resp;
};

struct CM_DISCONNECT_REQ
{
        struct rdma_event_channel ec;
        struct ucma_abi_disconnect cmd;
};

struct CM_DISCONNECT_RSP
{
        int ret_errno;
};

enum CtrlChannelState
{
        IDLE,
        REQ_DONE,
        RSP_DONE,
};

struct CtrlShmPiece
{
        volatile enum CtrlChannelState state;
        uint8_t req[CTRL_REQ_SIZE];
        uint8_t rsp[CTRL_RSP_SIZE];
};

#endif /* FREEFLOW_TYPES_H */

