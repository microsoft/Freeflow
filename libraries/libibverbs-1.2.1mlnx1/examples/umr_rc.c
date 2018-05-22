/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <assert.h>

#include "pingpong.h"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

enum {
	UMR_RECV_WRID = 1,
	UMR_SEND_WRID = 2,
};

static int page_size;
static int use_contiguous_mr;

struct umr_context {
	struct ibv_context			*context;
	struct ibv_comp_channel			*channel;
	struct ibv_pd				*pd;
	struct ibv_mr				**mr_arr;
	int					num_mrs;
	struct ibv_mr				*umr;
	struct ibv_exp_mkey_list_container	*mkey_list_container;
	struct ibv_cq				*cq;
	struct ibv_qp				*qp;
	void					**buf;
	int					size;
	int					rx_depth;
	int					pending;
	struct ibv_port_attr			portinfo;
	int					inlr_recv;
};

struct umr_dest {
	int lid;
	int qpn;
	int psn;
	union ibv_gid gid;
};

static int pp_connect_ctx(struct umr_context *ctx, int port, int my_psn,
			  enum ibv_mtu mtu, int sl,
			  struct umr_dest *dest, int sgid_idx)
{
	struct ibv_qp_attr attr = {
		.qp_state		= IBV_QPS_RTR,
		.path_mtu		= mtu,
		.dest_qp_num		= dest->qpn,
		.rq_psn			= dest->psn,
		.max_dest_rd_atomic	= 1,
		.min_rnr_timer		= 12,
		.ah_attr		= {
			.is_global	= 0,
			.dlid		= dest->lid,
			.sl		= sl,
			.src_path_bits	= 0,
			.port_num	= port
		}
	};

	if (dest->gid.global.interface_id) {
		attr.ah_attr.is_global = 1;
		attr.ah_attr.grh.hop_limit = 1;
		attr.ah_attr.grh.dgid = dest->gid;
		attr.ah_attr.grh.sgid_index = sgid_idx;
	}
	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_AV                 |
			  IBV_QP_PATH_MTU           |
			  IBV_QP_DEST_QPN           |
			  IBV_QP_RQ_PSN             |
			  IBV_QP_MAX_DEST_RD_ATOMIC |
			  IBV_QP_MIN_RNR_TIMER)) {
		fprintf(stderr, "Failed to modify QP to RTR\n");
		return 1;
	}

	attr.qp_state	    = IBV_QPS_RTS;
	attr.timeout	    = 14;
	attr.retry_cnt	    = 7;
	attr.rnr_retry	    = 7;
	attr.sq_psn	    = my_psn;
	attr.max_rd_atomic  = 1;
	if (ibv_modify_qp(ctx->qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_TIMEOUT            |
			  IBV_QP_RETRY_CNT          |
			  IBV_QP_RNR_RETRY          |
			  IBV_QP_SQ_PSN             |
			  IBV_QP_MAX_QP_RD_ATOMIC)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	return 0;
}

static struct umr_dest *pp_client_exch_dest(const char *servername, int port,
						 const struct umr_dest *my_dest)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
	int n;
	int sockfd = -1;
	struct umr_dest *rem_dest = NULL;
	char gid[33];

	if (asprintf(&service, "%d", port) < 0)
		return NULL;

	n = getaddrinfo(servername, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
		free(service);
		return NULL;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
		if (sockfd >= 0) {
			if (!connect(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			close(sockfd);
			sockfd = -1;
		}
	}

	freeaddrinfo(res);
	free(service);

	if (sockfd < 0) {
		fprintf(stderr, "Couldn't connect to %s:%d\n", servername, port);
		return NULL;
	}

	gid_to_wire_gid(&my_dest->gid, gid);
	sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn,
		my_dest->psn, gid);
	if (write(sockfd, msg, sizeof(msg)) != sizeof(msg)) {
		fprintf(stderr, "Couldn't send local address\n");
		goto out;
	}

	if (read(sockfd, msg, sizeof(msg)) != sizeof(msg)) {
		perror("client read");
		fprintf(stderr, "Couldn't read remote address\n");
		goto out;
	}

	if (write(sockfd, "done", sizeof("done")) != sizeof("done")) {
		fprintf(stderr, "Couldn't send \"done\" msg\n");
		goto out;
	}

	rem_dest = malloc(sizeof(*rem_dest));
	if (!rem_dest)
		goto out;

	sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn,
	       &rem_dest->psn, gid);
	wire_gid_to_gid(gid, &rem_dest->gid);

