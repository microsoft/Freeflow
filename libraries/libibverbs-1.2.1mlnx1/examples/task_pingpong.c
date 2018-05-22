/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2009-2010 Mellanox Technologies.  All rights reserved.
 */

#if HAVE_CONFIG_H
	#include <config.h>
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

#include "cc_pingpong.h"


enum {
	PP_RECV_WRID = 1,
	PP_SEND_WRID = 2,
	PP_CQE_WAIT  = 3,
};

char *wr_id_str[] = {
	[PP_RECV_WRID] = "RECV",
	[PP_SEND_WRID] = "SEND",
	[PP_CQE_WAIT]  = "CQE_WAIT",
};

static long page_size;

struct pingpong_calc_ctx {
	enum ibv_exp_calc_op         opcode;
	enum ibv_exp_calc_data_type  data_type;
	enum ibv_exp_calc_data_size  data_size;
	void                        *gather_buff;
	int                          gather_list_size;
	struct ibv_sge              *gather_list;
};

struct pingpong_context {
	struct ibv_context	*context;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_cq		*scq;
	struct ibv_cq		*rcq;
	struct ibv_qp		*qp;

	struct ibv_qp		*mqp;
	struct ibv_cq		*mcq;

	void			*buf;
	int			size;
	int			rx_depth;
	int			pending;

	int			scnt;
	int			rcnt;

	struct			pingpong_calc_ctx calc_op;
};

struct pingpong_dest {
	int lid;
	int qpn;
	int psn;
};


static int pp_connect_ctx(struct pingpong_context *ctx,
			    struct ibv_qp *qp,
			    int port,
			    int my_psn,
			    enum ibv_mtu mtu,
			    int sl,
			    struct pingpong_dest *dest)
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

	if (ibv_modify_qp(qp, &attr,
			  IBV_QP_STATE			|
			  IBV_QP_AV			|
			  IBV_QP_PATH_MTU		|
			  IBV_QP_DEST_QPN		|
			  IBV_QP_RQ_PSN			|
			  IBV_QP_MAX_DEST_RD_ATOMIC	|
			  IBV_QP_MIN_RNR_TIMER)) {
		fprintf(stderr, "Failed to modify QP to RTR\n");
		return 1;
	}

	attr.qp_state		= IBV_QPS_RTS;
	attr.timeout		= 14;
	attr.retry_cnt		= 7;
	attr.rnr_retry		= 7;
	attr.sq_psn		= my_psn;
	attr.max_rd_atomic	= 1;
	if (ibv_modify_qp(qp, &attr,
			  IBV_QP_STATE			|
			  IBV_QP_TIMEOUT		|
			  IBV_QP_RETRY_CNT		|
			  IBV_QP_RNR_RETRY		|
			  IBV_QP_SQ_PSN			|
			  IBV_QP_MAX_QP_RD_ATOMIC)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	return 0;
}

static struct pingpong_dest *pp_client_exch_dest(const char *servername,
						   int port,
						   const struct pingpong_dest *my_dest)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000:000000:000000"];
	int n;
	int sockfd = -1;
	struct pingpong_dest *rem_dest = NULL;

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

	sprintf(msg, "%04x:%06x:%06x", my_dest->lid, my_dest->qpn, my_dest->psn);
	if (write(sockfd, msg, sizeof msg) != sizeof msg) {
		fprintf(stderr, "Couldn't send local address\n");
		goto out;
	}

	if (read(sockfd, msg, sizeof msg) != sizeof msg) {
		perror("client read");
		fprintf(stderr, "Couldn't read remote address\n");
		goto out;
	}

	if (write(sockfd, "done", sizeof "done") != sizeof("done")) {
		fprintf(stderr, "Couldn't send \"done\" msg\n");
		goto out;
	}

	rem_dest = malloc(sizeof *rem_dest);
	if (!rem_dest)
		goto out;

	sscanf(msg, "%x:%x:%x", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn);

out:
	close(sockfd);
	return rem_dest;
}

