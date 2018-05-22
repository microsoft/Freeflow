/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
 * Copyright (c) 2006 Cisco Systems, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "ibverbs.h"
#include "infiniband/arch.h"
#include "infiniband/freeflow.h"

enum ibv_cmd_type {
	IBV_CMD_BASIC,
	IBV_CMD_EXTENDED
};

int ibv_cmd_get_context(struct ibv_context *context, struct ibv_get_context *cmd,
			size_t cmd_size, struct ibv_get_context_resp *resp,
			size_t resp_size)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_get_context ####\n");
		fflush(stdout);
	}

	struct IBV_GET_CONTEXT_RSP rsp;
	int rsp_size;
	request_router(IBV_GET_CONTEXT, NULL, &rsp, &rsp_size);
	
	context->async_fd = rsp.async_fd;
	context->num_comp_vectors = rsp.num_comp_vectors;

	return 0;
}

void copy_query_dev_fields(struct ibv_device_attr *device_attr,
				  struct ibv_query_device_resp *resp,
				  uint64_t *raw_fw_ver)
{
	*raw_fw_ver				= resp->fw_ver;
	device_attr->node_guid			= resp->node_guid;
	device_attr->sys_image_guid		= resp->sys_image_guid;
	device_attr->max_mr_size		= resp->max_mr_size;
	device_attr->page_size_cap		= resp->page_size_cap;
	device_attr->vendor_id			= resp->vendor_id;
	device_attr->vendor_part_id		= resp->vendor_part_id;
	device_attr->hw_ver			= resp->hw_ver;
	device_attr->max_qp			= resp->max_qp;
	device_attr->max_qp_wr			= resp->max_qp_wr;
	device_attr->device_cap_flags		= resp->device_cap_flags;
	device_attr->max_sge			= resp->max_sge;
	device_attr->max_sge_rd			= resp->max_sge_rd;
	device_attr->max_cq			= resp->max_cq;
	device_attr->max_cqe			= resp->max_cqe;
	device_attr->max_mr			= resp->max_mr;
	device_attr->max_pd			= resp->max_pd;
	device_attr->max_qp_rd_atom		= resp->max_qp_rd_atom;
	device_attr->max_ee_rd_atom		= resp->max_ee_rd_atom;
	device_attr->max_res_rd_atom		= resp->max_res_rd_atom;
	device_attr->max_qp_init_rd_atom	= resp->max_qp_init_rd_atom;
	device_attr->max_ee_init_rd_atom	= resp->max_ee_init_rd_atom;
	device_attr->atomic_cap			= resp->atomic_cap;
	device_attr->max_ee			= resp->max_ee;
	device_attr->max_rdd			= resp->max_rdd;
	device_attr->max_mw			= resp->max_mw;
	device_attr->max_raw_ipv6_qp		= resp->max_raw_ipv6_qp;
	device_attr->max_raw_ethy_qp		= resp->max_raw_ethy_qp;
	device_attr->max_mcast_grp		= resp->max_mcast_grp;
	device_attr->max_mcast_qp_attach	= resp->max_mcast_qp_attach;
	device_attr->max_total_mcast_qp_attach	= resp->max_total_mcast_qp_attach;
	device_attr->max_ah			= resp->max_ah;
	device_attr->max_fmr			= resp->max_fmr;
	device_attr->max_map_per_fmr		= resp->max_map_per_fmr;
	device_attr->max_srq			= resp->max_srq;
	device_attr->max_srq_wr			= resp->max_srq_wr;
	device_attr->max_srq_sge		= resp->max_srq_sge;
	device_attr->max_pkeys			= resp->max_pkeys;
	device_attr->local_ca_ack_delay		= resp->local_ca_ack_delay;
	device_attr->phys_port_cnt		= resp->phys_port_cnt;
}

int ibv_cmd_query_device(struct ibv_context *context,
			 struct ibv_device_attr *device_attr,
			 uint64_t *raw_fw_ver,
			 struct ibv_query_device *cmd, size_t cmd_size)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_query_device ####\n");
		fflush(stdout);
	}

	struct IBV_QUERY_DEV_RSP rsp;
	int rsp_size;
	request_router(IBV_QUERY_DEV, NULL, &rsp, &rsp_size);
	memcpy(device_attr, &rsp.dev_attr, sizeof(struct ibv_device_attr));
	
	if (PRINT_LOG)
	{
		printf("vendor id=%d, vendor_part id=%d\n", device_attr->vendor_id, device_attr->vendor_part_id);
		fflush(stdout);
	}

	return 0;
}

int ibv_cmd_query_port(struct ibv_context *context, uint8_t port_num,
		       struct ibv_port_attr *port_attr,
		       struct ibv_query_port *cmd, size_t cmd_size)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_query_port ####\n");
		fflush(stdout);
	}

	struct IBV_QUERY_PORT_REQ req;
	req.port_num = port_num;

	struct IBV_QUERY_PORT_RSP rsp;
	int rsp_size;
	request_router(IBV_QUERY_PORT, &req, &rsp, &rsp_size);
	
	memcpy(port_attr, &rsp.port_attr, sizeof(struct ibv_port_attr));	

	if (PRINT_LOG)
	{
		printf("state=%d, max_mtu=%d, active_mtu=%d\n", port_attr->state, port_attr->max_mtu, port_attr->active_mtu);
		fflush(stdout);
	}
	
	return 0;
}

int ibv_cmd_alloc_pd(struct ibv_context *context, struct ibv_pd *pd,
		     struct ibv_alloc_pd *cmd, size_t cmd_size,
		     struct ibv_alloc_pd_resp *resp, size_t resp_size)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_alloc_pd ####\n");
		fflush(stdout);	
	}
	
	struct IBV_ALLOC_PD_RSP rsp;
	int rsp_size;
	request_router(IBV_ALLOC_PD, NULL, &rsp, &rsp_size);

	pd->handle = rsp.pd_handle;
	pd->context = context;

	if (PRINT_LOG)
	{
		printf("PD handle = %d\n", pd->handle);
		fflush(stdout);
	}
	
	return 0;
}

int ibv_cmd_dealloc_pd(struct ibv_pd *pd)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_dealloc_pd ####\n");
		fflush(stdout);
	}

	struct IBV_DEALLOC_PD_REQ req;
	req.pd_handle = pd->handle;

	struct IBV_DEALLOC_PD_RSP rsp;
	int rsp_size;
	request_router(IBV_DEALLOC_PD, &req, &rsp, &rsp_size);

	return rsp.ret;
}

int ibv_cmd_open_xrcd(struct ibv_context *context, struct verbs_xrcd *xrcd,
				int vxrcd_size,
				struct ibv_xrcd_init_attr *attr,
				struct ibv_open_xrcd *cmd, size_t cmd_size,
				struct ibv_open_xrcd_resp *resp,
				size_t resp_size)
{
	printf("#### ibv_cmd_open_xrcd ####\n");
	fflush(stdout);

	IBV_INIT_CMD_RESP(cmd, cmd_size, OPEN_XRCD, resp, resp_size);

	if (attr->comp_mask >= IBV_XRCD_INIT_ATTR_RESERVED)
		return ENOSYS;

	if (!(attr->comp_mask & IBV_XRCD_INIT_ATTR_FD) ||
	    !(attr->comp_mask & IBV_XRCD_INIT_ATTR_OFLAGS))
		return EINVAL;

	cmd->fd = attr->fd;
	cmd->oflags = attr->oflags;
	/*if (write(context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);

	xrcd->xrcd.context = context;
	xrcd->comp_mask = 0;
	if (vext_field_avail(struct verbs_xrcd, handle, vxrcd_size)) {
		xrcd->comp_mask = VERBS_XRCD_HANDLE;
		xrcd->handle  = resp->xrcd_handle;
	}

	return 0;
}

int ibv_cmd_close_xrcd(struct verbs_xrcd *xrcd)
{
	printf("#### ibv_cmd_close_xrcd ####\n");
	fflush(stdout);

	struct ibv_close_xrcd cmd;

	IBV_INIT_CMD(&cmd, sizeof cmd, CLOSE_XRCD);
	cmd.xrcd_handle = xrcd->handle;

	/*if (write(xrcd->xrcd.context->cmd_fd, &cmd, sizeof cmd) != sizeof cmd)
		return errno;*/

	return 0;
}

int ibv_cmd_reg_mr(struct ibv_pd *pd, void *addr, size_t length,
		   uint64_t hca_va, int access,
		   struct ibv_mr *mr, struct ibv_reg_mr *cmd,
		   size_t cmd_size,
		   struct ibv_reg_mr_resp *resp, size_t resp_size)
{
	int ret;
	/* char key[64]; */
	struct mr_shm* p;
	
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_reg_mr ####\n");
		fflush(stdout);
	}

	struct IBV_REG_MR_REQ req_body;
	req_body.pd_handle = pd->handle;
	req_body.mem_size = length;
	req_body.access_flags = access;
	req_body.shm_name[0] = '\0';
	
	struct IBV_REG_MR_RSP rsp;
	int rsp_size;
	request_router(IBV_REG_MR, &req_body, &rsp, &rsp_size);

	mr->handle  = rsp.handle;
	mr->lkey    = rsp.lkey;
	mr->rkey    = rsp.rkey;
	strcpy(mr->shm_name, rsp.shm_name);

	// FreeFlow: mounting shared memory from router.
	mr->shm_fd = shm_open(mr->shm_name, O_CREAT | O_RDWR, 0666);
	ret = ftruncate(mr->shm_fd, length);

	char *membuff = (char *)malloc(length);
	memcpy(membuff, addr, length);

        int is_align = (long)addr % (4 * 1024) == 0 ? 1 : 0;
        if (is_align)
		mr->shm_ptr = mmap(addr, length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED | MAP_LOCKED, mr->shm_fd, 0); 
	else
		mr->shm_ptr = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, mr->shm_fd, 0); 


	if (mr->shm_ptr == MAP_FAILED || ret > 0){
		printf("mmap failed in reg mr.\n");
		fflush(stdout);
	}
	else
	{
		memcpy(mr->shm_ptr, membuff, length);
		if (PRINT_LOG)
		{
			printf("mmap succeed in reg mr.\n");
		}

		struct IBV_REG_MR_MAPPING_REQ new_req_body;
		new_req_body.key = mr->lkey;
		new_req_body.mr_ptr = addr;
		new_req_body.shm_ptr = mr->shm_ptr;


		struct IBV_REG_MR_MAPPING_RSP new_rsp;
		request_router(IBV_REG_MR_MAPPING, &new_req_body, &new_rsp, &rsp_size);

		p = (struct mr_shm*)mempool_insert(map_lkey_to_mrshm, mr->lkey);
		p->mr = addr;
		p->shm_ptr = mr->shm_ptr;

		if (PRINT_LOG)
		{
			printf("@@@@@@@@ lkey=%u, addr=%lu, shm_prt=%lu\n", mr->lkey, (uint64_t)addr, (uint64_t)(mr->shm_ptr));
			fflush(stdout);
		}

		/*
		hashmap_put(map_lkey_to_shm_ptr, key, (void*)(mr->shm_ptr));
		fflush(stdout);
		*/	
	}

	mr->addr = addr;
	mr->length = length;
	mr->context = pd->context;
	free(membuff);	
	return 0;
}

