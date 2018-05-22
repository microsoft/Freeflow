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
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <math.h>
#include <infiniband/arch.h>

#include "cc_pingpong.h"


#define MAX(x, y)	(((x) > (y)) ? (x) : (y))
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#define LAMBDA		(0.00001)

#define EXEC_INT(calc_op, op1, op2)					\
	((calc_op) == PP_CALC_LXOR	? ((!(op1) && (op2)) || ((op1) && !(op2)))	\
	: (calc_op) == PP_CALC_BXOR	? (((op1) ^  (op2)))	\
	: (calc_op) == PP_CALC_LOR	? (((op1) || (op2)))	\
	: (calc_op) == PP_CALC_BOR	? (((op1) |  (op2)))	\
	: (calc_op) == PP_CALC_LAND	? (((op1) && (op2)))	\
	: (calc_op) == PP_CALC_BAND	? (((op1) &  (op2)))	\
	: EXEC_FLOAT(calc_op, op1, op2))

#define EXEC_FLOAT(calc_op, op1, op2)					\
	((calc_op) == PP_CALC_ADD		? (((op1) +  (op2)))	\
	: (calc_op) == PP_CALC_MAX	? (MAX((op1), (op2)))	\
	: (calc_op) == PP_CALC_MIN	? (MIN((op1), (op2)))	\
	: (calc_op) == PP_CALC_MAXLOC	? (MAX((op1), (op2)))	\
	: (calc_op) == PP_CALC_MINLOC	? (MIN((op1), (op2)))	\
	: 0)

#define VERIFY_FLOAT(calc_op, data_type, op1, op2, res)		\
	((calc_op) == PP_CALC_ADD ?				\
		((fabs((data_type)EXEC_FLOAT(calc_op, op1, op2) - (res))) < LAMBDA)\
	: (((data_type)EXEC_FLOAT(calc_op, op1, op2)) == (res)))			\


#define VERIFY_INT(calc_op, data_type, op1, op2, res)			\
	(((data_type)EXEC_INT(calc_op, op1, op2)) == (res))


#define EXEC_VER_FLOAT(verify, calc_op, data_type, op1, op2, res)	\
	((verify) ?                                                     \
		(VERIFY_FLOAT(calc_op, data_type, (*(data_type *)op1),	\
			(*(data_type *)op2), (*(data_type *)res)))          \
	: (data_type)EXEC_FLOAT(calc_op, (*(data_type *)op1), (*(data_type *)op2)))

#define EXEC_VER_INT(verify, calc_op, data_type, op1, op2, res)	\
	((verify) ?                                                     \
		(VERIFY_INT(calc_op, data_type, (*(data_type *)op1),	\
			(*(data_type *)op2), (*(data_type *)res)))	\
	: (data_type)EXEC_INT(calc_op, (*(data_type *)op1), (*(data_type *)op2)))