out:
	close(sockfd);
	return rem_dest;
}

static struct umr_dest *pp_server_exch_dest(struct umr_context *ctx,
						 int ib_port, enum ibv_mtu mtu,
						 int port, int sl,
						 const struct umr_dest *my_dest,
						 int sgid_idx)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_flags    = AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000:000000:000000:00000000000000000000000000000000"];
	int n;
	int sockfd = -1, connfd;
	struct umr_dest *rem_dest = NULL;
	char gid[33];

	if (asprintf(&service, "%d", port) < 0)
		return NULL;

	n = getaddrinfo(NULL, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
		free(service);
		return NULL;
	}

	for (t = res; t; t = t->ai_next) {
		sockfd = socket(t->ai_family, t->ai_socktype, t->ai_protocol);
		if (sockfd >= 0) {
			n = 1;

			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

			if (!bind(sockfd, t->ai_addr, t->ai_addrlen))
				break;
			close(sockfd);
			sockfd = -1;
		}
	}

	freeaddrinfo(res);
	free(service);

	if (sockfd < 0) {
		fprintf(stderr, "Couldn't listen to port %d\n", port);
		return NULL;
	}

	listen(sockfd, 1);
	connfd = accept(sockfd, NULL, 0);
	close(sockfd);
	if (connfd < 0) {
		fprintf(stderr, "accept() failed\n");
		return NULL;
	}

	n = read(connfd, msg, sizeof(msg));
	if (n != sizeof(msg)) {
		perror("server read");
		fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int) sizeof(msg));
		goto out;
	}

	rem_dest = malloc(sizeof(*rem_dest));
	if (!rem_dest)
		goto out;

	sscanf(msg, "%x:%x:%x:%s", &rem_dest->lid, &rem_dest->qpn,
	       &rem_dest->psn, gid);
	wire_gid_to_gid(gid, &rem_dest->gid);

	if (pp_connect_ctx(ctx, ib_port, my_dest->psn, mtu, sl, rem_dest,
			   sgid_idx)) {
		fprintf(stderr, "Couldn't connect to remote QP\n");
		free(rem_dest);
		rem_dest = NULL;
		goto out;
	}


	gid_to_wire_gid(&my_dest->gid, gid);
	sprintf(msg, "%04x:%06x:%06x:%s", my_dest->lid, my_dest->qpn,
		my_dest->psn, gid);
	if (write(connfd, msg, sizeof(msg)) != sizeof(msg)) {
		fprintf(stderr, "Couldn't send local address\n");
		free(rem_dest);
		rem_dest = NULL;
		goto out;
	}

	/* expecting msg "done" */
	if (read(connfd, msg, sizeof(msg)) <= 0) {
		fprintf(stderr, "Couldn't read \"done\" msg\n");
		free(rem_dest);
		rem_dest = NULL;
		goto out;
	}

out:
	close(connfd);
	return rem_dest;
}