int ibv_cmd_reg_mr_ff(struct ibv_pd *pd, void **addr, size_t length, int access, char *shm_name, struct ibv_mr *mr)
{
	int ret;
	/* char key[64]; */
	struct mr_shm* p;
	
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_reg_mr_ff ####\n");
		fflush(stdout);
	}

	struct IBV_REG_MR_REQ req_body;
	req_body.pd_handle = pd->handle;
	req_body.mem_size = length;
	req_body.access_flags = access;
	if (shm_name == NULL)
	{
		req_body.shm_name[0] = '\0';
	}
	else
	{
		strcpy(req_body.shm_name, shm_name);
	}
	
	struct IBV_REG_MR_RSP rsp;
	int rsp_size;
	request_router(IBV_REG_MR, &req_body, &rsp, &rsp_size);

	mr->handle  = rsp.handle;
	mr->lkey    = rsp.lkey;
	mr->rkey    = rsp.rkey;
	strcpy(mr->shm_name, rsp.shm_name);

	// FreeFlow: mounting shared memory from router.
	mr->shm_fd = shm_open(mr->shm_name, O_CREAT | O_RDWR, 0666);
	ret = ftruncate(mr->shm_fd, length);
	mr->shm_ptr = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, mr->shm_fd, 0);
	
	if (mr->shm_ptr == MAP_FAILED || ret > 0){
		printf("mmap failed in reg mr.\n");
		fflush(stdout);
	}
	else
	{
		if (PRINT_LOG)
		{
			printf("mmap succeed in reg mr.\n");
		}

		struct IBV_REG_MR_MAPPING_REQ new_req_body;
		new_req_body.key = mr->lkey;
		new_req_body.mr_ptr = mr->shm_ptr;
		new_req_body.shm_ptr = mr->shm_ptr;
		struct IBV_REG_MR_MAPPING_RSP new_rsp;
		request_router(IBV_REG_MR_MAPPING, &new_req_body, &new_rsp, &rsp_size);

		// Change the pointer.
		char *membuff = (char *)malloc(length);
		if (*addr != NULL)
		{
			memcpy(membuff, *addr, length);
			*addr = mr->shm_ptr;
			memcpy(*addr, membuff, length);
		}
		else
		{
			*addr = mr->shm_ptr;
		}

		free(membuff);	

		p = (struct mr_shm*)mempool_insert(map_lkey_to_mrshm, mr->lkey);
		p->mr = *addr;
		p->shm_ptr = mr->shm_ptr;

		if (PRINT_LOG)
		{
			printf("@@@@@@@@ lkey=%u, *addr=%lu, shm_prt=%lu\n", mr->lkey, (uint64_t)(*addr), (uint64_t)(mr->shm_ptr));
			fflush(stdout);
		}

		/*
		hashmap_put(map_lkey_to_shm_ptr, key, (void*)(mr->shm_ptr));
		fflush(stdout);
		*/	
	}

	mr->addr = *addr;
	mr->length = length;
	mr->context = pd->context;
	return 0;
}

int ibv_cmd_rereg_mr(struct ibv_mr *mr, uint32_t flags, void *addr,
		     size_t length, uint64_t hca_va, int access,
		     struct ibv_pd *pd, struct ibv_rereg_mr *cmd,
		     size_t cmd_sz, struct ibv_rereg_mr_resp *resp,
		     size_t resp_sz)
{
	printf("#### ibv_cmd_rereg_mr ####\n");
	fflush(stdout);

	IBV_INIT_CMD_RESP(cmd, cmd_sz, REREG_MR, resp, resp_sz);

	cmd->mr_handle	  = mr->handle;
	cmd->flags	  = flags;
	cmd->start	  = (uintptr_t)addr;
	cmd->length	  = length;
	cmd->hca_va	  = hca_va;
	cmd->pd_handle	  = (flags & IBV_REREG_MR_CHANGE_PD) ? pd->handle : 0;
	cmd->access_flags = access;

	/*if (write(mr->context->cmd_fd, cmd, cmd_sz) != cmd_sz)
		return errno;*/

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_sz);

	mr->lkey    = resp->lkey;
	mr->rkey    = resp->rkey;
	if (flags & IBV_REREG_MR_CHANGE_PD)
		mr->context = pd->context;

	return 0;
}

int ibv_cmd_dereg_mr(struct ibv_mr *mr)
{
	// struct lkey_mr_shm* p;

	struct IBV_DEREG_MR_REQ req_body;
	req_body.handle = mr->handle;
	
	struct IBV_DEREG_MR_RSP rsp;
	int rsp_size;
	request_router(IBV_DEREG_MR, &req_body, &rsp, &rsp_size);	

	shm_unlink(mr->shm_name);

	mempool_del(map_lkey_to_mrshm, mr->lkey);

	return rsp.ret;
}

int ibv_cmd_alloc_mw(struct ibv_pd *pd, enum ibv_mw_type type,
		     struct ibv_mw *mw, struct ibv_alloc_mw *cmd,
		     size_t cmd_size,
		     struct ibv_alloc_mw_resp *resp, size_t resp_size)
{
	printf("#### ibv_cmd_alloc_mw ####\n");
	fflush(stdout);

	IBV_INIT_CMD_RESP(cmd, cmd_size, ALLOC_MW, resp, resp_size);
	cmd->pd_handle	= pd->handle;
	cmd->mw_type	= type;
	memset(cmd->reserved, 0, sizeof(cmd->reserved));

	/*if (write(pd->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);

	mw->context = pd->context;
	mw->pd      = pd;
	mw->rkey    = resp->rkey;
	mw->handle  = resp->mw_handle;
	mw->type    = type;

	return 0;
}

int ibv_cmd_dealloc_mw(struct ibv_mw *mw,
		       struct ibv_dealloc_mw *cmd, size_t cmd_size)
{
	printf("#### ibv_cmd_dealloc_mw ####\n");
	fflush(stdout);

	IBV_INIT_CMD(cmd, cmd_size, DEALLOC_MW);
	cmd->mw_handle = mw->handle;
	cmd->reserved = 0;

	/*if (write(mw->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

	return 0;
}

int ibv_cmd_create_cq(struct ibv_context *context, int cqe,
		      struct ibv_comp_channel *channel,
		      int comp_vector, struct ibv_cq *cq,
		      struct ibv_create_cq *cmd, size_t cmd_size,
		      struct ibv_create_cq_resp *resp, size_t resp_size)
{
	
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_create_cq ####\n");
		fflush(stdout);
	}

	struct IBV_CREATE_CQ_REQ req_body;
	req_body.channel_fd =channel ? comp_channel_map[channel->fd] : -1;
	req_body.comp_vector = comp_vector;
	req_body.cqe = cqe;
	
	struct IBV_CREATE_CQ_RSP rsp;
	int rsp_size;
	request_router(IBV_CREATE_CQ, &req_body, &rsp, &rsp_size);

	if (PRINT_LOG)
	{
		printf("CQ return from router: cqe=%d\n", rsp.cqe);
	}
	
	cq->handle  = rsp.handle;
	cq->cqe     = rsp.cqe;
	cq->context = context;

	int fd = shm_open(rsp.shm_name, O_CREAT | O_RDWR, 0666);
	if (ftruncate(fd, sizeof(struct CtrlShmPiece))) {
		printf("[Error] Fail to mount shm %s\n", rsp.shm_name);
		fflush(stdout);
	}
	void* shm_p = mmap(0, sizeof(struct CtrlShmPiece), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
	cq_shm_map[cq->handle] = (struct CtrlShmPiece *)shm_p;
	pthread_mutex_init(&(cq_shm_mtx_map[cq->handle]), NULL); 

	if (cqe > WR_QUEUE_SIZE) {
		printf("Freeflow WARNING: attempt to create a very large CQ. This may cause problems.\n");
	}
	map_cq_to_wr_queue[cq->handle] = (struct wr_queue*)malloc(sizeof(struct wr_queue));
	map_cq_to_wr_queue[cq->handle]->queue = (struct wr*)malloc(sizeof(struct wr) * WR_QUEUE_SIZE);
	map_cq_to_wr_queue[cq->handle]->head = 0;
	map_cq_to_wr_queue[cq->handle]->tail = 0;
	pthread_spin_init(&(map_cq_to_wr_queue[cq->handle]->head_lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(map_cq_to_wr_queue[cq->handle]->tail_lock), PTHREAD_PROCESS_PRIVATE);

	return 0;
}

int ibv_cmd_poll_cq(struct ibv_cq *ibcq, int ne, struct ibv_wc *wc)
{
	//struct timespec st, et, stt, ett;
	//int time_us;
	//clock_gettime(CLOCK_REALTIME, &st);

	int                      i, j, wr_index;
	struct sge_record        *s;
	struct wr_queue          *wr_queue;
	struct wr                *wr_p;

	struct CtrlShmPiece* csp = cq_shm_map[ibcq->handle];
	pthread_mutex_t* csp_mtx = &(cq_shm_mtx_map[ibcq->handle]);
	struct FfrRequestHeader   *req_header = (struct FfrRequestHeader*)(csp->req);
	struct IBV_POLL_CQ_REQ    *req = (struct IBV_POLL_CQ_REQ*)(csp->req + sizeof(struct FfrRequestHeader));
	struct FfrResponseHeader  *rsp_header = (struct FfrResponseHeader*)csp->rsp;
	struct ibv_wc             *wc_list = (struct ibv_wc*)(csp->rsp + sizeof(struct FfrResponseHeader));


	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_poll_cq ####\n");
		fflush(stdout);
	}

	pthread_mutex_lock(csp_mtx);

	req_header->client_id = ffr_client_id;
	req_header->func = IBV_POLL_CQ;
	req_header->body_size = sizeof(struct IBV_POLL_CQ_REQ);

	req->cq_handle = ibcq->handle;
	req->ne = ne;


	// patch SRQ
	if (map_cq_to_srq[ibcq->handle] <= MAP_SIZE) {
		pthread_spin_lock(&(map_srq_to_wr_queue[map_cq_to_srq[ibcq->handle]]->tail_lock));
	}

        uint8_t disable_fastpath = 0;
	if (!getenv("DISABLE_FASTPATH")) 
	{
            disable_fastpath = 0;
        }
        else 
	{
            disable_fastpath = atoi(getenv("DISABLE_FASTPATH"));
        }

        if (disable_fastpath)
	{
		request_router(IBV_POLL_CQ, req, wc_list, &rsp_header->rsp_size);
        }
        else
	{
 		request_router_shm(csp);
	}

	if (rsp_header->rsp_size % sizeof(struct ibv_wc) != 0)
	{
		printf("[Error] The rsp size is %d while the unit ibn_wc size is %d.", rsp_header->rsp_size, (int)sizeof(struct ibv_wc));
		fflush(stdout);
		pthread_mutex_unlock(csp_mtx);
		return -1;
	}

	int count = rsp_header->rsp_size / sizeof(struct ibv_wc);

	if (PRINT_LOG)
	{
		printf("count = %d, rsp_size = %d, sizeof struct ibv_wc=%d\n", count, rsp_header->rsp_size, (int)sizeof(struct ibv_wc));
		fflush(stdout);
	}

	memcpy((char*)wc, (char const *)wc_list, rsp_header->rsp_size);

	/*
	for (i = 0; i < count; i++) {
		wc[i].wr_id = wc_list[i].wr_id;
		wc[i].status = wc_list[i].status;
		wc[i].opcode = wc_list[i].opcode;
		wc[i].vendor_err = wc_list[i].vendor_err;
		wc[i].byte_len = wc_list[i].byte_len;
		wc[i].imm_data = wc_list[i].imm_data;
		wc[i].qp_num = wc_list[i].qp_num;
		wc[i].src_qp = wc_list[i].src_qp;
		wc[i].wc_flags = wc_list[i].wc_flags;
		wc[i].pkey_index = wc_list[i].pkey_index;
		wc[i].slid = wc_list[i].slid;
		wc[i].sl = wc_list[i].sl;
		wc[i].dlid_path_bits = wc_list[i].dlid_path_bits;
	}
	*/