static struct pingpong_dest *pp_server_exch_dest(struct pingpong_context *ctx,
						 int ib_port,
						 enum ibv_mtu mtu,
						 int port,
						 int sl,
						 const struct pingpong_dest *my_dest)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_flags    = AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000:000000:000000"];
	int n;
	int sockfd = -1, connfd;
	struct pingpong_dest *rem_dest = NULL;

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

			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof n);

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

	n = read(connfd, msg, sizeof msg);
	if (n != sizeof msg) {
		perror("server read");
		fprintf(stderr, "%d/%d: Couldn't read remote address\n",
			n, (int) sizeof msg);
		goto out;
	}

	rem_dest = malloc(sizeof *rem_dest);
	if (!rem_dest)
		goto out;

	sscanf(msg, "%x:%x:%x", &rem_dest->lid, &rem_dest->qpn, &rem_dest->psn);

	if (pp_connect_ctx(ctx, ctx->qp, ib_port, my_dest->psn, mtu,
			       sl, rem_dest)) {
		fprintf(stderr, "Couldn't connect to remote QP\n");
		free(rem_dest);
		rem_dest = NULL;
		goto out;
	}

	sprintf(msg, "%04x:%06x:%06x", my_dest->lid, my_dest->qpn,
		my_dest->psn);
	if (write(connfd, msg, sizeof msg) != sizeof msg) {
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



int __parse_calc_to_gather(char *ops_str,
			   enum pp_wr_calc_op calc_op,
			   enum pp_wr_data_type data_type,
			   int op_per_gather,
			   int max_num_operands, uint32_t lkey,
			   struct pingpong_calc_ctx *calc_ctx, void *buff)
{

	int i, gather_ix, num_operands;
	int sz;
	char *__gather_token, *__err_ptr = NULL;
	struct ibv_sge *gather_list = NULL;

	if (!ops_str) {
		fprintf(stderr, "You must choose an operation to perform.\n");
		return -1;
	}

	sz = pp_data_type_to_size(data_type);

	for (i = 0, num_operands = 1; i < strlen(ops_str); i++) {
		if (ops_str[i] == ',')
			num_operands++;
	}

	calc_ctx->gather_list_size = num_operands;

	__gather_token = strtok(ops_str, ",");
	if (!__gather_token)
		return -1;

	gather_list = calloc(num_operands/op_per_gather + (num_operands%op_per_gather ? 1 : 0),
			     sizeof *gather_list);
	if (!gather_list)
		return -1;

	for (i = 0, gather_ix = 0; i < num_operands; i++) {
		if (!(i % op_per_gather)) {
			gather_list[gather_ix].addr   = (uint64_t)(uintptr_t)buff
							+ (sz+8)*i;
			gather_list[gather_ix].length = (sz+8)*op_per_gather;
			gather_list[gather_ix].lkey   = lkey;

			gather_ix++;
		}

		switch (data_type) {
		case PP_DATA_TYPE_INT8:
			goto __gather_out;

		case PP_DATA_TYPE_INT16:
			goto __gather_out;

		case PP_DATA_TYPE_INT32:
			goto __gather_out;
			break;

		case PP_DATA_TYPE_INT64:
			*((int64_t *)buff + i*2) = strtoll(__gather_token,
							   &__err_ptr, 0);
			break;

		case PP_DATA_TYPE_FLOAT32:
			goto __gather_out;

		case PP_DATA_TYPE_FLOAT64:
			goto __gather_out;
			break;

		default:
			goto __gather_out;
		}

		__gather_token = strtok(NULL, ",");
		if (!__gather_token)
			break;

	}

	calc_ctx->gather_buff = buff;
	calc_ctx->gather_list = gather_list;

	return num_operands;

__gather_out:
	if (gather_list)
		free(gather_list);

	return -1;
}


static struct pingpong_context *pp_init_ctx(struct ibv_device *ib_dev,
						int size, int rx_depth,
						int port,
				     enum pp_wr_calc_op   calc_op,
				     enum pp_wr_data_type calc_data_type,
						char *calc_operands_str)
{
	struct pingpong_context *ctx;
	int rc;

	ctx = malloc(sizeof *ctx);
	if (!ctx)
		return NULL;
	memset(ctx, 0, sizeof *ctx);

	ctx->size	= size;
	ctx->rx_depth	= rx_depth;

	ctx->calc_op.opcode	= IBV_EXP_CALC_OP_NUMBER;
	ctx->calc_op.data_type	= IBV_EXP_CALC_DATA_TYPE_NUMBER;
	ctx->calc_op.data_size	= IBV_EXP_CALC_DATA_SIZE_NUMBER;

	ctx->buf = memalign(page_size, size);
	if (!ctx->buf) {
		fprintf(stderr, "Couldn't allocate work buf.\n");
		goto clean_ctx;
	}

	memset(ctx->buf, 0, size);

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
			goto clean_buffer;
	}

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_device;
	}

	ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size, IBV_ACCESS_LOCAL_WRITE);
	if (!ctx->mr) {
		fprintf(stderr, "Couldn't register MR\n");
		goto clean_pd;
	}

	if (calc_op != PP_CALC_INVALID) {
		int op_per_gather, max_num_op;

		ctx->calc_op.opcode	= IBV_EXP_CALC_OP_ADD;
		ctx->calc_op.data_type	= IBV_EXP_CALC_DATA_TYPE_INT;
		ctx->calc_op.data_size	= IBV_EXP_CALC_DATA_SIZE_64_BIT;

		rc = pp_query_calc_cap(ctx->context,
					  ctx->calc_op.opcode,
					  ctx->calc_op.data_type,
					  ctx->calc_op.data_size,
					  &op_per_gather, &max_num_op);
		if (rc) {
			fprintf(stderr, "-E- operation not supported on %s. valid ops are:\n",
				ibv_get_device_name(ib_dev));

			pp_print_dev_calc_ops(ctx->context);
			goto clean_mr;
		}

		if (__parse_calc_to_gather(calc_operands_str, calc_op, calc_data_type,
					   op_per_gather, max_num_op, ctx->mr->lkey,
					   &ctx->calc_op, ctx->buf) < 0)
			goto clean_mr;
	}

	{
		struct ibv_exp_cq_attr attr = {
			.comp_mask       = IBV_EXP_CQ_ATTR_CQ_CAP_FLAGS,
			.cq_cap_flags    = IBV_EXP_CQ_IGNORE_OVERRUN
		};

		ctx->rcq = ibv_create_cq(ctx->context, rx_depth, NULL, NULL, 0);
		if (!ctx->rcq) {
			fprintf(stderr, "Couldn't create CQ\n");
			goto clean_mr;
		}

		if (ibv_exp_modify_cq(ctx->rcq, &attr, IBV_EXP_CQ_CAP_FLAGS)) {
			fprintf(stderr, "Failed to modify CQ\n");
			goto clean_rcq;
		}
	}

	{
		struct ibv_exp_cq_attr attr = {
			.comp_mask       = IBV_EXP_CQ_ATTR_CQ_CAP_FLAGS,
			.cq_cap_flags    = IBV_EXP_CQ_IGNORE_OVERRUN
		};

		ctx->scq = ibv_create_cq(ctx->context, 0x10, NULL, NULL, 0);
		if (!ctx->scq) {
			fprintf(stderr, "Couldn't create CQ\n");
			goto clean_rcq;
		}

		if (ibv_exp_modify_cq(ctx->scq, &attr, IBV_EXP_CQ_CAP_FLAGS)) {
			fprintf(stderr, "Failed to modify CQ\n");
			goto clean_scq;
		}
	}

	{
		struct ibv_exp_qp_init_attr attr = {
			.send_cq = ctx->scq,
			.recv_cq = ctx->rcq,
			.cap     = {
				.max_send_wr  = 16,
				.max_recv_wr  = rx_depth,
				.max_send_sge = 16,
				.max_recv_sge = 16
			},
			.qp_type = IBV_QPT_RC,
			.pd = ctx->pd
		};

		{
			attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS | IBV_EXP_QP_INIT_ATTR_PD;
			attr.exp_create_flags = IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_SEND;
			ctx->qp = ibv_exp_create_qp(ctx->context, &attr);
		}
		if (!ctx->qp)  {
			fprintf(stderr, "Couldn't create QP\n");
			goto clean_scq;
		}
	}

	{
		struct ibv_qp_attr attr = {
			.qp_state		= IBV_QPS_INIT,
			.pkey_index		= 0,
			.port_num		= port,
			.qp_access_flags	= 0
		};

		if (ibv_modify_qp(ctx->qp, &attr,
				  IBV_QP_STATE		|
				  IBV_QP_PKEY_INDEX	|
				  IBV_QP_PORT		|
				  IBV_QP_ACCESS_FLAGS)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			goto clean_qp;
		}
	}


	/* Create MQ */
	ctx->mcq = ibv_create_cq(ctx->context, 0x40, NULL, NULL, 0);
	if (!ctx->mcq) {
		fprintf(stderr, "Couldn't create CQ\n");
		goto clean_qp;
	}

	{
		struct ibv_exp_qp_init_attr attr = {
			.send_cq = ctx->mcq,
			.recv_cq = ctx->mcq,
			.cap     = {
				.max_send_wr  = 0x40,
				.max_recv_wr  = 0,
				.max_send_sge = 1,
				.max_recv_sge = 1
			},
			.qp_type = IBV_QPT_RC,
			.pd = ctx->pd
		};

		{
			attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS | IBV_EXP_QP_INIT_ATTR_PD;
			attr.exp_create_flags = IBV_EXP_QP_CREATE_CROSS_CHANNEL;
			ctx->mqp = ibv_exp_create_qp(ctx->context, &attr);
		}
		if (!ctx->mqp)  {
			fprintf(stderr, "Couldn't create QP\n");
			goto clean_mcq;
		}
	}

	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = port,
			.qp_access_flags = 0
		};

		if (ibv_modify_qp(ctx->mqp, &attr,
				    IBV_QP_STATE              |
				    IBV_QP_PKEY_INDEX         |
				    IBV_QP_PORT               |
				    IBV_QP_ACCESS_FLAGS)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			goto clean_mqp;
		}
	}

	{
		struct ibv_qp_attr qp_attr = {
			.qp_state		= IBV_QPS_RTR,
			.path_mtu		= 1,
			.dest_qp_num		= ctx->mqp->qp_num,
			.rq_psn			= 0,
			.max_dest_rd_atomic	= 1,
			.min_rnr_timer		= 12,
			.ah_attr		= {
				.is_global	= 0,
				.dlid		= 0,
				.sl		= 0,
				.src_path_bits	= 0,
				.port_num	= port
			}
		};
		if (ibv_modify_qp(ctx->mqp, &qp_attr,
				    IBV_QP_STATE              |
				    IBV_QP_AV                 |
				    IBV_QP_PATH_MTU           |
				    IBV_QP_DEST_QPN           |
				    IBV_QP_RQ_PSN             |
				    IBV_QP_MAX_DEST_RD_ATOMIC |
				    IBV_QP_MIN_RNR_TIMER)) {
			fprintf(stderr, "Failed to modify QP to RTR\n");
			goto clean_mqp;
		}

		qp_attr.qp_state	= IBV_QPS_RTS;
		qp_attr.timeout		= 14;
		qp_attr.retry_cnt	= 7;
		qp_attr.rnr_retry	= 7;
		qp_attr.sq_psn	        = 0;
		qp_attr.max_rd_atomic   = 1;
		if (ibv_modify_qp(ctx->mqp, &qp_attr,
				  IBV_QP_STATE              |
				  IBV_QP_TIMEOUT            |
				  IBV_QP_RETRY_CNT          |
				  IBV_QP_RNR_RETRY          |
				  IBV_QP_SQ_PSN             |
				  IBV_QP_MAX_QP_RD_ATOMIC)) {
			fprintf(stderr, "Failed to modify QP to RTS\n");
			goto clean_mqp;
		}
	}

	return ctx;