#define EXEC_VERIFY(calc_data_type, calc_op, verify, op1, op2, res)	\
	((calc_data_type) == PP_DATA_TYPE_INT8 ?			\
		EXEC_VER_INT(verify, calc_op, int8_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_INT16 ?			\
		EXEC_VER_INT(verify, calc_op, int16_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_INT32 ?                    \
		EXEC_VER_INT(verify, calc_op, int32_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_INT64 ?                    \
		EXEC_VER_INT(verify, calc_op, int64_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_UINT8 ?			\
		EXEC_VER_INT(verify, calc_op, uint8_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_UINT16 ?			\
		EXEC_VER_INT(verify, calc_op, uint16_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_UINT32 ?                  \
		EXEC_VER_INT(verify, calc_op, uint32_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_UINT64 ?                  \
		EXEC_VER_INT(verify, calc_op, uint64_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_FLOAT32 ?			\
		EXEC_VER_FLOAT(verify, calc_op, float, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_FLOAT64 ?                  \
		EXEC_VER_FLOAT(verify, calc_op, FLOAT64, op1, op2, res)	\
	: 0)

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
	enum pp_wr_calc_op          init_opcode;
	enum pp_wr_data_type        init_data_type;
	enum ibv_exp_calc_op        opcode;
	enum ibv_exp_calc_data_type data_type;
	enum ibv_exp_calc_data_size data_size;
	void                       *gather_buff;
	int                         gather_list_size;
	struct ibv_sge             *gather_list;
};

struct pingpong_context {
	struct ibv_context      *context;
	struct ibv_comp_channel *channel;
	struct ibv_pd           *pd;
	struct ibv_mr           *mr;
	struct ibv_cq           *cq;
	struct ibv_qp           *qp;

	struct ibv_qp           *mqp;
	struct ibv_cq           *mcq;

	void                    *buf;
	void                    *net_buf;
	int                      size;
	int                      rx_depth;
	int                      pending;
	uint64_t		 last_result;

	struct pingpong_calc_ctx calc_op;

	int use_grh;
	int gid_index;
	union ibv_gid dgid;
};

struct pingpong_dest {
	int lid;
	int qpn;
	int psn;
};

static int pp_prepare_net_buff(int do_neg,
				   enum pp_wr_data_type type,
				   const void *in_buff, void *net_buff,
				   enum ibv_exp_calc_data_type *out_type,
				   enum ibv_exp_calc_data_size *out_size)
{
	int to_mult = (do_neg ? -1 : 1);
	int rc = 0;

	*out_size = IBV_EXP_CALC_DATA_SIZE_64_BIT;

	switch (type) {
	case PP_DATA_TYPE_INT8:
		*(uint64_t *)net_buff = *(uint8_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_INT;
		break;

	case PP_DATA_TYPE_UINT8:
		*(uint64_t *)net_buff = *(uint8_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_UINT;
		break;

	case PP_DATA_TYPE_INT16:
		*(uint64_t *)net_buff = *(uint16_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_INT;
		break;

	case PP_DATA_TYPE_UINT16:
		*(uint64_t *)net_buff = *(uint16_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_UINT;
		break;

	case PP_DATA_TYPE_INT32:
		*(uint64_t *)net_buff = *(uint32_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_INT;
		break;

	case PP_DATA_TYPE_UINT32:
		*(uint64_t *)net_buff = *(uint32_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_UINT;
		break;

	case PP_DATA_TYPE_INT64:
		*(uint64_t *)net_buff = *(uint64_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_INT;
		break;

	case PP_DATA_TYPE_UINT64:
		*(uint64_t *)net_buff = *(uint64_t *)in_buff * to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_UINT;
		break;

	case PP_DATA_TYPE_FLOAT32:
		*(double *)net_buff = (double)(*(float *)in_buff * (float)to_mult);
		*out_type = IBV_EXP_CALC_DATA_TYPE_FLOAT;
		break;

	case PP_DATA_TYPE_FLOAT64:
		*(double *)net_buff = *(double *)in_buff * (double)to_mult;
		*out_type = IBV_EXP_CALC_DATA_TYPE_FLOAT;
		break;

	default:
		fprintf(stderr, "invalid data type %d\n", type);
		rc = EINVAL;
	};

	return rc;
}

static inline int pp_prepare_host_buff(int do_neg,
				    enum pp_wr_data_type type,
				    const void *in_buff, void *host_buff)
{
	union {
		uint64_t	ll;
		double	lf;
	} tmp_buff;
	int to_mult = (do_neg ? -1 : 1);
	int rc = 0;

	/* todo - add better support in FLOAT */
	tmp_buff.ll = ntohll(*(uint64_t *)in_buff) * to_mult;

	switch (type) {
	case PP_DATA_TYPE_INT8:
	case PP_DATA_TYPE_UINT8:
		*(uint8_t *)host_buff = (uint8_t)tmp_buff.ll;
		break;

	case PP_DATA_TYPE_INT16:
	case PP_DATA_TYPE_UINT16:
		*(uint16_t *)host_buff = (uint16_t)tmp_buff.ll;
		break;

	case PP_DATA_TYPE_INT32:
	case PP_DATA_TYPE_UINT32:
		*(uint32_t *)host_buff = (uint32_t)tmp_buff.ll;
		break;

	case PP_DATA_TYPE_INT64:
	case PP_DATA_TYPE_UINT64:
		*(uint64_t *)host_buff = (uint64_t)tmp_buff.ll;
		break;

	case PP_DATA_TYPE_FLOAT32:
		*(float *)host_buff = (float)tmp_buff.lf;
		break;

	case PP_DATA_TYPE_FLOAT64:
		*(double *)host_buff = (double)tmp_buff.lf;
		break;

	default:
		fprintf(stderr, "invalid data type %d\n", type);
		rc = EINVAL;
	};

	return rc;
}

struct calc_pack_input {
	enum pp_wr_calc_op           op;
	enum pp_wr_data_type         type;
	const void                  *host_buf;
	uint64_t                     id;
	enum ibv_exp_calc_op        *out_op;
	enum ibv_exp_calc_data_type *out_type;
	enum ibv_exp_calc_data_size *out_size;
	void                        *net_buf;
};

struct calc_unpack_input {
	enum pp_wr_calc_op       op;
	enum pp_wr_data_type     type;
	const void              *net_buf;
	uint64_t                *id;
	void                    *host_buf;
};

/**
 * pp_pack_data_for_calc - modify the format of the data read from the source
 * buffer so calculation can be done on it.
 *
 * The function may also modify the operation, to match the modified data.
 */
static int pp_pack_data_for_calc(struct ibv_context *context,
			       struct calc_pack_input *params)
{
	enum pp_wr_calc_op op;
	enum pp_wr_data_type type;
	const void *host_buffer;
	uint64_t id;
	enum ibv_exp_calc_op *out_op;
	enum ibv_exp_calc_data_type *out_type;
	enum ibv_exp_calc_data_size *out_size;
	void *network_buffer;
	int do_neg = 0;
	int conv_op_to_bin = 0;

	/* input parameters check */
	if (!context ||
		!params ||
		!params->host_buf ||
		!params->net_buf ||
		!params->out_op ||
		!params->out_type ||
		!params->out_size ||
		params->type == PP_DATA_TYPE_INVALID ||
		params->op == PP_CALC_INVALID)
		return EINVAL;

	/* network buffer must be 16B aligned */
	if ((uintptr_t)(params->net_buf) % 16) {
		fprintf(stderr, "network buffer must be 16B aligned\n");
		return EINVAL;
	}

	op = params->op;
	type = params->type;
	host_buffer = params->host_buf;
	id = params->id;
	out_op = params->out_op;
	out_type = params->out_type;
	out_size = params->out_size;
	network_buffer = params->net_buf;

	*out_op = IBV_EXP_CALC_OP_NUMBER;
	*out_type = IBV_EXP_CALC_DATA_TYPE_NUMBER;
	*out_size = IBV_EXP_CALC_DATA_SIZE_NUMBER;

	switch (op) {
	case PP_CALC_LXOR:
		*out_op = IBV_EXP_CALC_OP_BXOR;
		conv_op_to_bin = 1;
		break;

	case PP_CALC_LOR:
		*out_op = IBV_EXP_CALC_OP_BOR;
		conv_op_to_bin = 1;
		break;

	case PP_CALC_LAND:
		*out_op = IBV_EXP_CALC_OP_BAND;
		conv_op_to_bin = 1;
		break;

	case PP_CALC_MIN:
		*out_op = IBV_EXP_CALC_OP_MAXLOC;
		do_neg = 1;
		break;

	case PP_CALC_BXOR:
		*out_op = IBV_EXP_CALC_OP_BXOR;
		break;

	case PP_CALC_BOR:
		*out_op = IBV_EXP_CALC_OP_BOR;
		break;

	case PP_CALC_BAND:
		*out_op = IBV_EXP_CALC_OP_BAND;
		break;

	case PP_CALC_ADD:
		*out_op = IBV_EXP_CALC_OP_ADD;
		break;

	case PP_CALC_MAX:
		*out_op = IBV_EXP_CALC_OP_MAXLOC;
		break;

	case PP_CALC_MAXLOC:
	case PP_CALC_MINLOC:
	case PP_CALC_PROD:	/* Unsupported operation */
	case PP_CALC_INVALID:
	default:
		fprintf(stderr, "unsupported op %d\n", op);
		return EINVAL;
	}

	/* convert data from user defined buffer to hardware supported representation */
	if (pp_prepare_net_buff(do_neg, type, host_buffer, network_buffer, out_type, out_size))
		return EINVAL;

	/* logical operations use true/false */
	if (conv_op_to_bin)
		*(uint64_t *)network_buffer = !!(*(uint64_t *)network_buffer);

	/* convert to network order supported by hardware */
	*(uint64_t *)network_buffer = htonll(*(uint64_t *)network_buffer);

	/* for MINLOC/MAXLOC - copy the ID to the network buffer */
	if (op == PP_CALC_MINLOC || op == PP_CALC_MAXLOC)
		*(uint64_t *)((unsigned char *)network_buffer + 8) = htonll(id);

	return 0;
}

/**
 * pp_unpack_data_from_calc - modify the format of the data read from the
 * network to the format in which the host expects it.
 */
static int pp_unpack_data_from_calc(struct ibv_context *context,
				  struct calc_unpack_input *params)
{
	enum pp_wr_calc_op op;
	enum pp_wr_data_type type;
	const void *network_buffer;
	uint64_t *id;
	void *host_buffer;
	int do_neg = 0;

	if (!context ||
		!params ||
		!params->net_buf ||
		!params->host_buf ||
		params->type == PP_DATA_TYPE_INVALID ||
		params->op == PP_CALC_INVALID)
		return EINVAL;

	op = params->op;
	type = params->type;
	network_buffer = params->net_buf;
	id = params->id;
	host_buffer = params->host_buf;

	/* Check if it's needed to convert the buffer & operation */
	if ((op == PP_CALC_MIN) || (op == PP_CALC_MINLOC))
		do_neg = 1;

	/* convert data from hardware supported data representation to user defined buffer */
	if (pp_prepare_host_buff(do_neg, type, network_buffer, host_buffer))
		return EINVAL;

	/* for MINLOC/MAXLOC - return ID */
	if (op == PP_CALC_MINLOC || op == PP_CALC_MAXLOC) {
		if (id)
			*id = ntohll(*(uint64_t *)((unsigned char *)network_buffer + 8));
		else
			return EINVAL;
	}

	return 0;
}


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
			.is_global	= !!ctx->use_grh,
			.dlid		= dest->lid,
			.sl		= sl,
			.src_path_bits	= 0,
			.port_num	= port,
		}
	};
	if (ctx->use_grh) {
		attr.ah_attr.grh.sgid_index = ctx->gid_index;
		attr.ah_attr.grh.hop_limit = 1;
		attr.ah_attr.grh.dgid = ctx->dgid;
	}

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

	if(write(sockfd, "done", sizeof("done")) != sizeof("done")) {
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

	/* expecting "done" msg */
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

int pp_parse_calc_to_gather(char *ops_str,
			   enum pp_wr_calc_op calc_op,
			   enum pp_wr_data_type data_type,
			   struct pingpong_calc_ctx *calc_ctx,
			   struct ibv_context *ibv_ctx,
			   void *buff,
			   void *net_buff)
{
	struct calc_pack_input params;
	int i, num_operands;
	char *__gather_token, *__err_ptr = NULL;

	if (!ops_str) {
		fprintf(stderr, "You must choose an operation to perform.\n");
		return -1;
	}

	calc_ctx->init_opcode = calc_op;
	calc_ctx->init_data_type = data_type;
	calc_ctx->opcode = IBV_EXP_CALC_OP_NUMBER;
	calc_ctx->data_type = IBV_EXP_CALC_DATA_TYPE_NUMBER;
	calc_ctx->data_size = IBV_EXP_CALC_DATA_SIZE_NUMBER;

	for (i = 0, num_operands = 1; i < strlen(ops_str); i++) {
		if (ops_str[i] == ',')
			num_operands++;
	}

	calc_ctx->gather_list_size = num_operands;

	__gather_token = strtok(ops_str, ",");
	if (!__gather_token)
		return -1;

	/* Build the gather list, assume one operand per sge. todo: improve for any nr of operands */
	for (i = 0; i < num_operands; i++) {
		/* copy the operands to the buffer */
		switch (data_type) {
		case PP_DATA_TYPE_INT8:
			return -1;

		case PP_DATA_TYPE_INT16:
			return -1;

		case PP_DATA_TYPE_INT32:
		case PP_DATA_TYPE_UINT32:
			*((int32_t *)buff + i*4) = strtol(__gather_token, &__err_ptr, 0);
			break;

		case PP_DATA_TYPE_INT64:
		case PP_DATA_TYPE_UINT64:
			*((int64_t *)buff + i*2) = strtoll(__gather_token, &__err_ptr, 0);
			break;

		case PP_DATA_TYPE_FLOAT32:
			*((float *)buff + i*4) = strtof(__gather_token, &__err_ptr);
			break;

		case PP_DATA_TYPE_FLOAT64:
			*((FLOAT64 *)buff + i*2) = strtof(__gather_token, &__err_ptr);
			break;

		default:
			return -1;
		}

		memset(&params, 0, sizeof(params));
		params.op = calc_ctx->init_opcode;
		params.type = calc_ctx->init_data_type;
		params.host_buf = (int64_t *) buff + i * 2;
		params.id = 0;
		params.out_op = &calc_ctx->opcode;
		params.out_type = &calc_ctx->data_type;
		params.out_size = &calc_ctx->data_size;
		params.net_buf = (uint64_t *) net_buff + i * 2;

		if (pp_pack_data_for_calc(ibv_ctx, &params)) {
			fprintf(stderr, "Error in pack\n");
			return -1;
		}
		__gather_token = strtok(NULL, ",");
		if (!__gather_token)
			break;

	}

	calc_ctx->gather_buff = net_buff;

	return num_operands;
}

static int pp_prepare_sg_list(int op_per_gather,
			     int num_operands,
			     uint32_t lkey,
			     struct pingpong_calc_ctx *calc_ctx,
			     void *buff)
{
	int num_sge, sz;
	int i, gather_ix;
	struct ibv_sge *gather_list = NULL;

	/* Data size is based on datatype returned from pack
	 * Note: INT16, INT32, INT64 -> INT64 (sz=8)
	 */
	sz = -1;
	sz = pp_calc_data_size_to_bytes(calc_ctx->data_size);
	num_sge = (num_operands / op_per_gather) + ((num_operands % op_per_gather) ? 1 : 0); /* todo - change to ceil. requires -lm */

	gather_list = calloc(num_sge, sizeof(*gather_list));
	if (!gather_list) {
		fprintf(stderr, "Failed to allocate %Zu bytes for gather_list\n",
				(num_sge * sizeof(*gather_list)));
		return -1;
	}

	/* Build the gather list */
	for (i = 0, gather_ix = 0; i < num_operands; i++) {
		if (!(i % op_per_gather)) {
			gather_list[gather_ix].addr   = (uint64_t)(uintptr_t)buff + ((sz + 8) * i);
			gather_list[gather_ix].length = (sz + 8) * op_per_gather;
			gather_list[gather_ix].lkey   = lkey;

			gather_ix++;
		}
	}

	calc_ctx->gather_list = gather_list;

	return 0;
}

struct pingpong_context *pp_init_ctx(struct ibv_device *ib_dev, int size,
				     int rx_depth, int port, int use_event,
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

	ctx->net_buf = memalign(page_size, size);
	if (!ctx->net_buf) {
		fprintf(stderr, "Couldn't allocate work buf.\n");
		goto clean_buffer;
	}
	memset(ctx->net_buf, 0, size);

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		goto clean_net_buf;
	}

	if (use_event) {
		ctx->channel = ibv_create_comp_channel(ctx->context);
		if (!ctx->channel) {
			fprintf(stderr, "Couldn't create completion channel\n");
			goto clean_device;
		}
	} else
		ctx->channel = NULL;

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_comp_channel;
	}

	ctx->mr = ibv_reg_mr(ctx->pd, ctx->net_buf, size, IBV_ACCESS_LOCAL_WRITE);
	if (!ctx->mr) {
		fprintf(stderr, "Couldn't register MR\n");
		goto clean_pd;
	}

	if (calc_op != PP_CALC_INVALID) {
		int op_per_gather, num_op, max_num_op;

		ctx->calc_op.opcode	= IBV_EXP_CALC_OP_NUMBER;
		ctx->calc_op.data_type	= IBV_EXP_CALC_DATA_TYPE_NUMBER;
		ctx->calc_op.data_size	= IBV_EXP_CALC_DATA_SIZE_NUMBER;

		num_op = pp_parse_calc_to_gather(calc_operands_str, calc_op, calc_data_type,
					&ctx->calc_op, ctx->context, ctx->buf, ctx->net_buf);
		if (num_op < 0) {
			fprintf(stderr, "-E- failed parsing calc operators\n");
			goto clean_mr;
		}

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

		if (pp_prepare_sg_list(op_per_gather, num_op, ctx->mr->lkey, &ctx->calc_op, ctx->net_buf)) {
			fprintf(stderr, "-failed to prepare the sg list\n");
			goto clean_mr;
		}
	}

	ctx->cq = ibv_create_cq(ctx->context, rx_depth + 1, NULL,
				ctx->channel, 0);
	if (!ctx->cq) {
		fprintf(stderr, "Couldn't create CQ\n");
		goto clean_gather_list;
	}

	{
		struct ibv_exp_qp_init_attr attr = {
			.send_cq = ctx->cq,
			.recv_cq = ctx->cq,
			.cap	 = {
				.max_send_wr  = 16,
				.max_recv_wr  = rx_depth,
				.max_send_sge = 16,
				.max_recv_sge = 16
			},
			.qp_type = IBV_QPT_RC,
			.pd = ctx->pd
		};

		attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS | IBV_EXP_QP_INIT_ATTR_PD;
		attr.exp_create_flags = IBV_EXP_QP_CREATE_CROSS_CHANNEL;

		ctx->qp = ibv_exp_create_qp(ctx->context, &attr);
		if (!ctx->qp)  {
			fprintf(stderr, "Couldn't create QP\n");
			goto clean_cq;
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

	ctx->mcq = ibv_create_cq(ctx->context, rx_depth + 1, NULL,
				ctx->channel, 0);
	if (!ctx->mcq) {
		fprintf(stderr, "Couldn't create CQ for MQP\n");
		goto clean_qp;
	}

	{
		struct ibv_exp_qp_init_attr mattr = {
			.send_cq = ctx->mcq,
			.recv_cq = ctx->mcq,
			.cap	 = {
				.max_send_wr  = 1,
				.max_recv_wr  = rx_depth,
				.max_send_sge = 16,
				.max_recv_sge = 16
			},
			.qp_type = IBV_QPT_RC,
			.pd = ctx->pd
		};

		mattr.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS | IBV_EXP_QP_INIT_ATTR_PD;
		mattr.exp_create_flags = IBV_EXP_QP_CREATE_CROSS_CHANNEL;

		ctx->mqp = ibv_exp_create_qp(ctx->context, &mattr);
		if (!ctx->qp)  {
			fprintf(stderr, "Couldn't create MQP\n");
			goto clean_mcq;
		}
	}

	{
		struct ibv_qp_attr mattr = {
			.qp_state		= IBV_QPS_INIT,
			.pkey_index		= 0,
			.port_num		= port,
			.qp_access_flags	= 0
		};

		if (ibv_modify_qp(ctx->mqp, &mattr,
				  IBV_QP_STATE		|
				  IBV_QP_PKEY_INDEX	|
				  IBV_QP_PORT		|
				  IBV_QP_ACCESS_FLAGS)) {
			fprintf(stderr, "Failed to modify MQP to INIT\n");
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

clean_cq:
	ibv_destroy_cq(ctx->cq);

clean_gather_list:
	free(ctx->calc_op.gather_list);

clean_mr:
	ibv_dereg_mr(ctx->mr);

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_comp_channel:
	if (ctx->channel)
		ibv_destroy_comp_channel(ctx->channel);

clean_device:
	ibv_close_device(ctx->context);

clean_net_buf:
	free(ctx->net_buf);

clean_buffer:
	free(ctx->buf);

clean_ctx:
	free(ctx);

	return NULL;
}

int pp_close_ctx(struct pingpong_context *ctx)
{
	if (ibv_destroy_qp(ctx->qp)) {
		fprintf(stderr, "Couldn't destroy QP\n");
		return 1;
	}


	if (ibv_destroy_qp(ctx->mqp)) {
		fprintf(stderr, "Couldn't destroy MQP\n");
		return 1;
	}


	if (ibv_destroy_cq(ctx->cq)) {
		fprintf(stderr, "Couldn't destroy CQ\n");
		return 1;
	}

	if (ibv_destroy_cq(ctx->mcq)) {
		fprintf(stderr, "Couldn't destroy MCQ\n");
		return 1;
	}

	free(ctx->calc_op.gather_list);

	if (ibv_dereg_mr(ctx->mr)) {
		fprintf(stderr, "Couldn't deregister MR\n");
		return 1;
	}

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
	free(ctx->buf);
	free(ctx->net_buf);
	free(ctx);

	return 0;
}

static int pp_post_recv(struct pingpong_context *ctx, int n)
{
	int rc;

	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->net_buf,
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

static int pp_post_send(struct pingpong_context *ctx)
{
	int ret;

	struct ibv_sge list = {
		.addr	= (uintptr_t) ctx->net_buf,
		.length = ctx->size,
		.lkey	= ctx->mr->lkey
	};
	struct ibv_exp_send_wr wr = {
		.wr_id		= PP_SEND_WRID,
		.sg_list	= &list,
		.num_sge	= 1,
		.exp_opcode	= IBV_EXP_WR_SEND,
		.exp_send_flags	= IBV_EXP_SEND_SIGNALED,
	};
	struct ibv_exp_send_wr *bad_wr;
	/* If this is a calc operation - set the required params in the wr */
	if (ctx->calc_op.opcode != IBV_EXP_CALC_OP_NUMBER) {
		wr.exp_opcode  = IBV_EXP_WR_SEND;
		wr.exp_send_flags |= IBV_EXP_SEND_WITH_CALC;
		wr.sg_list = ctx->calc_op.gather_list;
		wr.num_sge = ctx->calc_op.gather_list_size;

		wr.op.calc.calc_op   = ctx->calc_op.opcode;
		wr.op.calc.data_type = ctx->calc_op.data_type;
		wr.op.calc.data_size = ctx->calc_op.data_size;

	}

	ret = ibv_exp_post_send(ctx->qp, &wr, &bad_wr);

	return ret;
}

int pp_post_ext_wqe(struct pingpong_context *ctx, enum ibv_exp_wr_opcode op)
{
	int ret;
	struct ibv_exp_send_wr wr = {
		.wr_id		= PP_CQE_WAIT,
		.sg_list	= NULL,
		.num_sge	= 0,
		.exp_opcode	= op,
		.exp_send_flags	= IBV_EXP_SEND_SIGNALED,
	};
	struct ibv_exp_send_wr *bad_wr;

	switch (op) {
	case IBV_EXP_WR_RECV_ENABLE:
	case IBV_EXP_WR_SEND_ENABLE:

		wr.task.wqe_enable.qp = ctx->qp;
		wr.task.wqe_enable.wqe_count = 0;

		wr.exp_send_flags |= IBV_EXP_SEND_WAIT_EN_LAST;

		break;

	case IBV_EXP_WR_CQE_WAIT:
		wr.task.cqe_wait.cq = ctx->cq;
		wr.task.cqe_wait.cq_count = 1;

		wr.exp_send_flags |=  IBV_EXP_SEND_WAIT_EN_LAST;

		break;

	default:
		fprintf(stderr, "-E- unsupported m_wqe opcode %d\n", op);
		return -1;
	}

	ret = ibv_exp_post_send(ctx->mqp, &wr, &bad_wr);

	return ret;
}

int pp_poll_mcq(struct ibv_cq *cq, int num_cqe)
{
	int ne; int i;
	struct ibv_wc wc[2];

	if (num_cqe > 2) {
		fprintf(stderr, "-E- max num cqe exceeded\n");
		return -1;
	}

	do {
		ne = ibv_poll_cq(cq, num_cqe, wc);
		if (ne < 0) {
			fprintf(stderr, "poll CQ failed %d\n", ne);
			return 1;
		}
	} while (ne < 1);

	for (i = 0; i < ne; ++i) {
		if (wc[i].status != IBV_WC_SUCCESS) {
			fprintf(stderr, "Failed %s status %s (%d)\n",
					wr_id_str[(int)wc[i].wr_id],
					ibv_wc_status_str(wc[i].status),
					wc[i].status);
			return 1;
		}

		if ((int) wc[i].wr_id != PP_CQE_WAIT) {
			fprintf(stderr, "invalid wr_id %" PRIx64 "\n", wc[i].wr_id);
			return -1;
		}
	}

	return 0;
}

static int pp_calc_verify(struct pingpong_context *ctx,
			 enum pp_wr_data_type calc_data_type,
			 enum pp_wr_calc_op calc_opcode)
{
	uint64_t *op1 = &(ctx->last_result);
	uint64_t *op2 = (uint64_t *)ctx->buf + 2;
	uint64_t *res = (uint64_t *)ctx->buf;

	return !EXEC_VERIFY(calc_data_type, calc_opcode, 1, op1, op2, res);
}

static int pp_update_last_result(struct pingpong_context *ctx,
				enum pp_wr_data_type calc_data_type,
				enum pp_wr_calc_op calc_opcode)
{
	/* EXEC_VERIFY derefence result parameter */
	uint64_t *dummy;

	uint64_t *op1 = (uint64_t *)ctx->buf;
	uint64_t *op2 = (uint64_t *)ctx->buf + 2;
	uint64_t res = (uint64_t)EXEC_VERIFY(calc_data_type, calc_opcode, 0, op1, op2, dummy);

	ctx->last_result = res;
	return 0;
}


static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s				start a server and wait for connection\n", argv0);
	printf("  %s <host>			connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>		listen on/connect to port <port> (default 18515)\n");
	printf("  -d, --ib-dev=<dev>		use IB device <dev> (default first device found)\n");
	printf("  -i, --ib-port=<port>		use port <port> of IB device (default 1)\n");
	printf("  -s, --size=<size>		size of message to exchange (default 4096 minimum 16)\n");
	printf("  -m, --mtu=<size>		path MTU (default 1024)\n");
	printf("  -r, --rx-depth=<dep>		number of receives to post at a time (default 500)\n");
	printf("  -n, --iters=<iters>		number of exchanges (default 1000)\n");
	printf("  -l, --sl=<sl>			service level value\n");
	printf("  -e, --events			sleep on CQ events (default poll)\n");
	printf("  -c, --calc=<operation>	calc operation\n");
	printf("  -t, --op_type=<type>		calc operands type\n");
	printf("  -o, --operands=<o1,o2,...>	comma separated list of operands\n");
	printf("  -w, --wait_cq=cqn		wait for entries on cq\n");
	printf("  -v, --verbose			print verbose information\n");
	printf("  -V, --verify			verify calc operations\n");
	printf("  -g, --gid-index		gid index\n");
	printf("  -G, --dgid			remote gid, must be given if -g is used\n");
}

int main(int argc, char *argv[])
{
	struct ibv_device	**dev_list;
	struct ibv_device	*ib_dev = NULL;
	struct pingpong_context *ctx;
	struct pingpong_dest	my_dest;
	struct pingpong_dest	*rem_dest = NULL;
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
	int			rcnt, scnt;
	int			use_event = 0;
	int			mqe_poll = 0;
	int			verbose = 0;
	int			verify = 0;

	struct calc_unpack_input params;

	enum		pp_wr_data_type	calc_data_type = PP_DATA_TYPE_INVALID;
	enum		pp_wr_calc_op	calc_opcode = PP_CALC_INVALID;
	char		*calc_operands_str = NULL;
	struct		ibv_wc wc[2];
	int		ne, i, use_grh = 0, ret = 0, dgid_provided = 0;
	int		gid_index = 0;
	union		ibv_gid dgid;

	srand48(getpid() * time(NULL));

	page_size = sysconf(_SC_PAGESIZE);

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
			{ .name = "verbose",	.has_arg = 0, .val = 'v' },
			{ .name = "verify",	.has_arg = 0, .val = 'V' },
			{ .name = "gid-index",	.has_arg = 1, .val = 'g' },
			{ .name = "dgid",	.has_arg = 1, .val = 'G' },
			{ 0 }
		};

		c = getopt_long(argc, argv, "p:d:i:s:m:r:n:l:et:c:o:wfvVg:G:", long_options, NULL);
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

		case 'v':
			verbose = 1;
			break;

		case 'V':
			verify = 1;
			break;

		case 'e':
			++use_event;
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

		case 'w':
			mqe_poll = 1;
			break;

		case 'g':
			gid_index = strtol(optarg, NULL, 0);
			if (gid_index < 0) {
				usage(argv[0]);
				return 1;
			}
			use_grh = 1;
			break;

		case 'G':
			if (!inet_pton(AF_INET6, optarg, &dgid.raw)) {
				printf("%pI6\n", optarg);
				usage(argv[0]);
				return 1;
			}
			dgid_provided = 1;
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (use_grh && !dgid_provided) {
		fprintf(stderr, "GRH should be used but remote GID was not provided\n");
		return EINVAL;
	}

	memset(&params, 0, sizeof(params));

	/* calc and data type are mandatory */
	if (calc_opcode == PP_CALC_INVALID || calc_data_type == PP_DATA_TYPE_INVALID) {
		fprintf(stderr, "Data type and calc operation must be specified\n");
		return 1;
	}

	/* Verify that all the parameters required for calc operation were set */
	if (!calc_operands_str) {
		fprintf(stderr, "Operands must be set for calc operation\n");
		return 1;
	}

	if (optind == argc - 1)
		servername = strdupa(argv[optind]);
	else if (optind < argc) {
		usage(argv[0]);
		return 1;
	}

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		fprintf(stderr, "No IB devices found\n");
		return 1;
	}

	if (ib_devname) {
		int i;

		for (i = 0; dev_list[i]; ++i) {
			if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname)) {
				ib_dev = dev_list[i];
				break;
			}
		}
		if (!ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ib_devname);
			return 1;
		}
	} else
		ib_dev = *dev_list;

	ctx = pp_init_ctx(ib_dev, size, rx_depth, ib_port, use_event,
			  calc_opcode, calc_data_type, calc_operands_str);
	if (!ctx)
		return 1;

	ctx->gid_index = gid_index;
	ctx->use_grh = use_grh;
	memcpy(&ctx->dgid.raw, &dgid.raw, sizeof(union ibv_gid));
	if (servername)
		pp_update_last_result(ctx, calc_data_type, calc_opcode);
	else
		ctx->last_result = *(uint64_t *)ctx->buf;

	routs = pp_post_recv(ctx, ctx->rx_depth);
	if (routs < ctx->rx_depth) {
		fprintf(stderr, "Couldn't post receive (%d)\n", routs);
		ret = 1;
		goto out;
	}

	if (use_event)
		if (ibv_req_notify_cq(ctx->cq, 0)) {
			fprintf(stderr, "Couldn't request CQ notification\n");
			ret = 1;
			goto out;
		}

	my_dest.lid = pp_get_local_lid(ctx->context, ib_port);
	my_dest.qpn = ctx->qp->qp_num;
	my_dest.psn = lrand48() & 0xffffff;
	if (!my_dest.lid) {
		fprintf(stderr, "Couldn't get local LID\n");
		ret = 1;
		goto out;
	}

	printf("  local address:  LID 0x%04x, QPN 0x%06x, PSN 0x%06x  : MQPN 0x%06x\n",
		   my_dest.lid, my_dest.qpn, my_dest.psn, ctx->mqp->qp_num);

	if (servername)
		rem_dest = pp_client_exch_dest(servername, port, &my_dest);
	else
		rem_dest = pp_server_exch_dest(ctx, ib_port, mtu, port, sl, &my_dest);

	if (!rem_dest) {
		ret = 1;
		goto out;
	}

	printf("  remote address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x\n",
		   rem_dest->lid, rem_dest->qpn, rem_dest->psn);

	if (servername)
		if (pp_connect_ctx(ctx, ctx->qp, ib_port, my_dest.psn, mtu, sl, rem_dest)) {
			ret = 1;
			goto out;
		}

	if (mqe_poll) {
		struct pingpong_dest loop_dest;

		loop_dest.lid = my_dest.lid;
		loop_dest.psn = my_dest.psn;
		loop_dest.qpn = ctx->mqp->qp_num;

		if (pp_connect_ctx(ctx, ctx->mqp, ib_port, my_dest.psn, mtu, sl, &loop_dest)) {
			fprintf(stderr, "failed moving mqp to RTS\n");
			ret = 1;
			goto out;
		}
	}

	ctx->pending = PP_RECV_WRID;

	if (servername) {
		if (pp_post_send(ctx)) {
			fprintf(stderr, "Couldn't post send\n");
			ret = 1;
			goto out;
		}
		ctx->pending |= PP_SEND_WRID;

	}

	if (gettimeofday(&start, NULL)) {
		perror("gettimeofday");
		ret = 1;
		goto out;

	}


	rcnt = scnt = 0;
	while (rcnt < iters || scnt < iters) {
		if (use_event) {
			struct ibv_cq *ev_cq;
			void		  *ev_ctx;

			if (ibv_get_cq_event(ctx->channel, &ev_cq, &ev_ctx)) {
				fprintf(stderr, "Failed to get cq_event\n");
				ret = 1;
				goto out;
			}

			++num_cq_events;

			if (ev_cq != ctx->cq) {
				fprintf(stderr, "CQ event for unknown CQ %p\n", ev_cq);
				ret = 1;
				goto out;
			}

			if (ibv_req_notify_cq(ctx->cq, 0)) {
				fprintf(stderr, "Couldn't request CQ notification\n");
				ret = 1;
				goto out;
			}
		}

		if (mqe_poll) {
			int ne;

			if (pp_post_ext_wqe(ctx, IBV_EXP_WR_CQE_WAIT)) {
				fprintf(stderr, "Failed posting cqe_wait wqe\n");
				ret = -1;
				goto out;
			}

			ne = pp_poll_mcq(ctx->mcq, 1);
			if (ne < 0) {
				fprintf(stderr, "poll MCQ failed %d\n", ne);
				ret = -1;
				goto out;
			}
		}

		do {
			ne = ibv_poll_cq(ctx->cq, 2, wc);
			if (ne < 0) {
				fprintf(stderr, "poll CQ failed %d\n", ne);
				ret = 1;
				goto out;
			}
		} while (!use_event && ne < 1);

		for (i = 0; i < ne; ++i) {
			if (wc[i].status != IBV_WC_SUCCESS) {
				fprintf(stderr, "Failed %s status %s (%d v:%d) for count %d\n",
					wr_id_str[(int) wc[i].wr_id],
					ibv_wc_status_str(wc[i].status), wc[i].status, wc[i].vendor_err,
					(int)(wc[i].wr_id == PP_SEND_WRID ? scnt : routs));
				ret = 1;
				goto out;
			}

			switch ((int)wc[i].wr_id) {
			case PP_SEND_WRID:
				++scnt;
				break;

			case PP_RECV_WRID:
				params.op = calc_opcode;
				params.type = calc_data_type;
				params.net_buf = ctx->net_buf;
				params.id = NULL;
				params.host_buf = ctx->buf;

				if (pp_unpack_data_from_calc(ctx->context, &params))
					fprintf(stderr, "Error in unpack \n");

				if (verbose) {

					switch (calc_data_type) {
					case PP_DATA_TYPE_INT32:
					case PP_DATA_TYPE_INT64:
					case PP_DATA_TYPE_UINT32:
					case PP_DATA_TYPE_UINT64:
						printf("incoming data is %" PRIu64 "\n", *(uint64_t *)ctx->buf);
						break;

					case PP_DATA_TYPE_FLOAT32:
						printf("incoming data is %f\n", *(float *)ctx->buf);
						break;

					case PP_DATA_TYPE_FLOAT64:
						printf("incoming data is %f\n", *(FLOAT64 *)ctx->buf);
						break;

					default:
						 printf("incoming data is 0%016" PRIu64 "\n",
							*(uint64_t *)ctx->buf);
					}
				}
				if (verify) {
					if (pp_calc_verify(ctx, calc_data_type, calc_opcode)) {
						fprintf(stderr, "Calc verification failed\n");
						ret = 1;
						goto out;
					}
				}
				pp_update_last_result(ctx, calc_data_type, calc_opcode);

				if (--routs <= 1) {
					routs += pp_post_recv(ctx, ctx->rx_depth - routs);

					if (routs < ctx->rx_depth) {
						fprintf(stderr,
							"Couldn't post receive (%d)\n",
							routs);
						ret = 1;
						goto out;
					}
				}

				++rcnt;
				break;

			default:
				fprintf(stderr, "Completion for unknown wr_id %d\n",
					(int) wc[i].wr_id);
				ret = 1;
				goto out;
			}

			ctx->pending &= ~(int)wc[i].wr_id;
			if (scnt < iters && !ctx->pending) {
				if (pp_post_send(ctx)) {
					fprintf(stderr, "Couldn't post send\n");
					ret = 1;
					goto out;
				}
				ctx->pending = PP_RECV_WRID | PP_SEND_WRID;
			}
		} /* for (i = 0; i < ne; ++i) */
	} /* while (rcnt < iters || scnt < iters) */

	if (gettimeofday(&end, NULL)) {
		perror("gettimeofday");
		ret = 1;
		goto out;
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

	ibv_ack_cq_events(ctx->cq, num_cq_events);
out:
	ret = pp_close_ctx(ctx);

	ibv_free_device_list(dev_list);
	if (calc_operands_str)
		free(calc_operands_str);

	free(rem_dest);

	return ret;
}