	if (PRINT_LOG)
	{
		for (i = 0; i < count; i++) {
			printf("=================================\n");
			printf("wc_list[%d].wr_id=%lu\n", i, (unsigned long)wc_list[i].wr_id);
			printf("wc_list[%d].status=%d\n", i, wc_list[i].status);
			printf("wc_list[%d].opcode=%d\n", i, wc_list[i].opcode);
			printf("wc_list[%d].vendor_err=%d\n", i, wc_list[i].vendor_err);
			printf("wc_list[%d].byte_len=%d\n", i, wc_list[i].byte_len);
			printf("wc_list[%d].imm_data=%u\n", i, wc_list[i].imm_data);
			printf("wc_list[%d].qp_num=%d\n", i, wc_list[i].qp_num);
			printf("wc_list[%d].src_qp=%d\n", i, wc_list[i].src_qp);
			printf("wc_list[%d].wc_flags=%d\n", i, wc_list[i].wc_flags);
			printf("wc_list[%d].pkey_index=%d\n", i, wc_list[i].pkey_index);
			printf("wc_list[%d].slid=%d\n", i, wc_list[i].slid);
			printf("wc_list[%d].sl=%d\n", i, wc_list[i].sl);
			printf("wc_list[%d].ldid_path_bits=%d\n", i, wc_list[i].dlid_path_bits);
			fflush(stdout);
		}
	}

	/* Copying shared memory here if it is a receve queue. */
	
	//struct node *n_ptr = NULL;
	for (i = 0; i < count; i++) {
		if ((wc_list[i].opcode) & IBV_WC_RECV) {
			//clock_gettime(CLOCK_REALTIME, &stt);
			
			if (map_cq_to_srq[ibcq->handle] > MAP_SIZE) {
				wr_queue = map_cq_to_wr_queue[ibcq->handle];
			}
			else {
				wr_queue = map_srq_to_wr_queue[map_cq_to_srq[ibcq->handle]];
			}

			// we are already in critical section
			// pthread_spin_lock(&(wr_queue->tail_lock));
			wr_index = wr_queue->tail;
			wr_queue->tail++;
			if (wr_queue->tail >= WR_QUEUE_SIZE) {
				wr_queue->tail = 0;
			}
			// pthread_spin_unlock(&(wr_queue->tail_lock));

			wr_p = wr_queue->queue + wr_index;

	            //clock_gettime(CLOCK_REALTIME, &ett);
                //   time_us = (ett.tv_sec - stt.tv_sec) * 1000000 +
                //   (ett.tv_nsec - stt.tv_nsec) / 1000;
                //if (time_us >= 5)
				//printf("@@ mempool_get --> %d\n", time_us);
				//fflush(stdout);

			/* freeflow retrieve addresses */
			if (wr_p->sge_num) {
				s = (struct sge_record*)(wr_p->sge_queue);
				for (j = 0; j < wr_p->sge_num; j++) {
					if (s->mr_addr != s->shm_addr)
					{
						memcpy(s->mr_addr, s->shm_addr, s->length);	
					}
					s++;
				}
				free(wr_p->sge_queue);
			}
		}
	}

	wmb();
	csp->state = IDLE;
	// patch SRQ
	if (map_cq_to_srq[ibcq->handle] <= MAP_SIZE) {
		pthread_spin_unlock(&(map_srq_to_wr_queue[map_cq_to_srq[ibcq->handle]]->tail_lock));
	}
	pthread_mutex_unlock(csp_mtx);

	//clock_gettime(CLOCK_REALTIME, &et);
    //    time_us = (et.tv_sec - st.tv_sec) * 1000000 +
    //                        (et.tv_nsec - st.tv_nsec) / 1000;

	//if (count > 0 && PRINT_LOG)
	//if (count > 0 && time_us >= 5)
	//{
	//	printf("##> poll_cq_delay = %d us, req size = %d Bytes\n", time_us, rsp_size);
	//	fflush(stdout);
	//}

	return count;
}

int ibv_cmd_req_notify_cq(struct ibv_cq *ibcq, int solicited_only)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_req_notify_cq ####\n");
		fflush(stdout);
	}

	struct IBV_REQ_NOTIFY_CQ_REQ req_body;
	req_body.cq_handle = ibcq->handle;
	req_body.solicited_only = solicited_only;
	
	struct IBV_REQ_NOTIFY_CQ_RSP rsp;
	int rsp_size;
	request_router(IBV_REQ_NOTIFY_CQ, &req_body, &rsp, &rsp_size);	

	return rsp.ret;
}

int ibv_cmd_resize_cq(struct ibv_cq *cq, int cqe,
		      struct ibv_resize_cq *cmd, size_t cmd_size,
		      struct ibv_resize_cq_resp *resp, size_t resp_size)
{
	printf("#### ibv_cmd_resize_cq ####\n");
	fflush(stdout);

	IBV_INIT_CMD_RESP(cmd, cmd_size, RESIZE_CQ, resp, resp_size);
	cmd->cq_handle = cq->handle;
	cmd->cqe       = cqe;

	/*if (write(cq->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);

	cq->cqe = resp->cqe;

	return 0;
}

int ibv_cmd_destroy_cq(struct ibv_cq *cq)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_destroy_cq ####\n");
		fflush(stdout);
	}

	struct IBV_DESTROY_CQ_REQ req_body;
	req_body.cq_handle = cq->handle;
	
	struct IBV_DESTROY_CQ_RSP rsp;
	int rsp_size;
	request_router(IBV_DESTROY_CQ, &req_body, &rsp, &rsp_size);	

	return rsp.ret;
}

int ibv_cmd_create_srq(struct ibv_pd *pd,
		       struct ibv_srq *srq, struct ibv_srq_init_attr *attr,
		       struct ibv_create_srq *cmd, size_t cmd_size,
		       struct ibv_create_srq_resp *resp, size_t resp_size)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_create_srq ####\n");
		fflush(stdout);
	}

	struct IBV_CREATE_SRQ_REQ req_body;
	req_body.pd_handle = pd->handle;
	memcpy(&(req_body.attr), attr, sizeof(struct ibv_srq_init_attr));
	
	struct IBV_CREATE_SRQ_RSP rsp;
	int rsp_size;
	request_router(IBV_CREATE_SRQ, &req_body, &rsp, &rsp_size);	

	srq->handle = rsp.srq_handle;
	srq->context = pd->context;

	if (attr->attr.max_wr > WR_QUEUE_SIZE) {
		printf("Freeflow WARNING: attempt to create a very large SRQ. This may cause problems.\n");
	}

	int fd = shm_open(rsp.shm_name, O_CREAT | O_RDWR, 0666);
	if (ftruncate(fd, sizeof(struct CtrlShmPiece))) {
		printf("[Error] Fail to mount shm %s\n", rsp.shm_name);
		fflush(stdout);
	}
	void* shm_p = mmap(0, sizeof(struct CtrlShmPiece), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
	srq_shm_map[srq->handle] = (struct CtrlShmPiece *)shm_p;

	map_srq_to_wr_queue[srq->handle] = (struct wr_queue*)malloc(sizeof(struct wr_queue));
	map_srq_to_wr_queue[srq->handle]->queue = (struct wr*)malloc(sizeof(struct wr) * WR_QUEUE_SIZE);
	map_srq_to_wr_queue[srq->handle]->head = 0;
	map_srq_to_wr_queue[srq->handle]->tail = 0;
	pthread_spin_init(&(map_srq_to_wr_queue[srq->handle]->head_lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(map_srq_to_wr_queue[srq->handle]->tail_lock), PTHREAD_PROCESS_PRIVATE);

	return 0;
}

int ibv_cmd_create_srq_ex(struct ibv_context *context,
			  struct verbs_srq *srq, int vsrq_sz,
			  struct ibv_srq_init_attr_ex *attr_ex,
			  struct ibv_create_xsrq *cmd, size_t cmd_size,
			  struct ibv_create_srq_resp *resp, size_t resp_size)
{
	printf("#### ibv_cmd_create_srq_ex ####\n");
	fflush(stdout);

	struct verbs_xrcd *vxrcd = NULL;

	IBV_INIT_CMD_RESP(cmd, cmd_size, CREATE_XSRQ, resp, resp_size);

	if (attr_ex->comp_mask >= IBV_SRQ_INIT_ATTR_RESERVED)
		return ENOSYS;

	if (!(attr_ex->comp_mask & IBV_SRQ_INIT_ATTR_PD))
		return EINVAL;

	cmd->user_handle = (uintptr_t) srq;
	cmd->pd_handle   = attr_ex->pd->handle;
	cmd->max_wr      = attr_ex->attr.max_wr;
	cmd->max_sge     = attr_ex->attr.max_sge;
	cmd->srq_limit   = attr_ex->attr.srq_limit;

	cmd->srq_type = (attr_ex->comp_mask & IBV_SRQ_INIT_ATTR_TYPE) ?
			attr_ex->srq_type : IBV_SRQT_BASIC;
	if (attr_ex->comp_mask & IBV_SRQ_INIT_ATTR_XRCD) {
		if (!(attr_ex->comp_mask & IBV_SRQ_INIT_ATTR_CQ))
			return EINVAL;

		vxrcd = container_of(attr_ex->xrcd, struct verbs_xrcd, xrcd);
		cmd->xrcd_handle = vxrcd->handle;
		cmd->cq_handle   = attr_ex->cq->handle;
	}

	/*if (write(context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);

	srq->srq.handle = resp->srq_handle;
	srq->srq.context = context;
	srq->srq.srq_context = attr_ex->srq_context;
	srq->srq.pd = attr_ex->pd;
	srq->srq.events_completed = 0;
	pthread_mutex_init(&srq->srq.mutex, NULL);
	pthread_cond_init(&srq->srq.cond, NULL);

	/*
	 * check that the last field is available.
	 * If it is than all the others exist as well
	 */
	if (vext_field_avail(struct verbs_srq, srq_num, vsrq_sz)) {
		srq->comp_mask = IBV_SRQ_INIT_ATTR_TYPE;
		srq->srq_type = (attr_ex->comp_mask & IBV_SRQ_INIT_ATTR_TYPE) ?
				attr_ex->srq_type : IBV_SRQT_BASIC;

		if (srq->srq_type == IBV_SRQT_XRC) {
			srq->comp_mask |= VERBS_SRQ_NUM;
			srq->srq_num = resp->srqn;
		}

		if (attr_ex->comp_mask & IBV_SRQ_INIT_ATTR_XRCD) {
			srq->comp_mask |= VERBS_SRQ_XRCD;
			srq->xrcd = vxrcd;
		}

		if (attr_ex->comp_mask & IBV_SRQ_INIT_ATTR_CQ) {
			srq->comp_mask |= VERBS_SRQ_CQ;
			srq->cq = attr_ex->cq;
		}
	}