static int create_umr(struct umr_context *ctx, int num_mrs,
		      int use_repeat_block, int umr_ninl_send,
		      int list_length, int size, int rb_len,
		      int rb_stride, int rb_count)
{
	struct ibv_exp_create_mr_in mrin;
	struct ibv_exp_mem_region *mem_reg_list = NULL;
	struct ibv_exp_mem_repeat_block *mem_rep_list = NULL;
	struct ibv_exp_send_wr wr;
	struct ibv_exp_send_wr *bad_wr;
	int rc;
	int i, err = 0;
	int umr_len = 0;
	struct ibv_exp_wc wc;
	int ne;
	int ndim = 1;
	size_t *rpt_cnt = NULL;

	if (use_repeat_block) {
		mem_rep_list = calloc(num_mrs, sizeof(*mem_rep_list));
		if (!mem_rep_list) {
			fprintf(stderr, "Failed to allocate mkey_list\n");
			return -1;
		}

		for (i = 0; i < num_mrs; i++) {
			mem_rep_list[i].byte_count = calloc(ndim, sizeof(mem_rep_list[i].byte_count[0]));
			if (!mem_rep_list[i].byte_count)
				goto clean_mem_reg_list;

			mem_rep_list[i].stride = calloc(ndim, sizeof(mem_rep_list[i].stride[0]));
			if (!mem_rep_list[i].stride)
				goto clean_mem_reg_list;
		}

		rpt_cnt = (size_t *)calloc(ndim, sizeof(*rpt_cnt));
		if (!rpt_cnt) {
			fprintf(stderr, "Failed to allocate rpt_cnt\n");
			err = -1;
			goto clean_mem_reg_list;
		}

		for (i = 0; i < ndim; i++)
			rpt_cnt[i] = rb_count;

		for (i = 0; i < num_mrs; i++) {
			mem_rep_list[i].base_addr = (uint64_t)(uintptr_t)ctx->mr_arr[i]->addr;
			mem_rep_list[i].byte_count[0] = rb_len;
			mem_rep_list[i].mr = ctx->mr_arr[i];
			mem_rep_list[i].stride[0] = rb_stride;

			umr_len += rb_count * mem_rep_list[i].byte_count[0];
		}
	} else {
		mem_reg_list = calloc(num_mrs, sizeof(*mem_reg_list));
		if (!mem_reg_list) {
			fprintf(stderr, "Failed to allocate mkey_list\n");
			return -1;
		}

		for (i = 0; i < num_mrs; i++) {
			mem_reg_list[i].base_addr = (uint64_t)(uintptr_t)ctx->mr_arr[i]->addr;
			mem_reg_list[i].length = ctx->mr_arr[i]->length;
			mem_reg_list[i].mr = ctx->mr_arr[i];
			umr_len += mem_reg_list[i].length;
		}
	}

	memset(&mrin, 0, sizeof(mrin));
	mrin.pd = ctx->pd;
	mrin.attr.create_flags = IBV_EXP_MR_INDIRECT_KLMS;
	mrin.attr.exp_access_flags = IBV_EXP_ACCESS_LOCAL_WRITE;
	mrin.attr.max_klm_list_size = num_mrs;
	ctx->umr = ibv_exp_create_mr(&mrin);
	if (!ctx->umr) {
		fprintf(stderr, "Failed to create modified_mr\n");
		err = -1;
		goto clean_rpt_cnt;
	}

	if (umr_ninl_send) {
		struct ibv_exp_mkey_list_container_attr in = {
				.pd = ctx->pd,
				.mkey_list_type = IBV_EXP_MKEY_LIST_TYPE_INDIRECT_MR,
				.max_klm_list_size = list_length
		};
		ctx->mkey_list_container = ibv_exp_alloc_mkey_list_memory(&in);
		if (!ctx->mkey_list_container) {
			fprintf(stderr, "Failed to allocate alloc_mkey_list_memory\n");
			err = -1;
			goto clean_umr;
		}
	}

	memset(&wr, 0, sizeof(wr));
	if (use_repeat_block) {
		wr.ext_op.umr.umr_type = IBV_EXP_UMR_REPEAT;
		wr.ext_op.umr.mem_list.rb.mem_repeat_block_list = mem_rep_list;
		wr.ext_op.umr.mem_list.rb.stride_dim = 1;
		wr.ext_op.umr.mem_list.rb.repeat_count = rpt_cnt;
	} else {
		wr.ext_op.umr.umr_type = IBV_EXP_UMR_MR_LIST;
		wr.ext_op.umr.mem_list.mem_reg_list = mem_reg_list;
	}

	if (umr_ninl_send)
		wr.ext_op.umr.memory_objects = ctx->mkey_list_container;
	else
		wr.exp_send_flags = IBV_EXP_SEND_INLINE;

	wr.ext_op.umr.exp_access = IBV_EXP_ACCESS_LOCAL_WRITE;
	wr.ext_op.umr.modified_mr = ctx->umr;
	wr.ext_op.umr.base_addr = (uint64_t)(uintptr_t)ctx->mr_arr[0]->addr;
	wr.ext_op.umr.num_mrs = num_mrs;
	wr.exp_send_flags |= IBV_EXP_SEND_SIGNALED;
	wr.exp_opcode = IBV_EXP_WR_UMR_FILL;

	rc = ibv_exp_post_send(ctx->qp, &wr, &bad_wr);
	if (rc) {
		fprintf(stderr, "Failed in ibv_exp_post_send IBV_EXP_WR_UMR_FILL\n");
		err = -1;
		goto clean_mkey_list;
	}

	ne = 0;
	while (!ne) {
		ne = ibv_exp_poll_cq(ctx->cq, 1, &wc, sizeof(wc));
		if (ne < 0) {
			fprintf(stderr, "poll CQ failed after IBV_EXP_WR_UMR_FILL\n");
			goto invalidate_umr;
		}
	}

	if (wc.status != IBV_WC_SUCCESS) {
		fprintf(stderr, "comp status %d\n", wc.status);
		goto invalidate_umr;
	}

	ctx->umr->length = umr_len;
	ctx->umr->addr = (void *)(unsigned long)wr.ext_op.umr.base_addr;

	goto clean_rpt_cnt;

invalidate_umr:
	wr.exp_opcode = IBV_EXP_WR_UMR_INVALIDATE;
	ibv_exp_post_send(ctx->qp, &wr, &bad_wr);

clean_mkey_list:
	if (umr_ninl_send)
		ibv_exp_dealloc_mkey_list_memory(ctx->mkey_list_container);

clean_umr:
	ibv_dereg_mr(ctx->umr);

clean_rpt_cnt:
	if (use_repeat_block)
		free(rpt_cnt);

clean_mem_reg_list:
	if (use_repeat_block) {
		for (i = 0; i < num_mrs; i++) {
			if (mem_rep_list[i].stride)
				free(mem_rep_list[i].stride);
			if (mem_rep_list[i].byte_count)
				free(mem_rep_list[i].byte_count);
		}
		if (mem_rep_list)
			free(mem_rep_list);
	}
	if (mem_reg_list)
		free(mem_reg_list);

	return err;
}