clean_mqp:
	ibv_destroy_qp(ctx->mqp);

clean_mcq:
	ibv_destroy_cq(ctx->mcq);

clean_qp:
	ibv_destroy_qp(ctx->qp);

clean_scq:
	ibv_destroy_cq(ctx->scq);

clean_rcq:
	ibv_destroy_cq(ctx->rcq);

clean_mr:
	ibv_dereg_mr(ctx->mr);

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_device:
	ibv_close_device(ctx->context);

clean_buffer:
	free(ctx->buf);

clean_ctx:
	free(ctx);

	return NULL;
}


int pp_close_ctx(struct pingpong_context *ctx)
{
	if (ibv_destroy_qp(ctx->mqp)) {
		fprintf(stderr, "Couldn't destroy mQP\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->mcq)) {
		fprintf(stderr, "Couldn't destroy mCQ\n");
		return 1;
	}

	if (ibv_destroy_qp(ctx->qp)) {
		fprintf(stderr, "Couldn't destroy QP\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->rcq)) {
		fprintf(stderr, "Couldn't destroy rCQ\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->scq)) {
		fprintf(stderr, "Couldn't destroy sCQ\n");
		return 1;
	}

	if (ibv_dereg_mr(ctx->mr)) {
		fprintf(stderr, "Couldn't deregister MR\n");
		return 1;
	}

	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	free(ctx->buf);
	free(ctx);

	return 0;
}


static int pp_post_recv(struct pingpong_context *ctx, int n)
{
	int rc;

	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->buf,
		.length = ctx->size,
		.lkey	= ctx->mr->lkey
	};
	struct ibv_recv_wr wr = {
		.wr_id		= PP_RECV_WRID,
		.sg_list	= &list,
		.num_sge	= 1,
	};
	struct ibv_recv_wr *bad_wr;
	int i;

	for (i = 0; i < n; ++i) {
		rc = ibv_post_recv(ctx->qp, &wr, &bad_wr);
		if (rc)
			return rc;
	}

	return i;
}

static int pp_post_send(struct pingpong_context *ctx, int wait_recv)
{
	int rc;
	struct ibv_exp_task task_post, task_en, task_wait, *task_p;
	struct ibv_wc mwc;
	struct ibv_wc wc;
	int ne;

	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->buf,
		.length = ctx->size,
		.lkey	= ctx->mr->lkey
	};

	struct ibv_exp_send_wr wr = {
		.wr_id	    = PP_SEND_WRID,
		.sg_list    = &list,
		.num_sge    = 1,
		.exp_opcode = IBV_EXP_WR_SEND,
		.exp_send_flags = IBV_EXP_SEND_SIGNALED,
	};

	struct ibv_exp_send_wr wr_en = {
		.wr_id	    = wr.wr_id,
		.sg_list    = NULL,
		.num_sge    = 0,
		.exp_opcode     = IBV_EXP_WR_SEND_ENABLE,
		.exp_send_flags = (wait_recv ? 0 : IBV_EXP_SEND_SIGNALED),
	};

	struct ibv_exp_send_wr wr_wait = {
		.wr_id	    = ctx->scnt,
		.sg_list    = NULL,
		.num_sge    = 0,
		.exp_opcode = IBV_EXP_WR_CQE_WAIT,
		.exp_send_flags = IBV_EXP_SEND_SIGNALED,
	};

	/* fill in send work calc request */
	if (ctx->calc_op.opcode != IBV_EXP_CALC_OP_NUMBER) {
		wr.exp_opcode = IBV_EXP_WR_SEND;
		wr.exp_send_flags |= IBV_EXP_SEND_WITH_CALC;
		wr.sg_list = ctx->calc_op.gather_list;
		wr.num_sge = ctx->calc_op.gather_list_size;

		wr.op.calc.calc_op   = ctx->calc_op.opcode;
		wr.op.calc.data_type = ctx->calc_op.data_type;
		wr.op.calc.data_size = ctx->calc_op.data_size;
		wr.next = NULL;
	}

	memset(&task_post, 0, sizeof(task_post));
	task_post.task_type = IBV_EXP_TASK_SEND;
	task_post.item.qp = ctx->qp;
	task_post.item.send_wr = &wr;

	task_post.next = NULL;
	task_p = &task_post;

	/* fill in send work enable request */
	{
		wr_en.task.wqe_enable.qp   = ctx->qp;
		wr_en.task.wqe_enable.wqe_count = 0;

		wr_en.exp_send_flags |= IBV_EXP_SEND_WAIT_EN_LAST;

		memset(&task_en, 0, sizeof(task_en));
		task_en.task_type = IBV_EXP_TASK_SEND;
		task_en.item.qp = ctx->mqp;
		task_en.item.send_wr = &wr_en;

		task_en.next = NULL;
		task_post.next = &task_en;
	}

	/* fill in wait work enable request */
	if (wait_recv) {
		wr_wait.task.cqe_wait.cq   = ctx->rcq;
		wr_wait.task.cqe_wait.cq_count = 1;

		wr_wait.exp_send_flags |=  IBV_EXP_SEND_WAIT_EN_LAST;
		wr_wait.next = NULL;

		memset(&task_wait, 0, sizeof(task_wait));
		task_wait.task_type = IBV_EXP_TASK_SEND;
		task_wait.item.qp = ctx->mqp;
		task_wait.item.send_wr = &wr_wait;

		task_wait.next		= &task_post;
		task_p			= &task_wait;
	}


	rc = ibv_exp_post_task(ctx->context, task_p, NULL);
	if (rc)
		return rc;

	do {
		rc = ibv_poll_cq(ctx->mcq, 1, &mwc);
		if (rc < 0)
			return -1;
	} while (rc == 0);

	if (mwc.status != IBV_WC_SUCCESS)
		return -1;

	do {
		ne = ibv_poll_cq(ctx->scq, 1, &wc);
		if (ne < 0) {
			fprintf(stderr, "poll CQ failed %d\n", ne);
			return 1;
		}
	} while (!ne);

	if (wc.status != IBV_WC_SUCCESS) {
		fprintf(stderr, "cqe error status %s (%d v:%d) for count %d\n",
			ibv_wc_status_str(wc.status),
			wc.status, wc.vendor_err,
			ctx->rcnt);
		return 1;
	}

	return 0;
}


static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n",
	       argv0);
	printf("  %s <host>     connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>           listen on/connect to port"
	       " <port> (default 18515)\n");
	printf("  -d, --ib-dev=<dev>          use IB device <dev> "
	       "(default first device found)\n");
	printf("  -i, --ib-port=<port>        use port <port> of IB device"
	       " (default 1)\n");
	printf("  -s, --size=<size>           size of message to exchange "
	       "(default 4096 minimum 16)\n");
	printf("  -m, --mtu=<size>            path MTU (default 1024)\n");
	printf("  -r, --rx-depth=<dep>        number of receives to post"
	       " at a time (default 500)\n");
	printf("  -n, --iters=<iters>         number of exchanges"
	       " (default 1000)\n");
	printf("  -l, --sl=<sl>               service level value\n");
	printf("  -e, --events                sleep on CQ events"
	       " (default poll)\n");
	printf("  -c, --calc                  calc operation (supported ADD)\n");
	printf("  -t, --op_type=<type>        calc operands type (supported INT64)\n");
	printf("  -o, --operands=<o1,o2,...>  comma separated list of"
	       " operands\n");
	printf("  -w, --wait_cq=cqn           wait for entries on cq\n");
}