	attr_ex->attr.max_wr = resp->max_wr;
	attr_ex->attr.max_sge = resp->max_sge;

	return 0;
}

int ibv_cmd_modify_srq(struct ibv_srq *srq,
		       struct ibv_srq_attr *srq_attr,
		       int srq_attr_mask,
		       struct ibv_modify_srq *cmd, size_t cmd_size)
{
	//if (PRINT_LOG)
	//{
		printf("#### ibv_cmd_modify_srq ####\n");
		fflush(stdout);
	//}

	struct IBV_MODIFY_SRQ_REQ req_body;
	req_body.srq_handle = srq->handle;
	memcpy(&req_body.attr, srq_attr, sizeof(struct ibv_srq_attr));
	req_body.srq_attr_mask = srq_attr_mask;
	
	struct IBV_MODIFY_SRQ_RSP rsp;
	int rsp_size;
	request_router(IBV_MODIFY_SRQ, &req_body, &rsp, &rsp_size);
	return rsp.ret;
}

int ibv_cmd_query_srq(struct ibv_srq *srq, struct ibv_srq_attr *srq_attr,
		      struct ibv_query_srq *cmd, size_t cmd_size)
{
	printf("#### ibv_cmd_query_srq ####\n");
	fflush(stdout);

	struct ibv_query_srq_resp resp;

	IBV_INIT_CMD_RESP(cmd, cmd_size, QUERY_SRQ, &resp, sizeof resp);
	cmd->srq_handle = srq->handle;
	cmd->reserved   = 0;

	/*if (write(srq->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

	VALGRIND_MAKE_MEM_DEFINED(&resp, sizeof resp);

	srq_attr->max_wr    = resp.max_wr;
	srq_attr->max_sge   = resp.max_sge;
	srq_attr->srq_limit = resp.srq_limit;

	return 0;
}

int ibv_cmd_destroy_srq(struct ibv_srq *srq)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_destroy_srq ####\n");
		fflush(stdout);
	}

	struct IBV_DESTROY_SRQ_REQ req_body;
	req_body.srq_handle = srq->handle;
	
	struct IBV_DESTROY_SRQ_RSP rsp;
	int rsp_size;
	request_router(IBV_DESTROY_SRQ, &req_body, &rsp, &rsp_size);	

	return rsp.ret;
}

int ibv_cmd_create_qp(struct ibv_pd *pd,
		      struct ibv_qp *qp, struct ibv_qp_init_attr *attr,
		      struct ibv_create_qp *cmd, size_t cmd_size,
		      struct ibv_create_qp_resp *resp, size_t resp_size)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_create_qp ####\n");
		fflush(stdout);
	}

	struct IBV_CREATE_QP_REQ req_body;
	// void* wr_queue, *p;

	req_body.pd_handle = pd->handle;
	req_body.qp_type = attr->qp_type;
	req_body.sq_sig_all = attr->sq_sig_all;
	req_body.send_cq_handle = attr->send_cq->handle;
	req_body.recv_cq_handle = attr->recv_cq->handle;
	req_body.srq_handle = attr->srq ? attr->srq->handle : 0;
	req_body.cap.max_recv_sge    = attr->cap.max_recv_sge;
	req_body.cap.max_send_sge    = attr->cap.max_send_sge;
	req_body.cap.max_recv_wr     = attr->cap.max_recv_wr;
	req_body.cap.max_send_wr     = attr->cap.max_send_wr;
	req_body.cap.max_inline_data = attr->cap.max_inline_data;

	struct IBV_CREATE_QP_RSP rsp;
	int rsp_size;
	request_router(IBV_CREATE_QP, &req_body, &rsp, &rsp_size);

	qp->handle 		  = rsp.handle;
	qp->qp_num 		  = rsp.qp_num;
	qp->context		  = pd->context;

	if (abi_ver > 3) {
		attr->cap.max_recv_sge    = rsp.cap.max_recv_sge;
		attr->cap.max_send_sge    = rsp.cap.max_send_sge;
		attr->cap.max_recv_wr     = rsp.cap.max_recv_wr;
		attr->cap.max_send_wr     = rsp.cap.max_send_wr;
		attr->cap.max_inline_data = rsp.cap.max_inline_data;
	}
	
	// Disable inline data
	attr->cap.max_inline_data = 0;

	// Patch SRQ
	if (qp->srq) {
		map_cq_to_srq[qp->recv_cq->handle] = qp->srq->handle;
	}
	else {
		map_cq_to_srq[qp->recv_cq->handle] = MAP_SIZE + 1;
	}

	int fd = shm_open(rsp.shm_name, O_CREAT | O_RDWR, 0666);
	if (ftruncate(fd, sizeof(struct CtrlShmPiece))) {
		printf("[Error] Fail to mount shm %s\n", rsp.shm_name);
		fflush(stdout);
	}
	void* shm_p = mmap(0, sizeof(struct CtrlShmPiece), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 0);
        
    if (!shm_p)
	{
		printf("[Error] Fail to mount shm %s\n", rsp.shm_name);
		fflush(stdout);
	}
        else
	{
		printf("[INFO] Succeed to mount shm %s\n", rsp.shm_name);
		fflush(stdout);
	}

        qp_shm_map[qp->handle] = (struct CtrlShmPiece *)shm_p;
        pthread_mutex_init(&(qp_shm_mtx_map[qp->handle]), NULL); 

	return 0;

}

int ibv_cmd_open_qp(struct ibv_context *context, struct verbs_qp *qp,
			int vqp_sz,
			struct ibv_qp_open_attr *attr,
			struct ibv_open_qp *cmd, size_t cmd_size,
			struct ibv_create_qp_resp *resp, size_t resp_size)
{
	printf("#### ibv_cmd_open_qp ####\n");
	fflush(stdout);

	struct verbs_xrcd *xrcd;
	IBV_INIT_CMD_RESP(cmd, cmd_size, OPEN_QP, resp, resp_size);

	if (attr->comp_mask >= IBV_QP_OPEN_ATTR_RESERVED)
		return ENOSYS;

	if (!(attr->comp_mask & IBV_QP_OPEN_ATTR_XRCD) ||
	    !(attr->comp_mask & IBV_QP_OPEN_ATTR_NUM) ||
	    !(attr->comp_mask & IBV_QP_OPEN_ATTR_TYPE))
		return EINVAL;

	xrcd = container_of(attr->xrcd, struct verbs_xrcd, xrcd);
	cmd->user_handle = (uintptr_t) qp;
	cmd->pd_handle   = xrcd->handle;
	cmd->qpn         = attr->qp_num;
	cmd->qp_type     = attr->qp_type;

	/*if (write(context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);

	qp->qp.handle     = resp->qp_handle;
	qp->qp.context    = context;
	qp->qp.qp_context = attr->qp_context;
	qp->qp.pd	  = NULL;
	qp->qp.send_cq	  = qp->qp.recv_cq = NULL;
	qp->qp.srq	  = NULL;
	qp->qp.qp_num	  = attr->qp_num;
	qp->qp.qp_type	  = attr->qp_type;
	qp->qp.state	  = IBV_QPS_UNKNOWN;
	qp->qp.events_completed = 0;
	pthread_mutex_init(&qp->qp.mutex, NULL);
	pthread_cond_init(&qp->qp.cond, NULL);

	qp->comp_mask = 0;
	if (vext_field_avail(struct verbs_qp, xrcd, vqp_sz)) {
		qp->comp_mask |= VERBS_QP_XRCD;
		qp->xrcd	  = xrcd;
	}

	return 0;
}

int ibv_cmd_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		     int attr_mask,
		     struct ibv_qp_init_attr *init_attr,
		     struct ibv_query_qp *cmd, size_t cmd_size)
{
	printf("#### ibv_cmd_query_qp ####\n");
	fflush(stdout);

	struct ibv_query_qp_resp resp;

	IBV_INIT_CMD_RESP(cmd, cmd_size, QUERY_QP, &resp, sizeof resp);
	cmd->qp_handle = qp->handle;
	cmd->attr_mask = attr_mask;

	/*if (write(qp->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;*/

        struct IBV_QUERY_QP_REQ req_body;
        struct IBV_QUERY_QP_RSP rsp;
        int rsp_size;

	memset(req_body.cmd, 0, 100);
	memcpy(req_body.cmd, cmd, cmd_size);
	req_body.cmd_size = cmd_size;
        request_router(IBV_QUERY_QP, &req_body, &rsp, &rsp_size);
	memcpy(&resp, &rsp.resp, sizeof resp);

	VALGRIND_MAKE_MEM_DEFINED(&resp, sizeof resp);