static struct umr_context *pp_init_ctx(struct ibv_device *ib_dev, int size,
				       int rx_depth, int port,
				       int use_event, int inlr_recv,
				       int num_mrs)
{
	struct umr_context *ctx;
	struct ibv_exp_device_attr dattr;
	int ret, i, num_allocated_bufs = 0;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return NULL;

	memset(&dattr, 0, sizeof(dattr));

	ctx->num_mrs = num_mrs;
	ctx->size     = size * num_mrs;
	ctx->rx_depth = rx_depth;

	ctx->buf = calloc(num_mrs, sizeof(void *));
	if (!ctx->buf)
		goto clean_ctx;

	if (!use_contiguous_mr) {
		for (i = 0; i < num_mrs; i++) {
			ctx->buf[i] = memalign(page_size, size);
			if (!ctx->buf) {
				fprintf(stderr, "Couldn't allocate work buf.\n");
				goto clean_buffer;
			}
			num_allocated_bufs++;
		}
	}

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		goto clean_buffer;
	}

	if (inlr_recv)
		dattr.comp_mask |= IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ;

	ret = ibv_exp_query_device(ctx->context, &dattr);
	if (inlr_recv) {
		if (ret) {
			printf("  Couldn't query device for inline-receive capabilities.\n");
		} else if (!(dattr.comp_mask & IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ)) {
			printf("  Inline-receive not supported by driver.\n");
		} else if (dattr.inline_recv_sz < inlr_recv) {
			printf("  Max inline-receive(%d) < Requested inline-receive(%d).\n",
			       dattr.inline_recv_sz, inlr_recv);
		}
	}
	ctx->inlr_recv = inlr_recv;

	memset(&dattr, 0, sizeof(dattr));
	dattr.comp_mask |= IBV_EXP_DEVICE_ATTR_UMR;
	ret = ibv_exp_query_device(ctx->context, &dattr);
	if (ret)
		printf("  Couldn't query device for UMR capabilities.\n");

	if (use_event) {
		ctx->channel = ibv_create_comp_channel(ctx->context);
		if (!ctx->channel) {
			fprintf(stderr, "Couldn't create completion channel\n");
			goto clean_device;
		}
	} else {
		ctx->channel = NULL;
	}

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_comp_channel;
	}

	ctx->mr_arr = calloc(num_mrs, sizeof(struct ibv_mr *));
	for (i = 0; i < num_mrs; i++) {
		if (!use_contiguous_mr) {
			ctx->mr_arr[i] = ibv_reg_mr(ctx->pd, ctx->buf[i], size,
						    IBV_ACCESS_LOCAL_WRITE);
		} else {
			struct ibv_exp_reg_mr_in in;

			memset(&in, 0, sizeof(in));
			in.pd = ctx->pd;
			in.addr = NULL;
			in.length = size;
			in.exp_access = IBV_EXP_ACCESS_LOCAL_WRITE |
					IBV_EXP_ACCESS_ALLOCATE_MR;
			in.comp_mask = 0;

			ctx->mr_arr[i] = ibv_exp_reg_mr(&in);
		}

		if (!ctx->mr_arr[i]) {
			fprintf(stderr, "Couldn't register MR num %d\n", i);
			goto clean_pd;
		} else {
			if (use_contiguous_mr)
				ctx->buf[i] = ctx->mr_arr[i]->addr;
		}

		memset(ctx->buf[i], i, size);
	}

	ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL,
				ctx->channel, 0);
	if (!ctx->cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		goto clean_mr;
	}

	{
		struct ibv_exp_qp_init_attr attr = {
			.send_cq = ctx->cq,
			.recv_cq = ctx->cq,
			.cap     = {
				.max_send_wr  = 200,
				.max_recv_wr  = rx_depth,
				.max_send_sge = 2,
				.max_recv_sge = 2,
			},
			.qp_type = IBV_QPT_RC,
			.pd = ctx->pd,
			.comp_mask = IBV_EXP_QP_INIT_ATTR_PD,
			.max_inl_recv = ctx->inlr_recv,
			.max_inl_send_klms = dattr.umr_caps.max_send_wqe_inline_klms
		};
		if (ctx->inlr_recv)
			attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_INL_RECV;

		attr.exp_create_flags |= IBV_EXP_QP_CREATE_UMR;
		attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS |
				  IBV_EXP_QP_INIT_ATTR_MAX_INL_KLMS;
		ctx->qp = ibv_exp_create_qp(ctx->context, &attr);

		if (!ctx->qp)  {
			fprintf(stderr, "Couldn't create QP, errno = %d\n", errno);
			goto clean_cq;
		}
		if (ctx->inlr_recv > attr.max_inl_recv)
			printf("  Actual inline-receive(%d) < requested inline-receive(%d)\n",
			       attr.max_inl_recv, ctx->inlr_recv);
	}

	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = port,
			.qp_access_flags = 0
		};

		if (ibv_modify_qp(ctx->qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_ACCESS_FLAGS)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			goto clean_qp;
		}
	}

	return ctx;