int main(int argc, char *argv[])
{
	struct ibv_device	**dev_list;
	struct ibv_device	*ib_dev = NULL;
	struct pingpong_context *ctx;
	struct pingpong_dest	my_dest;
	struct pingpong_dest	*rem_dest;
	struct timeval		start, end;
	char			*ib_devname = NULL;
	char			*servername = NULL;
	int			port = 18515;
	int			ib_port = 1;
	int			size = 4096;

	enum ibv_mtu		mtu = IBV_MTU_1024;
	int			rx_depth = 500;
	int			iters = 1000;
	int			routs;
	int			num_cq_events = 0;
	int			sl = 0;

	enum pp_wr_data_type	calc_data_type = PP_DATA_TYPE_INVALID;
	enum pp_wr_calc_op	calc_opcode = PP_CALC_INVALID;
	char			*calc_operands_str = NULL;

	srand48(getpid() * time(NULL));

	while (1) {
		int c;

		static struct option long_options[] = {
			{ .name = "port",	.has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",	.has_arg = 1, .val = 'd' },
			{ .name = "ib-port",	.has_arg = 1, .val = 'i' },
			{ .name = "size",	.has_arg = 1, .val = 's' },
			{ .name = "mtu",	.has_arg = 1, .val = 'm' },
			{ .name = "rx-depth",   .has_arg = 1, .val = 'r' },
			{ .name = "iters",	.has_arg = 1, .val = 'n' },
			{ .name = "sl",		.has_arg = 1, .val = 'l' },
			{ .name = "events",	.has_arg = 0, .val = 'e' },
			{ .name = "calc",	.has_arg = 1, .val = 'c' },
			{ .name = "op_type",	.has_arg = 1, .val = 't' },
			{ .name = "operands",   .has_arg = 1, .val = 'o' },
			{ .name = "poll_mqe",   .has_arg = 0, .val = 'w' },
			{ 0 }
		};

		c = getopt_long(argc, argv, "p:d:i:s:m:r:n:l:et:c:o:wf",
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
			if (size < 16) {
				usage(argv[0]);
				return 1;
			}
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

		case 't':
			calc_data_type = pp_str_to_data_type(optarg);
			if (calc_data_type == PP_DATA_TYPE_INVALID) {
				printf("-E- invalid data types. Valid values are:\n");
				pp_print_data_type();
				return 1;
			}
			break;

		case 'o':
			calc_operands_str = strdup(optarg);
			break;

		case 'c':
			calc_opcode = pp_str_to_calc_op(optarg);
			if (calc_opcode == PP_CALC_INVALID) {
				printf("-E- invalid data types. Valid values are:\n");
				pp_print_calc_op();
				return 1;
			}
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind == argc - 1)
		servername = strdupa(argv[optind]);
	else if (optind < argc) {
		usage(argv[0]);
		return 1;
	}

	page_size = sysconf(_SC_PAGESIZE);

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		fprintf(stderr, "No IB devices found\n");
		return 1;
	}

	if (!ib_devname) {
		ib_dev = *dev_list;
		if (!ib_dev) {
			fprintf(stderr, "No IB devices found\n");
			return 1;
		}
	} else {
		int i;
		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]),
			    ib_devname))
				break;
		ib_dev = dev_list[i];
		if (!ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ib_devname);
			return 1;
		}
	}

	ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port,
			      calc_opcode, calc_data_type, calc_operands_str);
	if (!ctx)
		return 1;

	routs = pp_post_recv(ctx, ctx->rx_depth);
	if (routs < ctx->rx_depth) {
		fprintf(stderr, "Couldn't post receive (%d)\n", routs);
		return 1;
	}

	my_dest.lid = pp_get_local_lid(ctx->context, ib_port);
	my_dest.qpn = ctx->qp->qp_num;
	my_dest.psn = lrand48() & 0xffffff;
	if (!my_dest.lid) {
		fprintf(stderr, "Couldn't get local LID\n");
		return 1;
	}

	printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x\n",
		my_dest.lid, my_dest.qpn, my_dest.psn);

	if (servername)
		rem_dest = pp_client_exch_dest(servername, port, &my_dest);
	else
		rem_dest = pp_server_exch_dest(ctx, ib_port, mtu,
						   port, sl, &my_dest);

	if (!rem_dest)
		return 1;

	printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x\n",
		rem_dest->lid, rem_dest->qpn, rem_dest->psn);

	if (servername)
		if (pp_connect_ctx(ctx, ctx->qp, ib_port, my_dest.psn, mtu,
				       sl, rem_dest))
			return 1;

	if (servername) {
		if (pp_post_send(ctx, 0)) {
			fprintf(stderr, "Couldn't post send\n");
			return 1;
		}

		if (gettimeofday(&start, NULL)) {
			perror("gettimeofday");
			return 1;
		}
	}

	ctx->scnt = ctx->rcnt = 0;
	while (ctx->rcnt < iters && ctx->scnt < iters) {
		struct ibv_wc wc;
		int ne;

		do {
			ne = ibv_poll_cq(ctx->rcq, 1, &wc);
			if (ne < 0) {
				fprintf(stderr, "poll CQ failed %d\n", ne);
				return 1;
			}
		} while (ne < 1);

		if (wc.status != IBV_WC_SUCCESS) {
			fprintf(stderr, "cqe error status %s (%d v:%d)"
				" for count %d\n",
				ibv_wc_status_str(wc.status),
				wc.status, wc.vendor_err,
				ctx->rcnt);
			return 1;
		}

		ctx->rcnt++;

		if (pp_post_recv(ctx, 1) < 0) {
			fprintf(stderr, "Couldn't post receive\n");
			return 1;
		}

		if (pp_post_send(ctx, 1)) {
			fprintf(stderr, "Couldn't post send\n");
			return 1;
		}
	}


	if (gettimeofday(&end, NULL)) {
		perror("gettimeofday");
		return 1;
	}

	{
		float usec = (end.tv_sec - start.tv_sec) * 1000000 +
			(end.tv_usec - start.tv_usec);
		long long bytes = (long long) size * iters * 2;

		printf("%lld bytes in %.2f seconds = %.2f Mbit/sec\n",
			   bytes, usec / 1000000., bytes * 8. / usec);
		printf("%d iters in %.2f seconds = %.2f usec/iter\n",
			   iters, usec / 1000000., usec / iters);
	}

	ibv_ack_cq_events(ctx->rcq, num_cq_events);

	if (pp_close_ctx(ctx))
		return 1;

	ibv_free_device_list(dev_list);
	if (calc_operands_str)
		free(calc_operands_str);

	free(rem_dest);

	return 0;
}