	attr->qkey                          = resp.qkey;
	attr->rq_psn                        = resp.rq_psn;
	attr->sq_psn                        = resp.sq_psn;
	attr->dest_qp_num                   = resp.dest_qp_num;
	attr->qp_access_flags               = resp.qp_access_flags;
	attr->pkey_index                    = resp.pkey_index;
	attr->alt_pkey_index                = resp.alt_pkey_index;
	attr->qp_state                      = resp.qp_state;
	attr->cur_qp_state                  = resp.cur_qp_state;
	attr->path_mtu                      = resp.path_mtu;
	attr->path_mig_state                = resp.path_mig_state;
	attr->sq_draining                   = resp.sq_draining;
	attr->max_rd_atomic                 = resp.max_rd_atomic;
	attr->max_dest_rd_atomic            = resp.max_dest_rd_atomic;
	attr->min_rnr_timer                 = resp.min_rnr_timer;
	attr->port_num                      = resp.port_num;
	attr->timeout                       = resp.timeout;
	attr->retry_cnt                     = resp.retry_cnt;
	attr->rnr_retry                     = resp.rnr_retry;
	attr->alt_port_num                  = resp.alt_port_num;
	attr->alt_timeout                   = resp.alt_timeout;
	attr->cap.max_send_wr               = resp.max_send_wr;
	attr->cap.max_recv_wr               = resp.max_recv_wr;
	attr->cap.max_send_sge              = resp.max_send_sge;
	attr->cap.max_recv_sge              = resp.max_recv_sge;
	attr->cap.max_inline_data           = resp.max_inline_data;

	memcpy(attr->ah_attr.grh.dgid.raw, resp.dest.dgid, 16);
	attr->ah_attr.grh.flow_label        = resp.dest.flow_label;
	attr->ah_attr.dlid                  = resp.dest.dlid;
	attr->ah_attr.grh.sgid_index        = resp.dest.sgid_index;
	attr->ah_attr.grh.hop_limit         = resp.dest.hop_limit;
	attr->ah_attr.grh.traffic_class     = resp.dest.traffic_class;
	attr->ah_attr.sl                    = resp.dest.sl;
	attr->ah_attr.src_path_bits         = resp.dest.src_path_bits;
	attr->ah_attr.static_rate           = resp.dest.static_rate;
	attr->ah_attr.is_global             = resp.dest.is_global;
	attr->ah_attr.port_num              = resp.dest.port_num;

	memcpy(attr->alt_ah_attr.grh.dgid.raw, resp.alt_dest.dgid, 16);
	attr->alt_ah_attr.grh.flow_label    = resp.alt_dest.flow_label;
	attr->alt_ah_attr.dlid              = resp.alt_dest.dlid;
	attr->alt_ah_attr.grh.sgid_index    = resp.alt_dest.sgid_index;
	attr->alt_ah_attr.grh.hop_limit     = resp.alt_dest.hop_limit;
	attr->alt_ah_attr.grh.traffic_class = resp.alt_dest.traffic_class;
	attr->alt_ah_attr.sl                = resp.alt_dest.sl;
	attr->alt_ah_attr.src_path_bits     = resp.alt_dest.src_path_bits;
	attr->alt_ah_attr.static_rate       = resp.alt_dest.static_rate;
	attr->alt_ah_attr.is_global         = resp.alt_dest.is_global;
	attr->alt_ah_attr.port_num          = resp.alt_dest.port_num;

	init_attr->qp_context               = qp->qp_context;
	init_attr->send_cq                  = qp->send_cq;
	init_attr->recv_cq                  = qp->recv_cq;
	init_attr->srq                      = qp->srq;
	init_attr->qp_type                  = qp->qp_type;
	init_attr->cap.max_send_wr          = resp.max_send_wr;
	init_attr->cap.max_recv_wr          = resp.max_recv_wr;
	init_attr->cap.max_send_sge         = resp.max_send_sge;
	init_attr->cap.max_recv_sge         = resp.max_recv_sge;
	init_attr->cap.max_inline_data      = resp.max_inline_data;
	init_attr->sq_sig_all               = resp.sq_sig_all;
	qp->state			    = attr->cur_qp_state;

	return 0;
}

int ibv_cmd_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		      int attr_mask,
		      struct ibv_modify_qp *cmd, size_t cmd_size)
{
	struct IBV_MODIFY_QP_REQ req_body;
	memcpy(&req_body.attr, attr, sizeof(struct ibv_qp_attr));
	req_body.attr_mask = attr_mask;
	req_body.handle = qp->handle;
	
	struct IBV_MODIFY_QP_RSP rsp;
	int rsp_size;
	request_router(IBV_MODIFY_QP, &req_body, &rsp, &rsp_size);

	if (attr_mask & IBV_QP_STATE)
        qp->state = attr->qp_state;

    	if (PRINT_LOG)
    	{
    		printf("QP in req: %d, and QP in rsp: %d. size of attr = %d\n", qp->handle, rsp.handle, sizeof(struct ibv_qp_attr));
    		fflush(stdout);
    	}

	if (rsp.ret)
		errno = rsp.ret;
	return rsp.ret;
}

int ibv_cmd_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr,
		      struct ibv_send_wr **bad_wr)
{	
	//struct timespec st, et, stt, ett;
	//int time_us;
	//clock_gettime(CLOCK_REALTIME, &st);
	//struct ibv_post_send_resp resp;
	struct ibv_send_wr       *i;
	struct ibv_send_wr       *n, *tmp = NULL;
	struct ibv_sge           *s;
	uint32_t				 *ah, ah_count = 0;
	uint32_t                 wr_count = 0, sge_count = 0, ret_errno;
	int                       j;

	char                      *mr, *shm, *addr;
    struct mr_shm             *p;
    struct CtrlShmPiece       *csp = qp_shm_map[ibqp->handle];
    pthread_mutex_t           *csp_mtx = &(qp_shm_mtx_map[ibqp->handle]);
	struct FfrRequestHeader   *header = (struct FfrRequestHeader*)(csp->req);
	struct ibv_post_send      *cmd = (struct ibv_post_send*)(csp->req + sizeof(struct FfrRequestHeader));
	struct IBV_POST_SEND_RSP  *rsp = (struct IBV_POST_SEND_RSP*)(csp->rsp + sizeof(struct FfrResponseHeader));
	

    if (PRINT_LOG)
    {
    	printf("#### ibv_cmd_post_send ####\n");
		fflush(stdout);
    }

	for (i = wr; i; i = i->next) {
		wr_count++;
		sge_count += i->num_sge;
	}

	if (ibqp->qp_type == IBV_QPT_UD) {
		ah_count = wr_count;
	}

	// Entering critical section
	pthread_mutex_lock(csp_mtx);

	header->client_id = ffr_client_id;
	header->func = IBV_POST_SEND;
	header->body_size = sizeof *cmd + wr_count * sizeof *n + sge_count * sizeof *s + ah_count * sizeof(uint32_t);

	//IBV_INIT_CMD_RESP(cmd, req_body.wr_size, POST_SEND, &resp, sizeof resp);
	cmd->qp_handle = ibqp->handle;
	cmd->wr_count  = wr_count;
	cmd->sge_count = sge_count;

	n = (struct ibv_send_wr *) ((void *) cmd + sizeof *cmd);
	s = (struct ibv_sge *) (n + wr_count);
	ah = (uint32_t*) (s + sge_count);

	tmp = n;
	for (i = wr; i; i = i->next) {
		memcpy(tmp, i, sizeof(struct ibv_send_wr));
		if (ibqp->qp_type == IBV_QPT_UD) {
			*ah = i->wr.ud.ah->handle;
			ah = ah + 1;
		}

		if (tmp->num_sge) {
			/* freeflow copy data */
			memcpy(s, i->sg_list, tmp->num_sge * sizeof *s);
			for (j = 0; j < tmp->num_sge; j++) {

                //clock_gettime(CLOCK_REALTIME, &stt);
				p = (struct mr_shm*)mempool_get(map_lkey_to_mrshm, s[j].lkey);
                //clock_gettime(CLOCK_REALTIME, &ett);
                //time_us = (ett.tv_sec - stt.tv_sec) * 1000000 +
                //(ett.tv_nsec - stt.tv_nsec) / 1000;
                //if (time_us >= 5)
				//printf("mempool_get --> %d\n", time_us);
				//fflush(stdout);

				mr = p->mr;
				shm = p->shm_ptr;

				addr = (char*)(s[j].addr);
				if (shm + (addr - mr) != addr)
				{
					memcpy(shm + (addr - mr), addr, s[j].length);	
				}

				s[j].addr = addr - mr;

				if (PRINT_LOG)
				{
					printf("!!!!!!!!!! length=%u,addr=%lu,lkey=%u,mr_ptr=%lu,shm_ptr=%lu,original_addr=%lu\n", s[j].length, s[j].addr, s[j].lkey, (uint64_t)mr, (uint64_t)shm, (uint64_t)addr);
					fflush(stdout);
					printf("wr=%lu, sge=%lu, sizeof send_wr=%lu\n", (uint64_t)n, (uint64_t)s, sizeof(*n));
					fflush(stdout);
				}
			}

			s += tmp->num_sge;
		}

		tmp++;
	}

        uint8_t disable_fastpath = 0;
        if (!getenv("DISABLE_FASTPATH"))
        {
            disable_fastpath = 0;
        }
        else
        {
            disable_fastpath = atoi(getenv("DISABLE_FASTPATH"));
        }

	if (disable_fastpath)
	{
		struct IBV_POST_SEND_REQ req_body;
                req_body.wr_size = header->body_size;
		req_body.wr = cmd;
		int rsp_size;
		request_router(IBV_POST_SEND, &req_body, rsp, &rsp_size);
	}
	else
		request_router_shm(csp);
 
	wr_count = rsp->bad_wr;
	if (wr_count) {
		i = wr;
		while (--wr_count)
			i = i->next;
		*bad_wr = i;
	} else if (rsp->ret_errno) {
		*bad_wr = wr;
	}
	
	ret_errno = rsp->ret_errno;
	wmb();
	csp->state = IDLE;
	pthread_mutex_unlock(csp_mtx);

	//clock_gettime(CLOCK_REALTIME, &et);
    //    time_us = (et.tv_sec - st.tv_sec) * 1000000 +
    //                        (et.tv_nsec - st.tv_nsec) / 1000;

	//if (PRINT_LOG)
	//if (time_us >= 5)
	//{
	//	printf("--------------> post_send_delay = %d us, wr_count = %d, sge_count = %d\n", time_us, wr_count, sge_count);
	//	fflush(stdout);
	//}

	return ret_errno;
}

int ibv_cmd_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr,
		      struct ibv_recv_wr **bad_wr)
{
	//struct ibv_post_recv_resp resp;
	struct ibv_recv_wr        *i;
	struct ibv_recv_wr        *n = NULL, *tmp = NULL;
	struct ibv_sge            *s;
	uint32_t                  wr_count = 0, sge_count = 0, ret_errno;
	int                       j, wr_index;
	char                      *mr = NULL, *addr = NULL;
	struct mr_shm             *p;
	struct wr                 *wr_p;
	struct sge_record         *sge_p;
	struct wr_queue           *wr_queue;
    struct CtrlShmPiece       *csp = qp_shm_map[ibqp->handle];
    pthread_mutex_t           *csp_mtx = &(qp_shm_mtx_map[ibqp->handle]);
	struct FfrRequestHeader   *header = (struct FfrRequestHeader*)(csp->req);
	struct ibv_post_recv      *cmd = (struct ibv_post_recv*)(csp->req + sizeof(struct FfrRequestHeader));
	struct IBV_POST_RECV_RSP  *rsp = (struct IBV_POST_RECV_RSP*)(csp->rsp + sizeof(struct FfrResponseHeader));