clean_qp:
	ibv_destroy_qp(ctx->qp);

clean_cq:
	ibv_destroy_cq(ctx->cq);

clean_mr:
	for (i = 0; i < num_mrs; i++)
		ibv_dereg_mr(ctx->mr_arr[i]);

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_comp_channel:
	if (ctx->channel)
		ibv_destroy_comp_channel(ctx->channel);

clean_device:
	ibv_close_device(ctx->context);

clean_buffer:
	if (!use_contiguous_mr)
		for (i = 0; i < num_allocated_bufs; i++)
			free(ctx->buf[i]);

	free(ctx->buf);

clean_ctx:
	free(ctx);

	return NULL;
}

int invalidate_umr(struct umr_context *ctx)
{
	struct ibv_exp_send_wr wr, *bad_wr;;
	struct ibv_exp_wc wc;
	int rc;
	int ne;

	if (!ctx->umr)
		return 0;

	if (ctx->umr->addr) {
		memset(&wr, 0, sizeof(wr));
		wr.exp_opcode = IBV_EXP_WR_UMR_INVALIDATE;
		wr.ext_op.umr.modified_mr = ctx->umr;
		wr.exp_send_flags = IBV_EXP_SEND_SIGNALED;
		rc = ibv_exp_post_send(ctx->qp, &wr, &bad_wr);
		if (rc)
			printf("failed to invalidate_umr (%d)\n", rc);
	}

	do {
		ne = ibv_exp_poll_cq(ctx->cq, 1, &wc, sizeof(wc));
		if (ne < 0) {
			fprintf(stderr, "poll CQ failed after IBV_EXP_WR_UMR_FILL\n");
			return 1;
		}
	} while (!ne);

	if (wc.status != IBV_WC_SUCCESS) {
		printf("comp status %d\n", wc.status);
		return 1;
	}

	return 0;
}

int pp_close_ctx(struct umr_context *ctx)
{
	int i;

	if (ctx->mkey_list_container) {
		if (ibv_exp_dealloc_mkey_list_memory(ctx->mkey_list_container)) {
			fprintf(stderr, "Couldn't dealloc mkey list memory\n");
			return 1;
		}
	}

	if (ibv_destroy_qp(ctx->qp)) {
		fprintf(stderr, "Couldn't destroy QP\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->cq)) {
		fprintf(stderr, "Couldn't destroy CQ\n");
		return 1;
	}

	for (i = 0; i < ctx->num_mrs; i++)
		if (ibv_dereg_mr(ctx->mr_arr[i])) {
			fprintf(stderr, "Couldn't deregister MR\n");
			return 1;
		}
	free(ctx->mr_arr);

	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ctx->channel) {
		if (ibv_destroy_comp_channel(ctx->channel)) {
			fprintf(stderr, "Couldn't destroy completion channel\n");
			return 1;
		}
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	if (!use_contiguous_mr)
		for (i = 0; i < ctx->num_mrs; i++)
			free(ctx->buf[i]);

	free(ctx->buf);
	free(ctx);

	return 0;
}

static int pp_post_recv(struct umr_context *ctx, int n)
{
	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->umr->addr,
		.length = ctx->umr->length,
		.lkey	= ctx->umr->lkey
	};
	struct ibv_recv_wr wr = {
		.wr_id	    = UMR_RECV_WRID,
		.sg_list    = &list,
		.num_sge    = 1,
	};
	struct ibv_recv_wr *bad_wr;
	int i;

	for (i = 0; i < n; ++i)
		if (ibv_post_recv(ctx->qp, &wr, &bad_wr))
			break;

	return i;
}