	for (i = wr; i; i = i->next) {
		wr_count++;
		sge_count += i->num_sge;
	}

	// Entering critical section
	pthread_mutex_lock(csp_mtx);

	header->client_id = ffr_client_id;
	header->func = IBV_POST_RECV;
	header->body_size = sizeof *cmd + wr_count * sizeof *n + sge_count * sizeof *s;

	// IBV_INIT_CMD_RESP(cmd, req_body.wr_size, POST_RECV, &resp, sizeof resp);
	cmd->qp_handle = ibqp->handle;
	cmd->wr_count  = wr_count;
	cmd->sge_count = sge_count;

	n = (struct ibv_recv_wr *) ((void *) cmd + sizeof *cmd);
	s = (struct ibv_sge *) (n + wr_count);

	wr_queue = map_cq_to_wr_queue[ibqp->recv_cq->handle];

	tmp = n;
	for (i = wr; i; i = i->next) {
		memcpy(tmp, i, sizeof(struct ibv_recv_wr));

		// We are already in critical section
		//pthread_spin_lock(&(wr_queue->head_lock));
		wr_index = wr_queue->head;
		wr_queue->head++;
		if (wr_queue->head >= WR_QUEUE_SIZE) {
			wr_queue->head = 0;
		}
		//pthread_spin_unlock(&(wr_queue->head_lock));

		wr_p = wr_queue->queue + wr_index;
		wr_p->sge_num = tmp->num_sge;

		if (tmp->num_sge) {
			wr_p->sge_queue = malloc(sizeof(struct sge_record) * tmp->num_sge);
			sge_p = (struct sge_record*)wr_p->sge_queue;

			/* freeflow keeps track of offsets */
			tmp->sg_list = s;
			memcpy(s, i->sg_list, tmp->num_sge * sizeof *s);

			for (j = 0; j < tmp->num_sge; j++) {
				p = (struct mr_shm*)mempool_get(map_lkey_to_mrshm, s[j].lkey);
				mr = p->mr;
				
				addr = (char*)(s[j].addr);
				s[j].addr = addr - mr;

				sge_p->length = s[j].length;
				sge_p->mr_addr = mr + s[j].addr;
				sge_p->shm_addr = p->shm_ptr + s[j].addr;
				sge_p++;
			}
			s += tmp->num_sge;
		}

		tmp++;
	}

        uint8_t disable_fastpath = 0;
        if (!getenv("DISABLE_FASTPATH"))
        {
            disable_fastpath = 0;
        }
        else
        {
            disable_fastpath = atoi(getenv("DISABLE_FASTPATH"));
        }

	if (disable_fastpath)
	{
		struct IBV_POST_RECV_REQ req_body;
                req_body.wr_size = header->body_size;
		req_body.wr = cmd;
		int rsp_size;
		request_router(IBV_POST_RECV, &req_body, rsp, &rsp_size);
	}
	else
		request_router_shm(csp);
 
	wr_count = rsp->bad_wr;
	if (wr_count) {
		i = wr;
		while (--wr_count)
			i = i->next;
		*bad_wr = i;
	} else if (rsp->ret_errno) {
		*bad_wr = wr;
	}

	ret_errno = rsp->ret_errno;
	wmb();
	csp->state = IDLE;
	pthread_mutex_unlock(csp_mtx);

	return ret_errno;
}

int ibv_cmd_post_srq_recv(struct ibv_srq *srq, struct ibv_recv_wr *wr,
		      struct ibv_recv_wr **bad_wr)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_post_srq_recv ####\n");
		fflush(stdout);
	}

	struct ibv_recv_wr        *i;
	struct ibv_recv_wr        *n = NULL, *tmp = NULL;
	struct ibv_sge            *s;
	uint32_t                  wr_count = 0, sge_count = 0, ret_errno;
	int                       j, wr_index;
	char                      *mr = NULL, *addr = NULL;
	struct mr_shm             *p;
	struct wr                 *wr_p;
	struct sge_record         *sge_p;
	struct wr_queue           *wr_queue;
    struct CtrlShmPiece       *csp = srq_shm_map[srq->handle];
    pthread_spinlock_t        *csp_mtx = &(map_srq_to_wr_queue[srq->handle]->head_lock);
	struct FfrRequestHeader   *header = (struct FfrRequestHeader*)(csp->req);
	struct ibv_post_srq_recv      *cmd = (struct ibv_post_srq_recv*)(csp->req + sizeof(struct FfrRequestHeader));
	struct IBV_POST_SRQ_RECV_RSP  *rsp = (struct IBV_POST_SRQ_RECV_RSP*)(csp->rsp + sizeof(struct FfrResponseHeader));

	for (i = wr; i; i = i->next) {
		wr_count++;
		sge_count += i->num_sge;
	}

	// Entering critical section
	pthread_spin_lock(csp_mtx);

	header->client_id = ffr_client_id;
	header->func = IBV_POST_SRQ_RECV;
	header->body_size = sizeof *cmd + wr_count * sizeof *n + sge_count * sizeof *s;

	cmd->srq_handle = srq->handle;
	cmd->wr_count  = wr_count;
	cmd->sge_count = sge_count;
	// cmd->wqe_size  = sizeof *n;

	n = (struct ibv_recv_wr *) ((void *) cmd + sizeof *cmd);
	s = (struct ibv_sge *) (n + wr_count);

	wr_queue = map_srq_to_wr_queue[srq->handle];

	tmp = n;
	for (i = wr; i; i = i->next) {
		memcpy(tmp, i, sizeof(struct ibv_recv_wr));

		// We are already in critical section
		//pthread_spin_lock(&(wr_queue->head_lock));
		wr_index = wr_queue->head;
		wr_queue->head++;
		if (wr_queue->head >= WR_QUEUE_SIZE) {
			wr_queue->head = 0;
		}
		//pthread_spin_unlock(&(wr_queue->head_lock));
		wr_p = wr_queue->queue + wr_index;

		wr_p->sge_num = tmp->num_sge;

		if (tmp->num_sge) {
			/* freeflow keeps track of offsets */
			wr_p->sge_queue = malloc(sizeof(struct sge_record) * tmp->num_sge);
			sge_p = (struct sge_record*)wr_p->sge_queue;

			tmp->sg_list = s;
			memcpy(s, i->sg_list, tmp->num_sge * sizeof *s);

			for (j = 0; j < tmp->num_sge; j++) {
				p = (struct mr_shm*)mempool_get(map_lkey_to_mrshm, s[j].lkey);
				mr = p->mr;
				
				addr = (char*)(s[j].addr);
				s[j].addr = addr - mr;

				sge_p->length = s[j].length;
				sge_p->mr_addr = mr + s[j].addr;
				sge_p->shm_addr = p->shm_ptr + s[j].addr;
				sge_p++;
			}
			s += tmp->num_sge;
		}

		tmp++;
	}

        uint8_t disable_fastpath = 0;
        if (!getenv("DISABLE_FASTPATH"))
        {
            disable_fastpath = 0;
        }
        else
        {
            disable_fastpath = atoi(getenv("DISABLE_FASTPATH"));
        }

	if (disable_fastpath)
	{
		struct IBV_POST_SRQ_RECV_REQ req_body;
                req_body.wr_size = header->body_size;
		req_body.wr = cmd;
		int rsp_size;
		request_router(IBV_POST_SRQ_RECV, &req_body, rsp, &rsp_size);
	}
	else
		request_router_shm(csp);

	wr_count = rsp->bad_wr;
	if (wr_count) {
		i = wr;
		while (--wr_count)
			i = i->next;
		*bad_wr = i;
	} else if (rsp->ret_errno) {
		*bad_wr = wr;
	}

	ret_errno = rsp->ret_errno;
	wmb();
	csp->state = IDLE;
	pthread_spin_unlock(csp_mtx);

	return ret_errno;
}

int ibv_cmd_create_ah(struct ibv_pd *pd, struct ibv_ah *ah,
		      struct ibv_ah_attr *attr,
		      struct ibv_create_ah_resp *resp,
		      size_t resp_size)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_create_ah ####\n");
		fflush(stdout);
	}

	printf("attr->dlid; = %d\n", attr->dlid);
	fflush(stdout);
	printf("attr->sl; = %d\n", attr->sl);
	fflush(stdout);
	printf("attr->src_path_bits; = %d\n", attr->src_path_bits);
	fflush(stdout);
	printf("attr->static_rate; = %d\n", attr->static_rate);
	fflush(stdout);
	printf("attr->is_global; = %d\n", attr->is_global);
	fflush(stdout);
	printf("attr->port_num; = %d\n", attr->port_num);
	fflush(stdout);
	printf("attr->grh.flow_label; = %d\n", attr->grh.flow_label);
	fflush(stdout);
	printf("attr->grh.sgid_index; = %d\n", attr->grh.sgid_index);
	fflush(stdout);
	printf("attr->grh.hop_limit; = %d\n", attr->grh.hop_limit);
	printf("attr->grh.traffic_class; = %d\n", attr->grh.traffic_class);
	fflush(stdout);

	struct IBV_CREATE_AH_REQ req_body;
	memcpy(&(req_body.ah_attr), attr, sizeof(struct ibv_ah_attr));
	req_body.pd_handle = pd->handle;
	
	struct IBV_CREATE_AH_RSP rsp;
	int rsp_size;
	request_router(IBV_CREATE_AH, &req_body, &rsp, &rsp_size);

	ah->handle = rsp.ah_handle;
	ah->context = pd->context;
	ah->pd = pd;

	return rsp.ret;
}

int ibv_cmd_destroy_ah(struct ibv_ah *ah)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_destroy_ah ####\n");
		fflush(stdout);
	}

	struct IBV_DESTROY_AH_REQ req_body;
	req_body.ah_handle = ah->handle;
	
	struct IBV_DESTROY_AH_RSP rsp;
	int rsp_size;
	request_router(IBV_DESTROY_AH, &req_body, &rsp, &rsp_size);

	return rsp.ret;
}

int ibv_cmd_destroy_qp(struct ibv_qp *qp)
{
	if (PRINT_LOG)
	{
		printf("#### ibv_cmd_destroy_qp ####\n");
		fflush(stdout);
	}

	struct IBV_DESTROY_QP_REQ req_body;
	req_body.qp_handle = qp->handle;
	
	struct IBV_DESTROY_QP_RSP rsp;
	int rsp_size;
	request_router(IBV_DESTROY_QP, &req_body, &rsp, &rsp_size);	

	// TODO: clean up mempool

	return rsp.ret;
}

int ibv_cmd_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
	//if (PRINT_LOG)
	//{
		printf("#### ibv_cmd_attach_mcast ####\n");
		fflush(stdout);
	//}

	struct ibv_attach_mcast cmd;

	IBV_INIT_CMD(&cmd, sizeof cmd, ATTACH_MCAST);
	memcpy(cmd.gid, gid->raw, sizeof cmd.gid);
	cmd.qp_handle = qp->handle;
	cmd.mlid      = lid;
	cmd.reserved  = 0;

	/*if (write(qp->context->cmd_fd, &cmd, sizeof cmd) != sizeof cmd)
		return errno;*/

	return 0;
}

int ibv_cmd_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
	//if (PRINT_LOG)
	//{
		printf("#### ibv_cmd_detach_mcast ####\n");
		fflush(stdout);
	//}

	struct ibv_detach_mcast cmd;

	IBV_INIT_CMD(&cmd, sizeof cmd, DETACH_MCAST);
	memcpy(cmd.gid, gid->raw, sizeof cmd.gid);
	cmd.qp_handle = qp->handle;
	cmd.mlid      = lid;
	cmd.reserved  = 0;

	/*if (write(qp->context->cmd_fd, &cmd, sizeof cmd) != sizeof cmd)
		return errno;*/

	return 0;
}

static int buffer_is_zero(char *addr, ssize_t size)
{
	return addr[0] == 0 && !memcmp(addr, addr + 1, size - 1);
}

static int get_filters_size(struct ibv_exp_flow_spec *ib_spec,
			    struct ibv_exp_kern_spec *kern_spec,
			    int *ib_filter_size, int *kern_filter_size,
			    enum ibv_exp_flow_spec_type type)
{
	void *ib_spec_filter_mask;
	int curr_kern_filter_size;
	int min_filter_size;

	*ib_filter_size = (ib_spec->hdr.size - sizeof(ib_spec->hdr)) / 2;

	switch (type) {
	case IBV_EXP_FLOW_SPEC_IPV4_EXT:
		min_filter_size =
			offsetof(struct ibv_exp_kern_ipv4_ext_filter, flags) +
			sizeof(kern_spec->ipv4_ext.mask.flags);
		curr_kern_filter_size = min_filter_size;
		ib_spec_filter_mask = (void *)&ib_spec->ipv4_ext.val +
			*ib_filter_size;
		break;
	case IBV_EXP_FLOW_SPEC_IPV6_EXT:
		min_filter_size =
			offsetof(struct ibv_exp_kern_ipv6_ext_filter,
				 hop_limit) +
			sizeof(kern_spec->ipv6_ext.mask.hop_limit);
		curr_kern_filter_size = min_filter_size;
		ib_spec_filter_mask = (void *)&ib_spec->ipv6_ext.val +
			*ib_filter_size;
		break;
	case IBV_EXP_FLOW_SPEC_VXLAN_TUNNEL:
		min_filter_size =
			offsetof(struct ibv_exp_kern_tunnel_filter,
				 tunnel_id) +
			sizeof(kern_spec->tunnel.mask.tunnel_id);
		curr_kern_filter_size = min_filter_size;
		ib_spec_filter_mask = (void *)&ib_spec->tunnel.val +
			*ib_filter_size;
		break;
	default:
		return EINVAL;
	}

	if (*ib_filter_size < min_filter_size)
		return EINVAL;

	if (*ib_filter_size > curr_kern_filter_size &&
	    !buffer_is_zero(ib_spec_filter_mask + curr_kern_filter_size,
			    *ib_filter_size - curr_kern_filter_size))
		return EOPNOTSUPP;

	*kern_filter_size = min(curr_kern_filter_size, *ib_filter_size);

	return 0;
}