static int pp_post_send(struct umr_context *ctx)
{
	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->umr->addr,
		.length = ctx->umr->length,
		.lkey	= ctx->umr->lkey
	};
	struct ibv_send_wr wr = {
		.wr_id	    = UMR_SEND_WRID,
		.sg_list    = &list,
		.num_sge    = 1,
		.opcode     = IBV_WR_SEND,
		.send_flags = IBV_SEND_SIGNALED,
	};
	struct ibv_send_wr *bad_wr;

	return ibv_post_send(ctx->qp, &wr, &bad_wr);
}

static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n", argv0);
	printf("  %s <host>     connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>         listen on/connect to port <port> (default 18515)\n");
	printf("  -d, --ib-dev=<dev>        use IB device <dev> (default first device found)\n");
	printf("  -i, --ib-port=<port>      use port <port> of IB device (default 1)\n");
	printf("  -s, --size=<size>         size of message to exchange (default 4096)\n");
	printf("  -m, --mtu=<size>          path MTU (default 1024)\n");
	printf("  -r, --rx-depth=<dep>      number of receives to post at a time (default 500)\n");
	printf("  -n, --iters=<iters>       number of exchanges (default 1000)\n");
	printf("  -l, --sl=<sl>             service level value\n");
	printf("  -e, --events              sleep on CQ events (default poll)\n");
	printf("  -g, --gid-idx=<gid index> local port gid index\n");
	printf("  -c, --contiguous-mr       use contiguous mr\n");
	printf("  -t, --inline-recv=<size>  size of inline-recv\n");
	printf("  -x, --num-mrs		    create umr with num-mrs (default 3)\n");
	printf("  -u, --umr-non-inline-send use umr-non-inline send (default inline send)\n");
	printf("  -b, --repeated-block      which memory block use for umr creation (default memory block)\n");
}