static int ib_spec_to_kern_spec(struct ibv_exp_flow_spec *ib_spec,
				struct ibv_exp_kern_spec *kern_spec,
				int is_exp)
{
	int kern_filter_size;
	int ib_filter_size;
	int ret;

	if (!is_exp && (ib_spec->hdr.type & IBV_EXP_FLOW_SPEC_INNER))
		return EINVAL;

	kern_spec->hdr.type = ib_spec->hdr.type;
	switch (ib_spec->hdr.type) {
	case IBV_EXP_FLOW_SPEC_ETH:
	case IBV_EXP_FLOW_SPEC_ETH | IBV_EXP_FLOW_SPEC_INNER:
		kern_spec->eth.size = sizeof(struct ibv_kern_spec_eth);
		memcpy(&kern_spec->eth.val, &ib_spec->eth.val,
		       sizeof(struct ibv_exp_flow_eth_filter));
		memcpy(&kern_spec->eth.mask, &ib_spec->eth.mask,
		       sizeof(struct ibv_exp_flow_eth_filter));
		break;
	case IBV_EXP_FLOW_SPEC_IB:
		if (!is_exp)
			return EINVAL;
		kern_spec->ib.size = sizeof(struct ibv_kern_spec_ib);
		memcpy(&kern_spec->ib.val, &ib_spec->ib.val,
		       sizeof(struct ibv_exp_flow_ib_filter));
		memcpy(&kern_spec->ib.mask, &ib_spec->ib.mask,
		       sizeof(struct ibv_exp_flow_ib_filter));
		break;
	case IBV_EXP_FLOW_SPEC_IPV4:
	case IBV_EXP_FLOW_SPEC_IPV4 | IBV_EXP_FLOW_SPEC_INNER:
		kern_spec->ipv4.size = sizeof(struct ibv_kern_spec_ipv4);
		memcpy(&kern_spec->ipv4.val, &ib_spec->ipv4.val,
		       sizeof(struct ibv_exp_flow_ipv4_filter));
		memcpy(&kern_spec->ipv4.mask, &ib_spec->ipv4.mask,
		       sizeof(struct ibv_exp_flow_ipv4_filter));
		break;
	case IBV_EXP_FLOW_SPEC_IPV6:
	case IBV_EXP_FLOW_SPEC_IPV6 | IBV_EXP_FLOW_SPEC_INNER:
		if (!is_exp)
			return EINVAL;
		kern_spec->ipv6.size = sizeof(struct ibv_exp_kern_spec_ipv6);
		memcpy(&kern_spec->ipv6.val, &ib_spec->ipv6.val,
		       sizeof(struct ibv_exp_flow_ipv6_filter));
		memcpy(&kern_spec->ipv6.mask, &ib_spec->ipv6.mask,
		       sizeof(struct ibv_exp_flow_ipv6_filter));
		break;
	case IBV_EXP_FLOW_SPEC_IPV6_EXT:
	case IBV_EXP_FLOW_SPEC_IPV6_EXT | IBV_EXP_FLOW_SPEC_INNER:
		if (!is_exp)
			return EINVAL;
		ret = get_filters_size(ib_spec, kern_spec,
				       &ib_filter_size, &kern_filter_size,
				       IBV_EXP_FLOW_SPEC_IPV6_EXT);
		if (ret)
			return ret;

		kern_spec->hdr.type = ib_spec->hdr.type & IBV_EXP_FLOW_SPEC_INNER ?
			IBV_EXP_FLOW_SPEC_IPV6 | IBV_EXP_FLOW_SPEC_INNER :
			IBV_EXP_FLOW_SPEC_IPV6;
		kern_spec->ipv6_ext.size = sizeof(struct ibv_exp_kern_spec_ipv6_ext);
		memcpy(&kern_spec->ipv6_ext.val, &ib_spec->ipv6_ext.val,
		       kern_filter_size);
		memcpy(&kern_spec->ipv6_ext.mask, (void *)&ib_spec->ipv6_ext.val
		       + ib_filter_size, kern_filter_size);
		break;
	case IBV_EXP_FLOW_SPEC_IPV4_EXT:
	case IBV_EXP_FLOW_SPEC_IPV4_EXT | IBV_EXP_FLOW_SPEC_INNER:
		if (!is_exp)
			return EINVAL;
		ret = get_filters_size(ib_spec, kern_spec,
				       &ib_filter_size, &kern_filter_size,
				       IBV_EXP_FLOW_SPEC_IPV4_EXT);
		if (ret)
			return ret;

		kern_spec->hdr.type = ib_spec->hdr.type & IBV_EXP_FLOW_SPEC_INNER ?
			IBV_EXP_FLOW_SPEC_IPV4 | IBV_EXP_FLOW_SPEC_INNER :
			IBV_EXP_FLOW_SPEC_IPV4;
		kern_spec->ipv4_ext.size = sizeof(struct
						  ibv_exp_kern_spec_ipv4_ext);
		memcpy(&kern_spec->ipv4_ext.val, &ib_spec->ipv4_ext.val,
		       kern_filter_size);
		memcpy(&kern_spec->ipv4_ext.mask, (void *)&ib_spec->ipv4_ext.val
		       + ib_filter_size, kern_filter_size);
		break;
	case IBV_EXP_FLOW_SPEC_TCP:
	case IBV_EXP_FLOW_SPEC_UDP:
	case IBV_EXP_FLOW_SPEC_TCP | IBV_EXP_FLOW_SPEC_INNER:
	case IBV_EXP_FLOW_SPEC_UDP | IBV_EXP_FLOW_SPEC_INNER:
		kern_spec->tcp_udp.size = sizeof(struct ibv_kern_spec_tcp_udp);
		memcpy(&kern_spec->tcp_udp.val, &ib_spec->tcp_udp.val,
		       sizeof(struct ibv_exp_flow_tcp_udp_filter));
		memcpy(&kern_spec->tcp_udp.mask, &ib_spec->tcp_udp.mask,
		       sizeof(struct ibv_exp_flow_tcp_udp_filter));
		break;
	case IBV_EXP_FLOW_SPEC_VXLAN_TUNNEL:
		if (!is_exp)
			return EINVAL;
		ret = get_filters_size(ib_spec, kern_spec,
				       &ib_filter_size, &kern_filter_size,
				       IBV_EXP_FLOW_SPEC_VXLAN_TUNNEL);
		if (ret)
			return ret;

		kern_spec->tunnel.size = sizeof(struct ibv_exp_kern_spec_tunnel);
		memcpy(&kern_spec->tunnel.val, &ib_spec->tunnel.val,
		       kern_filter_size);
		memcpy(&kern_spec->tunnel.mask, (void *)&ib_spec->tunnel.val
		       + ib_filter_size, kern_filter_size);
		break;
	case IBV_EXP_FLOW_SPEC_ACTION_TAG:
		if (!is_exp)
			return EINVAL;
		kern_spec->flow_tag.size = sizeof(struct
						  ibv_exp_kern_spec_action_tag);
		kern_spec->flow_tag.tag_id = ib_spec->flow_tag.tag_id;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

static int flow_is_exp(struct ibv_exp_flow_attr *flow_attr)
{
	int i;
	void *ib_spec = flow_attr + 1;

	for (i = 0; i < flow_attr->num_of_specs; i++) {
		if (((struct ibv_exp_flow_spec *)ib_spec)->hdr.type ==
		    IBV_EXP_FLOW_SPEC_IPV6 ||
		    ((struct ibv_exp_flow_spec *)ib_spec)->hdr.type ==
		    IBV_EXP_FLOW_SPEC_IPV4_EXT ||
		    ((struct ibv_exp_flow_spec *)ib_spec)->hdr.type ==
		    IBV_EXP_FLOW_SPEC_IPV6_EXT ||
		    ((struct ibv_exp_flow_spec *)ib_spec)->hdr.type ==
		    IBV_EXP_FLOW_SPEC_VXLAN_TUNNEL ||
		    ((struct ibv_exp_flow_spec *)ib_spec)->hdr.type ==
		    IBV_EXP_FLOW_SPEC_ACTION_TAG ||
		    ((struct ibv_exp_flow_spec *)ib_spec)->hdr.type &
		    IBV_EXP_FLOW_SPEC_INNER)
			return 1;
		ib_spec += ((struct ibv_exp_flow_spec *)ib_spec)->hdr.size;
	}

	return 0;
}

static struct ibv_flow *cmd_create_flow(struct ibv_qp *qp,
					struct ibv_exp_flow_attr *flow_attr,
					void *ib_spec,
					int is_exp)
{
	if (PRINT_LOG)
	{
		printf("### cmd_create_flow ###\n");
		fflush(stdout);
	}

	struct ibv_create_flow *cmd;
	struct ibv_create_flow_resp resp;
	struct ibv_flow *flow_id;
	size_t cmd_size;
	size_t written_size;
	int i, err = 0;
	void *kern_spec;
	int exp_flow = flow_is_exp(flow_attr);
	size_t spec_size;

	spec_size = exp_flow ? sizeof(struct ibv_exp_kern_spec) :
		sizeof(struct ibv_kern_spec);

	cmd_size = sizeof(*cmd) + (flow_attr->num_of_specs * spec_size);

	cmd = alloca(cmd_size);
	flow_id = calloc(1, sizeof(*flow_id));
	if (!flow_id)
		return NULL;
	memset(cmd, 0, cmd_size);

	cmd->qp_handle = qp->handle;

	cmd->flow_attr.type = flow_attr->type;
	cmd->flow_attr.priority = flow_attr->priority;
	cmd->flow_attr.num_of_specs = flow_attr->num_of_specs;
	cmd->flow_attr.port = flow_attr->port;
	cmd->flow_attr.flags = flow_attr->flags;

	kern_spec = cmd + 1;
        //struct ibv_kern_spec *ib_spec_ptr = NULL;
	for (i = 0; i < flow_attr->num_of_specs; i++) {
		err = ib_spec_to_kern_spec(ib_spec, kern_spec, is_exp);
		if (err) {
			errno = err;
			goto err;
		}

		//ib_spec_ptr = kern_spec;
                //printf("@@@ spec.size=%d, spec.type=%x\n", ib_spec_ptr->hdr.size, ib_spec_ptr->hdr.type);
		
		cmd->flow_attr.size +=
			((struct ibv_kern_spec *)kern_spec)->hdr.size;
		kern_spec += ((struct ibv_kern_spec *)kern_spec)->hdr.size;
		ib_spec += ((struct ibv_exp_flow_spec *)ib_spec)->hdr.size;
	}

	written_size = sizeof(*cmd) + cmd->flow_attr.size;
	if (!exp_flow)
		IBV_INIT_CMD_RESP_EX_VCMD(cmd, written_size, written_size,
					  CREATE_FLOW, &resp, sizeof(resp));
	else
		IBV_INIT_CMD_RESP_EXP(CREATE_FLOW, cmd, written_size, 0,
				      &resp, sizeof(resp), 0);

	/*if (write(qp->context->cmd_fd, cmd, written_size) != written_size)
		goto err;*/

        //void *ptr = cmd + 1;
        //printf("exp_flow=%d, written_size=%d, num_of_specs: %d\n", exp_flow, written_size, cmd->flow_attr.num_of_specs);

        //int iii = 0;
        //for (iii = 0; iii < cmd->flow_attr.num_of_specs; iii++)
        //{
        //        printf("spec.size=%d, spec.type=%x\n", ((struct ibv_kern_spec*)ptr)->hdr.size, ((struct ibv_kern_spec*)ptr)->hdr.type);
        //        ptr = ptr + (((struct ibv_kern_spec*)ptr)->hdr.size);
        //}

        struct IBV_CREATE_FLOW_REQ req_body;
        struct IBV_CREATE_FLOW_RSP rsp;
        int rsp_size;

	memset(&req_body.cmd, 0, 1024);
	memcpy(&req_body.cmd, cmd, written_size);
	req_body.written_size = written_size;
	req_body.exp_flow = exp_flow;
        request_router(IBV_CREATE_FLOW, &req_body, &rsp, &rsp_size);
	memcpy(&resp, &rsp.resp, sizeof resp);

	VALGRIND_MAKE_MEM_DEFINED(&resp, sizeof(resp));

	flow_id->context = qp->context;
	flow_id->handle = resp.flow_handle;
	return flow_id;
err:
	free(flow_id);
	return NULL;
}

struct ibv_exp_flow *ibv_exp_cmd_create_flow(struct ibv_qp *qp,
					     struct ibv_exp_flow_attr *flow_attr)
{
	//if (PRINT_LOG)
	//{
		printf("#### ibv_exp_cmd_create_flow ####\n");
		fflush(stdout);
	//}

	void *ib_spec = flow_attr + 1;
	struct ibv_flow *fl;

	fl = cmd_create_flow(qp, flow_attr, ib_spec, 1);

	if (fl)
		return (struct ibv_exp_flow *)&fl->context;

	return NULL;
}

struct ibv_flow *ibv_cmd_create_flow(struct ibv_qp *qp,
				     struct ibv_flow_attr *flow_attr)
{
	//if (PRINT_LOG)
	{
		printf("#### ibv_cmd_create_flow ####\n");
		fflush(stdout);
	}

	void *ib_spec = flow_attr + 1;

	if (flow_attr->comp_mask) {
		errno = EINVAL;
		return NULL;
	}

	return cmd_create_flow(qp, (struct ibv_exp_flow_attr *)&flow_attr->type,
			       ib_spec, 0);
}

static int cmd_destroy_flow(uint32_t handle, int cmd_fd)
{
	//if (PRINT_LOG)
	//{
		printf("#### cmd_destroy_flow ####\n");
		fflush(stdout);
	//}

	struct ibv_destroy_flow cmd;
	int ret = 0;

	memset(&cmd, 0, sizeof(cmd));
	IBV_INIT_CMD_EX(&cmd, sizeof(cmd), DESTROY_FLOW);
	cmd.flow_handle = handle;

	/*if (write(cmd_fd, &cmd, sizeof(cmd)) != sizeof(cmd))
		ret = errno;*/

        struct IBV_DESTROY_FLOW_REQ req_body;
        struct IBV_DESTROY_FLOW_RSP rsp;
        int rsp_size;

	memcpy(&req_body.cmd, &cmd, sizeof cmd);
        request_router(IBV_DESTROY_FLOW, &req_body, &rsp, &rsp_size);

	return rsp.ret_errno;
}

int ibv_exp_cmd_destroy_flow(struct ibv_exp_flow *flow_id)
{
	//if (PRINT_LOG)
	//{
		printf("#### ibv_exp_cmd_destroy_flow ####\n");
		fflush(stdout);
	//}

	int ret = cmd_destroy_flow(flow_id->handle, flow_id->context->cmd_fd);
	struct ibv_flow *fl = (void *)flow_id - offsetof(struct ibv_flow, context);

	free(fl);

	return ret;
}

int ibv_cmd_destroy_flow(struct ibv_flow *flow_id)
{
	//if (PRINT_LOG)
	//{
		printf("#### ibv_cmd_destroy_flow ####\n");
		fflush(stdout);
	//}

	int ret = cmd_destroy_flow(flow_id->handle, flow_id->context->cmd_fd);

	free(flow_id);

	return ret;
}

int ibv_cmd_query_device_ex(struct ibv_context *context,
			    const struct ibv_query_device_ex_input *input,
			    struct ibv_device_attr_ex *attr, size_t attr_size,
			    uint64_t *raw_fw_ver,
			    struct ibv_query_device_ex *cmd,
			    size_t cmd_core_size,
			    size_t cmd_size,
			    struct ibv_query_device_resp_ex *resp,
			    size_t resp_core_size,
			    size_t resp_size)
{
	int err;

	if (input && input->comp_mask)
		return EINVAL;

	if (attr_size < offsetof(struct ibv_device_attr_ex, comp_mask) +
			sizeof(attr->comp_mask))
		return EINVAL;

	if (resp_core_size < offsetof(struct ibv_query_device_resp_ex,
				      response_length) +
			     sizeof(resp->response_length))
		return EINVAL;

	IBV_INIT_CMD_RESP_EX_V(cmd, cmd_core_size, cmd_size,
			       QUERY_DEVICE_EX, resp, resp_core_size,
			       resp_size);
	cmd->comp_mask = 0;
	cmd->reserved = 0;
	memset(attr->orig_attr.fw_ver, 0, sizeof(attr->orig_attr.fw_ver));
	memset(&attr->comp_mask, 0, attr_size - sizeof(attr->orig_attr));
	err = write(context->cmd_fd, cmd, cmd_size);
	if (err != cmd_size)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);
	copy_query_dev_fields(&attr->orig_attr, &resp->base, raw_fw_ver);
	/* Report back supported comp_mask bits. For now no comp_mask bit is
	 * defined */
	attr->comp_mask = resp->comp_mask & 0;
	if (attr_size >= offsetof(struct ibv_device_attr_ex, odp_caps) +
			 sizeof(attr->odp_caps)) {
		if (resp->response_length >=
		    offsetof(struct ibv_query_device_resp_ex, odp_caps) +
		    sizeof(resp->odp_caps)) {
			attr->odp_caps.general_caps = resp->odp_caps.general_caps;
			attr->odp_caps.per_transport_caps.rc_odp_caps =
				resp->odp_caps.per_transport_caps.rc_odp_caps;
			attr->odp_caps.per_transport_caps.uc_odp_caps =
				resp->odp_caps.per_transport_caps.uc_odp_caps;
			attr->odp_caps.per_transport_caps.ud_odp_caps =
				resp->odp_caps.per_transport_caps.ud_odp_caps;
		} else {
			memset(&attr->odp_caps, 0, sizeof(attr->odp_caps));
		}
	}

	return 0;
}