int main(int argc, char *argv[])
{
	struct ibv_device	**dev_list;
	struct ibv_device	*ib_dev;
	struct umr_context	*ctx;
	struct umr_dest		my_dest;
	struct umr_dest		*rem_dest = NULL;
	struct timeval		start, end;
	char			*ib_devname = NULL;
	char			*servername = NULL;
	int			port = 18515;
	int			ib_port = 1;
	int			size = 4096;
	enum ibv_mtu		mtu = IBV_MTU_1024;
	int			rx_depth = 500;
	int			iters = 1000;
	int			use_event = 0;
	int			routs;
	int			rcnt, scnt;
	int			num_cq_events = 0;
	int			sl = 0;
	int			gidx = -1;
	char			gid[INET6_ADDRSTRLEN];
	int			inlr_recv = 0;
	int			umr_ninl_send = 0;
	int			num_mrs = 3;
	int			use_repeat_block = 0;
	int			rb_len = 50;
	int			rb_stride = 100;
	int			rb_count = 30;
	struct			ibv_exp_device_attr dattr;
	int			ret = 0;

	srand48(getpid() * time(NULL));

	while (1) {
		int c;

		static struct option long_options[] = {
			{ .name = "port",			.has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",			.has_arg = 1, .val = 'd' },
			{ .name = "ib-port",			.has_arg = 1, .val = 'i' },
			{ .name = "size",			.has_arg = 1, .val = 's' },
			{ .name = "mtu",			.has_arg = 1, .val = 'm' },
			{ .name = "rx-depth",			.has_arg = 1, .val = 'r' },
			{ .name = "iters",			.has_arg = 1, .val = 'n' },
			{ .name = "sl",				.has_arg = 1, .val = 'l' },
			{ .name = "events",			.has_arg = 0, .val = 'e' },
			{ .name = "gid-idx",			.has_arg = 1, .val = 'g' },
			{ .name = "contiguous-mr",		.has_arg = 0, .val = 'c' },
			{ .name = "inline-recv",		.has_arg = 1, .val = 't' },
			{ .name = "num-mrs",			.has_arg = 1, .val = 'x' },
			{ .name = "umr-non-inline-send",	.has_arg = 0, .val = 'u' },
			{ .name = "repeated-block",		.has_arg = 0, .val = 'b' },
			{ .name = "repeated-block-len",		.has_arg = 1, .val = 'h' },
			{ .name = "repeated-block-stide",	.has_arg = 1, .val = 'k' },
			{ .name = "repeated-block-count",	.has_arg = 1, .val = 'o' },
			{ 0 }
		};

		c = getopt_long(argc, argv, "p:d:i:s:m:r:n:l:ecg:t:x:ubh:k:o:",
							long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			port = strtol(optarg, NULL, 0);
			if (port < 0 || port > 65535) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'd':
			ib_devname = strdupa(optarg);
			break;

		case 'i':
			ib_port = strtol(optarg, NULL, 0);
			if (ib_port < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 's':
			size = strtol(optarg, NULL, 0);
			break;

		case 'm':
			mtu = pp_mtu_to_enum(strtol(optarg, NULL, 0));
			if (mtu < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'r':
			rx_depth = strtol(optarg, NULL, 0);
			break;

		case 'n':
			iters = strtol(optarg, NULL, 0);
			break;

		case 'l':
			sl = strtol(optarg, NULL, 0);
			break;

		case 'e':
			++use_event;
			break;

		case 'g':
			gidx = strtol(optarg, NULL, 0);
			break;

		case 'c':
			++use_contiguous_mr;
			break;

		case 't':
			inlr_recv = strtol(optarg, NULL, 0);
			break;

		case 'x':
			num_mrs = strtol(optarg, NULL, 0);
			break;

		case 'u':
			++umr_ninl_send;
			break;

		case 'b':
			++use_repeat_block;
			break;

		case 'h':
			rb_len = strtol(optarg, NULL, 0);
			break;

		case 'k':
			rb_stride = strtol(optarg, NULL, 0);
			break;

		case 'o':
			rb_count = strtol(optarg, NULL, 0);
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind == argc - 1) {
			servername = strdupa(argv[optind]);
	} else if (optind < argc) {
		usage(argv[0]);
		return 1;
	}

	page_size = sysconf(_SC_PAGESIZE);

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get IB devices list");
		return 1;
	}

	if (!ib_devname) {
		ib_dev = *dev_list;
		if (!ib_dev) {
			fprintf(stderr, "No IB devices found\n");
			goto err;
		}
	} else {
		int i;
		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname))
				break;
		ib_dev = dev_list[i];
		if (!ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ib_devname);
			goto err;
		}
	}

	ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event, inlr_recv, num_mrs);
	if (!ctx)
		goto err;

	if (use_event)
		if (ibv_req_notify_cq(ctx->cq, 0)) {
			fprintf(stderr, "Couldn't request CQ notification\n");
			goto close_ctx;
		}

	if (pp_get_port_info(ctx->context, ib_port, &ctx->portinfo)) {
		fprintf(stderr, "Couldn't get port info\n");
		goto close_ctx;
	}

	my_dest.lid = ctx->portinfo.lid;
	if (ctx->portinfo.link_layer != IBV_LINK_LAYER_ETHERNET &&
	    !my_dest.lid) {
		fprintf(stderr, "Couldn't get local LID\n");
		goto close_ctx;
	}

	if (gidx >= 0) {
		if (ibv_query_gid(ctx->context, ib_port, gidx, &my_dest.gid)) {
			fprintf(stderr, "can't read sgid of index %d\n", gidx);
			goto close_ctx;
		}
	} else {
		memset(&my_dest.gid, 0, sizeof(my_dest.gid));
	}

	my_dest.qpn = ctx->qp->qp_num;
	my_dest.psn = lrand48() & 0xffffff;
	inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof(gid));
	printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	       my_dest.lid, my_dest.qpn, my_dest.psn, gid);


	if (servername)
		rem_dest = pp_client_exch_dest(servername, port, &my_dest);
	else
		rem_dest = pp_server_exch_dest(ctx, ib_port, mtu, port, sl,
					       &my_dest, gidx);

	if (!rem_dest)
		goto close_ctx;

	inet_ntop(AF_INET6, &rem_dest->gid, gid, sizeof(gid));
	printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %s\n",
	       rem_dest->lid, rem_dest->qpn, rem_dest->psn, gid);

	if (servername)
		if (pp_connect_ctx(ctx, ib_port, my_dest.psn, mtu, sl, rem_dest,
				   gidx))
			goto close_ctx;

	memset(&dattr, 0, sizeof(dattr));
	dattr.comp_mask |= IBV_EXP_DEVICE_ATTR_UMR;
	ret = ibv_exp_query_device(ctx->context, &dattr);
	if (ret) {
		printf("  Couldn't query device for UMR capabilities.\n");
		goto close_ctx;
	}

	if (create_umr(ctx, num_mrs, use_repeat_block, umr_ninl_send, dattr.umr_caps.max_klm_list_size,
		       size, rb_len, rb_stride, rb_count)) {
		fprintf(stderr, "Failed to create umr\n");
		goto close_ctx;
	}

	routs = pp_post_recv(ctx, ctx->rx_depth);
	if (routs < ctx->rx_depth) {
		fprintf(stderr, "Couldn't post receive (%d)\n", routs);
		goto close_umr;
	}

	ctx->pending = UMR_RECV_WRID;

	if (servername) {
		if (pp_post_send(ctx)) {
			fprintf(stderr, "Couldn't post send\n");
			goto close_umr;
		}
		ctx->pending |= UMR_SEND_WRID;
	}

	if (gettimeofday(&start, NULL)) {
		perror("gettimeofday");
		goto close_umr;
	}

	rcnt = scnt = 0;
	while (rcnt < iters || scnt < iters) {
		if (use_event) {
			struct ibv_cq *ev_cq;
			void          *ev_ctx;

			if (ibv_get_cq_event(ctx->channel, &ev_cq, &ev_ctx)) {
				fprintf(stderr, "Failed to get cq_event\n");
				goto close_umr;
			}

			++num_cq_events;

			if (ev_cq != ctx->cq) {
				fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
				goto close_umr;
			}

			if (ibv_req_notify_cq(ctx->cq, 0)) {
				fprintf(stderr, "Couldn't request CQ notification\n");
				goto close_umr;
			}
		}

		{
			struct ibv_exp_wc wc[2];
			int ne, i;

			do {
				ne = ibv_exp_poll_cq(ctx->cq, 2, wc, sizeof(wc[0]));
				if (ne < 0) {
					fprintf(stderr, "poll CQ failed %d\n", ne);
					goto close_umr;
				}
			} while (!use_event && ne < 1);

			for (i = 0; i < ne; ++i) {
				if (wc[i].status != IBV_WC_SUCCESS) {
					fprintf(stderr, "Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(wc[i].status),
						wc[i].status, (int) wc[i].wr_id);
					goto close_umr;
				}

				switch ((int) wc[i].wr_id) {
				case UMR_SEND_WRID:
					++scnt;
					break;

				case UMR_RECV_WRID:
					if (--routs <= 1) {
						routs += pp_post_recv(ctx, ctx->rx_depth - routs);
						if (routs < ctx->rx_depth) {
							fprintf(stderr,
								"Couldn't post receive (%d)\n",
								routs);
							goto close_umr;
						}
					}

					++rcnt;
					break;

				default:
					fprintf(stderr, "Completion for unknown wr_id %d\n",
						(int) wc[i].wr_id);
					goto close_umr;
				}

				ctx->pending &= ~(int) wc[i].wr_id;
				if (scnt < iters && !ctx->pending) {
					if (pp_post_send(ctx)) {
						fprintf(stderr, "Couldn't post send\n");
						goto close_umr;
					}
					ctx->pending = UMR_RECV_WRID |
						       UMR_SEND_WRID;
				}
			}
		}
	}

	if (gettimeofday(&end, NULL)) {
		perror("gettimeofday");
		goto close_umr;
	}

	{
		float usec = (end.tv_sec - start.tv_sec) * 1000000 +
			(end.tv_usec - start.tv_usec);
		long long bytes = (long long) size * num_mrs * iters * 2;

		printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n",
		       bytes, usec / 1000000., bytes * 8. / usec);
		printf("%d iters in %.2f seconds = %.2f usec/iter\n",
		       iters, usec / 1000000., usec / iters);
	}

	ibv_ack_cq_events(ctx->cq, num_cq_events);

close_umr:
	invalidate_umr(ctx);
	if (ibv_dereg_mr(ctx->umr)) {
		fprintf(stderr, "Couldn't deregister UMR\n");
		goto err;
	}

close_ctx:
	if (!(pp_close_ctx(ctx)))
		goto out;
err:
	ret = 1;
out:
	ibv_free_device_list(dev_list);
	if (rem_dest)
		free(rem_dest);
	return ret;
}
