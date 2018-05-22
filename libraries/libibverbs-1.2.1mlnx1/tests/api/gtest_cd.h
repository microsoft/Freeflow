/**
* Copyright (C) Mellanox Technologies Ltd. 2012.  ALL RIGHTS RESERVED.
* This software product is a proprietary product of Mellanox Technologies Ltd.
* (the "Company") and all right, title, and interest and to the software product,
* including all associated intellectual property rights, are and shall
* remain exclusively with the Company.
*
* This software product is governed by the End User License Agreement
* provided with the software product.
* $COPYRIGHT$
* $HEADER$
*/

#pragma pack( push, 1 )
struct test_entry {
	int lid;		/* LID of the IB port */
	int qpn;		/* QP number */
	int psn;
	uint32_t rkey;		/* Remote key */
	uintptr_t vaddr;	/* Buffer address */

	union ibv_gid gid;	/* GID of the IB port */
};
#pragma pack( pop )


enum pp_wr_data_type {
	PP_DATA_TYPE_INT8 = 0,
	PP_DATA_TYPE_INT16,
	PP_DATA_TYPE_INT32,
	PP_DATA_TYPE_INT64,
	PP_DATA_TYPE_UINT8,
	PP_DATA_TYPE_UINT16,
	PP_DATA_TYPE_UINT32,
	PP_DATA_TYPE_UINT64,
	PP_DATA_TYPE_FLOAT32,
	PP_DATA_TYPE_FLOAT64,
	PP_DATA_TYPE_FLOAT96,
	PP_DATA_TYPE_INVALID		/* Keep Last */
};

enum pp_wr_calc_op {
	PP_CALC_LXOR = 0,
	PP_CALC_BXOR,
	PP_CALC_LOR,
	PP_CALC_BOR,
	PP_CALC_LAND,
	PP_CALC_BAND,
	PP_CALC_ADD,
	PP_CALC_MAX,
	PP_CALC_MIN,
	PP_CALC_MAXLOC,
	PP_CALC_MINLOC,
	PP_CALC_PROD,
	PP_CALC_INVALID		/* Keep Last */
};

/*
 * IBV_EXP_WR_CALC_SEND is special opcode:
 * opcode = IBV_EXP_WR_SEND
 * send_flags = IBV_SEND_EXTENDED
 */
#define IBV_EXP_WR_CALC_SEND	((enum ibv_exp_wr_opcode)(IBV_EXP_WR_CQE_WAIT + 1))

struct pingpong_calc_ctx {
	enum pp_wr_calc_op            init_opcode;
	enum pp_wr_data_type          init_data_type;
	enum ibv_exp_calc_op          opcode;
	enum ibv_exp_calc_data_type   data_type;
	enum ibv_exp_calc_data_size   data_size;
	void                         *gather_buff;
	int                           gather_list_size;
	struct ibv_sge               *gather_list;
};

struct test_context {
	struct ibv_context     *context;	/* device handle */
	struct ibv_pd          *pd;		/* PD handle */
	struct ibv_mr          *mr;		/* MR handle for buf */
	struct ibv_cq          *scq;		/* sCQ handle */
	struct ibv_cq          *rcq;		/* rCQ handle */
	struct ibv_qp          *qp;		/* QP handle */
	struct ibv_cq          *mcq;		/* mCQ handle */
	struct ibv_qp          *mqp;		/* mQP handle */
#if defined(IBV_TEST_UD_ENABLED) && (IBV_TEST_UD_ENABLED == 1)
	struct ibv_ah          *ah;
#endif
	struct ibv_wc          *wc;             /* Work Completion array */
	void                   *net_buf;        /* memory buffer pointer, used for RDMA and send */
	void                   *buf;            /* local memory buffer pointer */
	int                     size;
	int                     cq_tx_depth;
	int                     cq_rx_depth;
	int                     qp_tx_depth;
	int                     qp_rx_depth;
	int                     port;
	struct test_entry	my_info;
	struct test_entry	peer_info;

	void                   *last_result;
	const char             *str_input;
	int                     pending;

	struct pingpong_calc_ctx calc_op;
};


enum {
	TEST_BASE_WRID = 0x0000FFFF,
	TEST_RECV_WRID = 0x00010000,
	TEST_SEND_WRID = 0x00020000
};

#define TEST_SET_WRID(type, code)   ((type) | (code))
#define TEST_GET_WRID(opcode)       ((opcode) & TEST_BASE_WRID)
#define TEST_GET_TYPE(opcode)       ((opcode) & (~TEST_BASE_WRID))


#define QP_PORT			1
#define MQP_PORT		1
#define DEFAULT_DEPTH		0x1F
#define MSG_SIZE		sizeof(uint64_t)
#define MAX_POLL_CQ_TIMEOUT	10000 /* poll CQ timeout in millisec (10 seconds) */

static INLINE int __calc_data_size_to_bytes(enum ibv_exp_calc_data_size data_size)
{
	switch (data_size) {
	case IBV_EXP_CALC_DATA_SIZE_64_BIT:     return 8;
	case IBV_EXP_CALC_DATA_SIZE_NUMBER: /* fall through */
	default:		       return -1;
	}
}

static INLINE const char *__calc_op_to_str(enum pp_wr_calc_op calc_op)
{
	switch (calc_op) {
	case PP_CALC_LXOR:   return "LXOR";
	case PP_CALC_BXOR:   return "BXOR";
	case PP_CALC_LOR:    return "LOR";
	case PP_CALC_BOR:    return "BOR";
	case PP_CALC_LAND:   return "LAND";
	case PP_CALC_BAND:   return "BAND";
	case PP_CALC_ADD:    return "ADD";
	case PP_CALC_MAX:    return "MAX";
	case PP_CALC_MIN:    return "MIN";
	case PP_CALC_MAXLOC: return "MAXLOC";
	case PP_CALC_MINLOC: return "MINLOC";
	case PP_CALC_PROD:   return "PROD";
	case PP_CALC_INVALID: /* fall through */
	default:		   return "invalid opcode";
	}
}

static INLINE int __data_type_to_size(enum pp_wr_data_type data_type)
{
	switch (data_type) {
	case PP_DATA_TYPE_INT8:     return 1;
	case PP_DATA_TYPE_INT16:    return 2;
	case PP_DATA_TYPE_INT32:    return 4;
	case PP_DATA_TYPE_INT64:    return 8;
	case PP_DATA_TYPE_UINT8:    return 1;
	case PP_DATA_TYPE_UINT16:   return 2;
	case PP_DATA_TYPE_UINT32:   return 4;
	case PP_DATA_TYPE_UINT64:   return 8;
	case PP_DATA_TYPE_FLOAT32:  return 4;
	case PP_DATA_TYPE_FLOAT64:  return 8;
	case PP_DATA_TYPE_INVALID: /* fall through */
	default:		       return -1;
	}
}

static INLINE const char *__data_type_to_str(enum pp_wr_data_type data_type)
{
	switch (data_type) {
	case PP_DATA_TYPE_INT8:     return "INT8";
	case PP_DATA_TYPE_INT16:    return "INT16";
	case PP_DATA_TYPE_INT32:    return "INT32";
	case PP_DATA_TYPE_INT64:    return "INT64";
	case PP_DATA_TYPE_UINT8:    return "UINT8";
	case PP_DATA_TYPE_UINT16:   return "UINT16";
	case PP_DATA_TYPE_UINT32:   return "UINT32";
	case PP_DATA_TYPE_UINT64:   return "UINT64";
	case PP_DATA_TYPE_FLOAT32:  return "FLOAT32";
	case PP_DATA_TYPE_FLOAT64:  return "FLOAT64";
	case PP_DATA_TYPE_INVALID: /* fall through */
	default:		       return "invalid data type";
	}
}

static INLINE const char* wr_id2str(uint64_t wr_id)
{
	switch (TEST_GET_TYPE(wr_id)) {
		case TEST_RECV_WRID:    return "RECV";
		case TEST_SEND_WRID:    return "SEND";
		default:	return "N/A";
	}
}


#define FLOAT64 double

#define MAX(x, y) 		(((x) > (y)) ? (x) : (y))
#define MIN(x, y) 		(((x) < (y)) ? (x) : (y))
#define LAMBDA			0.001

#define EXEC_INT(calc_op, op1, op2)					\
	((calc_op) == PP_CALC_LXOR	? ((!(op1) && (op2)) || ((op1) && !(op2)))	\
	: (calc_op) == PP_CALC_BXOR	? (((op1) ^  (op2)))	\
	: (calc_op) == PP_CALC_LOR 	? (((op1) || (op2)))	\
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

#define VERIFY_FLOAT(calc_op, data_type, op1, op2, res)       			\
	((calc_op) == PP_CALC_ADD ?				\
		((fabs((data_type)EXEC_FLOAT(calc_op, op1, op2) - (res))) < LAMBDA)\
	: (((data_type)EXEC_FLOAT(calc_op, op1, op2)) == (res)))			\


#define VERIFY_INT(calc_op, data_type, op1, op2, res)				\
	((data_type)EXEC_INT(calc_op, op1, op2)) == (res)


#define EXEC_VER_FLOAT(verify, calc_op, data_type, op1, op2, res) 	\
	((verify) ?                                                     \
		(VERIFY_FLOAT(calc_op, data_type, (*(data_type*)op1),     		\
			(*(data_type*)op2), (*(data_type*)res)))          \
	: (data_type)EXEC_FLOAT(calc_op, (*(data_type*)op1), (*(data_type*)op2)))

#define EXEC_VER_INT(verify, calc_op, data_type, op1, op2, res) 	\
	((verify) ?                                                     \
		(VERIFY_INT(calc_op, data_type, (*(data_type*)op1),       		\
			(*(data_type*)op2), (*(data_type*)res)))          \
	: (data_type)EXEC_INT(calc_op, (*(data_type*)op1), (*(data_type*)op2)))


#define EXEC_VERIFY(calc_data_type, calc_op, verify, op1, op2, res)   	\
	((calc_data_type) == PP_DATA_TYPE_INT8 ? 			\
		EXEC_VER_INT(verify, calc_op, int8_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_INT16 ?			\
		EXEC_VER_INT(verify, calc_op, int16_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_INT32 ?                   \
		EXEC_VER_INT(verify, calc_op, int32_t, op1, op2, res)	\
	: (calc_data_type) == PP_DATA_TYPE_INT64 ?                   \
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
	: (calc_data_type) == PP_DATA_TYPE_FLOAT64 ?                 \
		EXEC_VER_FLOAT(verify, calc_op, FLOAT64, op1, op2, res)	\
	: 0)

static int calc_display(struct test_context *ctx,
		       enum pp_wr_data_type calc_data_type,
		       enum pp_wr_calc_op calc_opcode) GTEST_ATTRIBUTE_UNUSED_;
static int calc_display(struct test_context *ctx,
		       enum pp_wr_data_type calc_data_type,
		       enum pp_wr_calc_op calc_opcode)
{
	void *op1 = ctx->last_result;
	void *op2 = (uint64_t *)ctx->buf + 2;
	void *res = (uint64_t *)ctx->buf;

	return EXEC_VERIFY(calc_data_type, calc_opcode, 0, op1, op2, res);
}

static int calc_verify(struct test_context *ctx,
		       enum pp_wr_data_type calc_data_type,
		       enum pp_wr_calc_op calc_opcode) GTEST_ATTRIBUTE_UNUSED_;
static int calc_verify(struct test_context *ctx,
		       enum pp_wr_data_type calc_data_type,
		       enum pp_wr_calc_op calc_opcode)
{
	void *op1 = ctx->last_result;
	void *op2 = (uint64_t *)ctx->buf + 2;
	void *res = (uint64_t *)ctx->buf;

	return !EXEC_VERIFY(calc_data_type, calc_opcode, 1, op1, op2, res);
}

static int update_last_result(struct test_context *ctx,
		enum pp_wr_data_type calc_data_type,
		enum pp_wr_calc_op calc_opcode) GTEST_ATTRIBUTE_UNUSED_;
static int update_last_result(struct test_context *ctx,
		enum pp_wr_data_type calc_data_type,
		enum pp_wr_calc_op calc_opcode)
{
#if 0
	/* EXEC_VERIFY derefence result parameter */
	uint64_t dummy;

	uint64_t *op1 = (uint64_t *)ctx->buf;
	uint64_t *op2 = (uint64_t *)ctx->buf + 2;
	uint64_t res = (uint64_t)EXEC_VERIFY(calc_data_type, calc_opcode, 0, op1, op2, &dummy);

	ctx->last_result = res;
#else
	memcpy(ctx->last_result, ctx->buf, sizeof(uint64_t));
#endif
	return 0;
}

static INLINE int __prepare_net_buff(bool do_neg,
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

static INLINE int __prepare_host_buff(bool do_neg,
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
	enum pp_wr_calc_op            op;
	enum pp_wr_data_type          type;
	const void                   *host_buf;
	uint64_t                      id;
	enum ibv_exp_calc_op         *out_op;
	enum ibv_exp_calc_data_type  *out_type;
	enum ibv_exp_calc_data_size  *out_size;
	void                         *net_buf;
};

struct calc_unpack_input {
	enum pp_wr_calc_op        op;
	enum pp_wr_data_type      type;
	const void                *net_buf;
	uint64_t                  *id;
	void                      *host_buf;
};

/**
 * __pack_data_for_calc - modify the format of the data read from the source
 * buffer so calculation can be done on it.
 *
 * The function may also modify the operation, to match the modified data.
 */
static int __pack_data_for_calc(struct ibv_context *context,
			       struct calc_pack_input *params) GTEST_ATTRIBUTE_UNUSED_;
static int __pack_data_for_calc(struct ibv_context *context,
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
	bool do_neg = false;
	bool conv_op_to_bin = false;

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
		conv_op_to_bin = true;
		break;

	case PP_CALC_LOR:
		*out_op = IBV_EXP_CALC_OP_BOR;
		conv_op_to_bin = true;
		break;

	case PP_CALC_LAND:
		*out_op = IBV_EXP_CALC_OP_BAND;
		conv_op_to_bin = true;
		break;

	case PP_CALC_MIN:
	case PP_CALC_MINLOC:
		*out_op = IBV_EXP_CALC_OP_MAXLOC;
		do_neg = true;
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
	case PP_CALC_MAXLOC:
		*out_op = IBV_EXP_CALC_OP_MAXLOC;
		break;

	case PP_CALC_PROD:	/* Unsupported operation */
	case PP_CALC_INVALID:
	default:
		fprintf(stderr, "unsupported op %d\n", op);
		return EINVAL;
	}

	/* convert data from user defined buffer to hardware supported representation */
	if (__prepare_net_buff(do_neg, type, host_buffer, network_buffer, out_type, out_size))
		return EINVAL;

	/* logical operations use true/false */
	if (conv_op_to_bin)
		*(uint64_t *)network_buffer = !!(*(uint64_t *)network_buffer);

	/* convert to network order supported by hardware */
	*(uint64_t *)network_buffer = htonll(*(uint64_t *)network_buffer);

	/* for MINLOC/MAXLOC - copy the ID to the network buffer */
	if (op == PP_CALC_MINLOC || op == PP_CALC_MAXLOC)
		*(uint64_t *)((unsigned char*)network_buffer + 8) = htonll(id);

	return 0;
}

/**
 * __unpack_data_from_calc - modify the format of the data read from the
 * network to the format in which the host expects it.
 */
static int __unpack_data_from_calc(struct ibv_context *context,
				  struct calc_unpack_input *params) GTEST_ATTRIBUTE_UNUSED_;
static int __unpack_data_from_calc(struct ibv_context *context,
				  struct calc_unpack_input *params)
{
	enum pp_wr_calc_op op;
	enum pp_wr_data_type type;
	const void *network_buffer;
	uint64_t *id;
	void *host_buffer;
	bool do_neg = false;

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
		do_neg = true;

	/* convert data from hardware supported data representation to user defined buffer */
	if (__prepare_host_buff(do_neg, type, network_buffer, host_buffer))
		return EINVAL;

	/* for MINLOC/MAXLOC - return ID */
	if (op == PP_CALC_MINLOC || op == PP_CALC_MAXLOC) {
		if (id)
			*id = ntohll(*(uint64_t *)((unsigned char*)network_buffer + 8));
		else
			return EINVAL;
	}

	return 0;
}

class verbs_test_cd : public verbs_test {
protected:
	virtual void SetUp() {

		int rc = EOK;

		/* First, we need to set up the super fixture (verbs_test). */
		verbs_test::SetUp();

		EXPECT_TRUE((device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL))
				<< "This class of tests is for Cross-Channel functionality."
				<< " But device does not support one";

		ctx = (struct test_context*)malloc(sizeof(struct test_context));
		ASSERT_TRUE(ctx != NULL);

		memset(ctx, 0, sizeof(*ctx));

		ctx->context = ibv_ctx;
		ctx->port = QP_PORT;
		ctx->qp_tx_depth = 20;
		ctx->qp_rx_depth = 100;
		ctx->cq_tx_depth = ctx->qp_tx_depth;
		ctx->cq_rx_depth = ctx->qp_rx_depth;
		ctx->size = sysconf(_SC_PAGESIZE);

		/*
		 * A Protection Domain (PD) allows the user to restrict which components can interact
		 * with only each other. These components can be AH, QP, MR, and SRQ
		 */
		ctx->pd = ibv_alloc_pd(ctx->context);
		ASSERT_TRUE(ctx->pd != NULL);

		/*
		 * VPI only works with registered memory. Any memory buffer which is valid in
		 * the process's virtual space can be registered. During the registration process
		 * the user sets memory permissions and receives a local and remote key
		 * (lkey/rkey) which will later be used to refer to this memory buffer
		 */
		ctx->net_buf = memalign(sysconf(_SC_PAGESIZE), ctx->size);
		ASSERT_TRUE(ctx->net_buf != NULL);
		memset(ctx->net_buf, 0, ctx->size);

		ctx->mr = ibv_reg_mr(ctx->pd, ctx->net_buf, ctx->size, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
		ASSERT_TRUE(ctx->mr != NULL);

		ctx->buf = memalign(sysconf(_SC_PAGESIZE), ctx->size);
		ASSERT_TRUE(ctx->buf != NULL);
		memset(ctx->buf, 0, ctx->size);

		ctx->last_result = memalign(sysconf(_SC_PAGESIZE), sizeof(uint64_t));
		ASSERT_TRUE(ctx->last_result != NULL);
		memset(ctx->last_result, 0, sizeof(uint64_t));

		ctx->mcq = ibv_create_cq(ctx->context, 0x10, NULL, NULL, 0);
		ASSERT_TRUE(ctx->mcq != NULL);

		{
			struct ibv_exp_qp_init_attr init_attr;

			memset(&init_attr, 0, sizeof(init_attr));

			init_attr.qp_context = NULL;
			init_attr.send_cq = ctx->mcq;
			init_attr.recv_cq = ctx->mcq;
			init_attr.srq = NULL;
			init_attr.cap.max_send_wr  = 0x40;
			init_attr.cap.max_recv_wr  = 0;
			init_attr.cap.max_send_sge = 16;
			init_attr.cap.max_recv_sge = 16;
			init_attr.cap.max_inline_data = 0;
			init_attr.qp_type = IBV_QPT_RC;
			init_attr.sq_sig_all = 0;
			init_attr.pd = ctx->pd;

			if (device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL) {
				init_attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS | IBV_EXP_QP_INIT_ATTR_PD;
				init_attr.exp_create_flags = IBV_EXP_QP_CREATE_CROSS_CHANNEL;
				ctx->mqp = ibv_exp_create_qp(ctx->context, &init_attr);
			} else {
				init_attr.comp_mask |= IBV_QP_INIT_ATTR_PD;
				ctx->mqp = ibv_exp_create_qp(ctx->context, &init_attr);
			}
			ASSERT_TRUE(ctx->mqp != NULL);
		}

		{
			struct ibv_qp_attr attr;

			memset(&attr, 0, sizeof(attr));

			attr.qp_state        = IBV_QPS_INIT;
			attr.pkey_index      = 0;
			attr.port_num        = MQP_PORT;
			attr.qp_access_flags = 0;

			rc = ibv_modify_qp(ctx->mqp, &attr,
					IBV_QP_STATE              |
					IBV_QP_PKEY_INDEX         |
					IBV_QP_PORT               |
					IBV_QP_ACCESS_FLAGS);
			ASSERT_EQ(rc, EOK);

			memset(&attr, 0, sizeof(attr));

			attr.qp_state              = IBV_QPS_RTR;
			attr.path_mtu              = IBV_MTU_1024;
			attr.dest_qp_num	   = ctx->mqp->qp_num;
			attr.rq_psn                = 0;
			attr.max_dest_rd_atomic    = 1;
			attr.min_rnr_timer         = 12;
			attr.ah_attr.is_global     = 0;
			attr.ah_attr.dlid          = 0;
			attr.ah_attr.sl            = 0;
			attr.ah_attr.src_path_bits = 0;
			attr.ah_attr.port_num      = 0;

			rc = ibv_modify_qp(ctx->mqp, &attr,
					IBV_QP_STATE              |
					IBV_QP_AV                 |
					IBV_QP_PATH_MTU           |
					IBV_QP_DEST_QPN           |
					IBV_QP_RQ_PSN             |
					IBV_QP_MAX_DEST_RD_ATOMIC |
					IBV_QP_MIN_RNR_TIMER);
			ASSERT_EQ(rc, EOK);

			memset(&attr, 0, sizeof(attr));

			attr.qp_state      = IBV_QPS_RTS;
			attr.timeout       = 14;
			attr.retry_cnt     = 7;
			attr.rnr_retry     = 7;
			attr.sq_psn        = 0;
			attr.max_rd_atomic = 1;
			rc = ibv_modify_qp(ctx->mqp, &attr,
					IBV_QP_STATE              |
					IBV_QP_TIMEOUT            |
					IBV_QP_RETRY_CNT          |
					IBV_QP_RNR_RETRY          |
					IBV_QP_SQ_PSN             |
					IBV_QP_MAX_QP_RD_ATOMIC);
			ASSERT_EQ(rc, EOK);
		}
	}

	virtual void TearDown() {

		if (ctx->mqp)
			ibv_destroy_qp(ctx->mqp);
		if (ctx->mcq)
			ibv_destroy_cq(ctx->mcq);
		if (ctx->qp)
			ibv_destroy_qp(ctx->qp);
		if (ctx->scq)
			ibv_destroy_cq(ctx->scq);
		if (ctx->rcq)
			ibv_destroy_cq(ctx->rcq);
		if (ctx->wc)
			free(ctx->wc);
#if defined(IBV_TEST_UD_ENABLED) && (IBV_TEST_UD_ENABLED == 1)
		if (ctx->ah)
			ibv_destroy_ah(ctx->ah);
#endif
		if (ctx->mr)
			ibv_dereg_mr(ctx->mr);
		if (ctx->pd)
			ibv_dealloc_pd(ctx->pd);
	        if (ctx->net_buf)
			free(ctx->net_buf);
	        if (ctx->buf)
			free(ctx->buf);
	        if (ctx->last_result)
			free(ctx->last_result);
	        if (ctx)
			free(ctx);

	        ctx = NULL;

		verbs_test::TearDown();

	}

	void __init_test(int qp_flag = 0, int qp_tx_depth = DEFAULT_DEPTH, int qp_rx_depth = DEFAULT_DEPTH,
			 int cq_tx_flag = 0, int cq_tx_depth = DEFAULT_DEPTH,
			 int cq_rx_flag = 0, int cq_rx_depth = DEFAULT_DEPTH) {

		int rc = EOK;

		ctx->qp_tx_depth = qp_tx_depth;
		ctx->qp_rx_depth = qp_rx_depth;
		ctx->cq_tx_depth = cq_tx_depth;
		ctx->cq_rx_depth = cq_rx_depth;

		/*
		 * A CQ contains completed work requests (WR). Each WR will generate a completion
		 * queue event (CQE) that is placed on the CQ. The CQE will specify if the WR was
		 * completed successfully or not.
		 */
		{
			ctx->scq = ibv_create_cq(ctx->context, ctx->cq_tx_depth, NULL, NULL, 0);
			ASSERT_TRUE(ctx->scq != NULL);

			if (cq_tx_flag)	{
				struct ibv_exp_cq_attr attr;

				attr.comp_mask            = IBV_EXP_CQ_ATTR_CQ_CAP_FLAGS;
				attr.moderation.cq_count  = 0;
				attr.moderation.cq_period = 0;
				attr.cq_cap_flags         = cq_tx_flag;

				rc = ibv_exp_modify_cq(ctx->scq, &attr, IBV_EXP_CQ_CAP_FLAGS);
				ASSERT_EQ(rc, EOK);
			}
		}

		{
			ctx->rcq = ibv_create_cq(ctx->context, ctx->cq_rx_depth, NULL, NULL, 0);
			ASSERT_TRUE(ctx->rcq != NULL);

			if (cq_rx_flag)	{
				struct ibv_exp_cq_attr attr;

				attr.comp_mask            = IBV_EXP_CQ_ATTR_CQ_CAP_FLAGS;
				attr.moderation.cq_count  = 0;
				attr.moderation.cq_period = 0;
				attr.cq_cap_flags         = cq_rx_flag;

				rc = ibv_exp_modify_cq(ctx->rcq, &attr, IBV_EXP_CQ_CAP_FLAGS);
				ASSERT_EQ(rc, EOK);
			}
		}

		ctx->wc = (struct ibv_wc*)malloc((ctx->cq_tx_depth + ctx->cq_rx_depth) * sizeof(struct ibv_wc));
		ASSERT_TRUE(ctx->wc != NULL);

		/*
		 * Creating a QP will also create an associated send queue and receive queue.
		 */
		{
			struct ibv_exp_qp_init_attr init_attr;

			memset(&init_attr, 0, sizeof(init_attr));

			init_attr.qp_context = NULL;
			init_attr.send_cq = ctx->scq;
			init_attr.recv_cq = ctx->rcq;
			init_attr.srq = NULL;
			init_attr.cap.max_send_wr  = ctx->qp_tx_depth;
			init_attr.cap.max_recv_wr  = ctx->qp_rx_depth;
			init_attr.cap.max_send_sge = 16;
			init_attr.cap.max_recv_sge = 16;
			init_attr.cap.max_inline_data = 0;
			init_attr.qp_type = IBV_QPT_RC;
#if defined(IBV_TEST_UD_ENABLED) && (IBV_TEST_UD_ENABLED == 1)
			init_attr.qp_type = IBV_QPT_UD;
#endif
			init_attr.sq_sig_all = 0;
			init_attr.pd = ctx->pd;

			{
				init_attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS | IBV_EXP_QP_INIT_ATTR_PD;
				init_attr.exp_create_flags = (enum ibv_exp_qp_create_flags)qp_flag;
				ctx->qp = ibv_exp_create_qp(ctx->context, &init_attr);
			}
			ASSERT_TRUE(ctx->qp != NULL);
		}

		/*
		 * Exchange connection info with a peer
		 * Convert all data in net order during this stage to guarantee correctness
		 */
		{
			struct test_entry local_info;
			struct test_entry remote_info;

			/*
			 * My connection info
			 */
			ctx->my_info.lid = get_local_lid(ctx->context, ctx->port);
			ctx->my_info.qpn = ctx->qp->qp_num;
			ctx->my_info.psn = lrand48() & 0xffffff;
			ctx->my_info.vaddr = (uintptr_t)ctx->net_buf + ctx->size;
			ctx->my_info.rkey = ctx->mr->rkey;
			memset(&(ctx->my_info.gid), 0, sizeof(ctx->my_info.gid));

			local_info.lid = htons(ctx->my_info.lid);
			local_info.qpn = htonl(ctx->my_info.qpn);
			local_info.psn = htonl(ctx->my_info.psn);
			local_info.vaddr = htonll(ctx->my_info.vaddr);
			local_info.rkey = htonl(ctx->my_info.rkey);
			memcpy(&(local_info.gid), &(ctx->my_info.gid), sizeof(ctx->my_info.gid));

			/*
			 * Exchange procedure  (We use loopback)...................
			 */
			memcpy(&remote_info, &local_info, sizeof(remote_info));

			/*
			 * Peer connection info
			 */
			ctx->peer_info.lid = ntohs(remote_info.lid);
			ctx->peer_info.qpn = ntohl(remote_info.qpn);
			ctx->peer_info.psn = ntohl(remote_info.psn);
			ctx->peer_info.vaddr = ntohll(remote_info.vaddr);
			ctx->peer_info.rkey = ntohl(remote_info.rkey);
			memcpy(&(ctx->peer_info.gid), &(remote_info.gid), sizeof(ctx->peer_info.gid));
		}

		ASSERT_LE(__post_read(ctx, ctx->qp_rx_depth), ctx->qp_rx_depth);

		/*
		 * A created QP still cannot be used until it is transitioned through several states,
		 * eventually getting to Ready To Send (RTS).
		 * IBV_QPS_INIT -> IBV_QPS_RTR -> IBV_QPS_RTS
		 */
#if defined(IBV_TEST_UD_ENABLED) && (IBV_TEST_UD_ENABLED == 1)
		{
			struct ibv_ah_attr ah_attr;
			struct ibv_qp_attr attr;

			memset(&attr, 0, sizeof(attr));

			attr.qp_state        = IBV_QPS_INIT;
			attr.pkey_index      = 0;
			attr.port_num        = ctx->port;
			attr.qp_access_flags = 0;
			attr.qkey            = 0x11111111;

			rc = ibv_modify_qp(ctx->qp, &attr,
					IBV_QP_STATE              |
					IBV_QP_PKEY_INDEX         |
					IBV_QP_PORT               |
					IBV_QP_QKEY);
			ASSERT_EQ(rc, EOK);

			memset(&attr, 0, sizeof(attr));

			attr.qp_state              = IBV_QPS_RTR;

			rc = ibv_modify_qp(ctx->qp, &attr,
					IBV_QP_STATE);
			ASSERT_EQ(rc, EOK);

			memset(&attr, 0, sizeof(attr));

			attr.qp_state      = IBV_QPS_RTS;
			attr.sq_psn        = ctx->my_info.psn;

			rc = ibv_modify_qp(ctx->qp, &attr,
					IBV_QP_STATE              |
					IBV_QP_SQ_PSN);
			ASSERT_EQ(rc, EOK);

			memset(&ah_attr, 0, sizeof(ah_attr));

			ah_attr.is_global    = 0;
			ah_attr.dlid          = ctx->peer_info.lid;
			ah_attr.sl            = 0;
			ah_attr.src_path_bits = 0;
			ah_attr.port_num      = ctx->port;

			ctx->ah = ibv_create_ah(ctx->pd, &ah_attr);
			ASSERT_TRUE(ctx->ah != NULL);
		}
#else
		{
			struct ibv_qp_attr attr;

			memset(&attr, 0, sizeof(attr));

			attr.qp_state        = IBV_QPS_INIT;
			attr.pkey_index      = 0;
			attr.port_num        = ctx->port;
			attr.qp_access_flags = IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

			rc = ibv_modify_qp(ctx->qp, &attr,
					IBV_QP_STATE              |
					IBV_QP_PKEY_INDEX         |
					IBV_QP_PORT               |
					IBV_QP_ACCESS_FLAGS);
			ASSERT_EQ(rc, EOK);

			memset(&attr, 0, sizeof(attr));

			attr.qp_state              = IBV_QPS_RTR;
			attr.path_mtu              = IBV_MTU_1024;
			attr.dest_qp_num	   = ctx->peer_info.qpn;
			attr.rq_psn                = ctx->peer_info.psn;
			attr.max_dest_rd_atomic    = 4;
			attr.min_rnr_timer         = 12;
			attr.ah_attr.is_global     = 0;
			attr.ah_attr.dlid          = ctx->peer_info.lid;
			attr.ah_attr.sl            = 0;
			attr.ah_attr.src_path_bits = 0;
			attr.ah_attr.port_num      = ctx->port;

			rc = ibv_modify_qp(ctx->qp, &attr,
					IBV_QP_STATE              |
					IBV_QP_AV                 |
					IBV_QP_PATH_MTU           |
					IBV_QP_DEST_QPN           |
					IBV_QP_RQ_PSN             |
					IBV_QP_MAX_DEST_RD_ATOMIC |
					IBV_QP_MIN_RNR_TIMER);
			ASSERT_EQ(rc, EOK);

			memset(&attr, 0, sizeof(attr));

			attr.qp_state      = IBV_QPS_RTS;
			attr.timeout       = 14;
			attr.retry_cnt     = 7;
			attr.rnr_retry     = 7;
			attr.sq_psn        = ctx->my_info.psn;
			attr.max_rd_atomic = 4;

			rc = ibv_modify_qp(ctx->qp, &attr,
					IBV_QP_STATE              |
					IBV_QP_TIMEOUT            |
					IBV_QP_RETRY_CNT          |
					IBV_QP_RNR_RETRY          |
					IBV_QP_SQ_PSN             |
					IBV_QP_MAX_QP_RD_ATOMIC);
			ASSERT_EQ(rc, EOK);
		}
#endif
	}

protected:
	struct test_context *ctx;

	uint16_t get_local_lid(struct ibv_context *context, int port)
	{
		struct ibv_port_attr attr;

		if (ibv_query_port(context, port, &attr))
			return 0;

		return attr.lid;
	}

	int __post_write(struct test_context *ctx, int64_t wrid, enum ibv_exp_wr_opcode opcode)
	{
		int rc = EOK;
		struct ibv_sge list;
		struct ibv_exp_send_wr wr;
		struct ibv_exp_send_wr *bad_wr;

		*(uint64_t*)ctx->net_buf = htonll(wrid);

		/* prepare the scatter/gather entry */
		memset(&list, 0, sizeof(list));
		list.addr = (uintptr_t)ctx->net_buf;
		list.length = ctx->size;
		list.lkey = ctx->mr->lkey;

		/* prepare the send work request */
		memset(&wr, 0, sizeof(wr));
		wr.wr_id = wrid;
		wr.next = NULL;
		wr.sg_list = &list;
		wr.num_sge = 1;
		wr.exp_opcode = opcode;
		wr.exp_send_flags = IBV_EXP_SEND_SIGNALED;
		wr.ex.imm_data = 0;
#if defined(IBV_TEST_UD_ENABLED) && (IBV_TEST_UD_ENABLED == 1)
		wr.wr.ud.ah = ctx->ah;
		wr.wr.ud.remote_qpn  = ctx->peer_info.qpn;
		wr.wr.ud.remote_qkey = 0x11111111;
#endif
		switch (opcode) {
		case IBV_EXP_WR_RDMA_WRITE:
		case IBV_EXP_WR_RDMA_READ:
			wr.wr.rdma.remote_addr = ctx->peer_info.vaddr;
			wr.wr.rdma.rkey = ctx->peer_info.rkey;
			break;

		case IBV_EXP_WR_RECV_ENABLE:
		case IBV_EXP_WR_SEND_ENABLE:

			wr.task.wqe_enable.qp = ctx->qp;
			wr.task.wqe_enable.wqe_count = 0;

			wr.exp_send_flags |=  IBV_EXP_SEND_WAIT_EN_LAST;
			break;

		case IBV_EXP_WR_CQE_WAIT:

			wr.task.cqe_wait.cq = ctx->scq;
			wr.task.cqe_wait.cq_count = 1;

			wr.exp_send_flags |= IBV_EXP_SEND_WAIT_EN_LAST;
			break;

		case IBV_EXP_WR_CALC_SEND:

			wr.op.calc.calc_op   = IBV_EXP_CALC_OP_ADD;
			wr.op.calc.data_type = IBV_EXP_CALC_DATA_TYPE_INT;
			wr.op.calc.data_size = IBV_EXP_CALC_DATA_SIZE_64_BIT;

			wr.exp_opcode  = IBV_EXP_WR_SEND;
			wr.exp_send_flags |= IBV_EXP_SEND_WITH_CALC;

			break;

		case IBV_WR_SEND:
			break;

		default:
			VERBS_TRACE("Invalid opcode: %d.\n", wr.exp_opcode);
			rc = EINVAL;
			break;
		}

		rc = ibv_exp_post_send(ctx->qp, &wr, &bad_wr);

		return rc;
	}

	int __post_read(struct test_context *ctx, int count)
	{
		int rc = EOK;
		struct ibv_sge list;
		struct ibv_recv_wr wr;
		struct ibv_recv_wr *bad_wr;
		int i = 0;

		/* prepare the scatter/gather entry */
		memset(&list, 0, sizeof(list));
		list.addr = (uintptr_t)ctx->net_buf;
		list.length = ctx->size;
		list.lkey = ctx->mr->lkey;

		/* prepare the send work request */
		memset(&wr, 0, sizeof(wr));
		wr.wr_id = TEST_SET_WRID(TEST_RECV_WRID, 0);
		wr.next = NULL;
		wr.sg_list = &list;
		wr.num_sge = 1;

		for (i = 0; i < count; i++)
			if ((rc = ibv_post_recv(ctx->qp, &wr, &bad_wr)) != EOK) {
				EXPECT_EQ(EOK, rc);
				EXPECT_EQ(EOK, errno);
				break;
			}

		return i;
	}

	int __query_calc_cap(struct ibv_context *context,
			 enum pp_wr_calc_op calc_op,
			 enum pp_wr_data_type data_type,
			 int *operands_per_gather,
			 int *max_num_operands)
	{
		struct pingpong_calc_ctx *calc_ctx = &ctx->calc_op;

		/* TODO: check using pp_query_device() should be added */
		if (calc_ctx->init_opcode == PP_CALC_PROD) {
			return EPERM;
		}

		if ((calc_ctx->init_opcode == PP_CALC_LXOR ||
		     calc_ctx->init_opcode == PP_CALC_BXOR ||
		     calc_ctx->init_opcode == PP_CALC_LOR ||
		     calc_ctx->init_opcode == PP_CALC_BOR ||
		     calc_ctx->init_opcode == PP_CALC_LAND ||
		     calc_ctx->init_opcode == PP_CALC_BAND) &&
				(calc_ctx->init_data_type == PP_DATA_TYPE_FLOAT32 ||
				 calc_ctx->init_data_type == PP_DATA_TYPE_FLOAT64)) {
			return EPERM;
		}

		/* There issues with comparing two vales when one is positive the other is negative */
		if (calc_ctx->init_opcode == PP_CALC_MIN ||
		    calc_ctx->init_opcode == PP_CALC_MINLOC ||
		    calc_ctx->init_opcode == PP_CALC_MAX ||
		    calc_ctx->init_opcode == PP_CALC_MAXLOC) {
			return EPERM;
		}

		if (operands_per_gather)
			*operands_per_gather = 1;

		if (max_num_operands)
			*max_num_operands = 2;

		return EOK;
	}

	int __create_data_for_calc(const char *str,
			enum pp_wr_calc_op calc_op,
			enum pp_wr_data_type data_type)
	{
		struct pingpong_calc_ctx *calc_ctx = &ctx->calc_op;
		char *ops_str = NULL;
		int op_per_gather, max_num_op;
		struct calc_pack_input params;
		int i, num_operands;
		int sz;
		char *__gather_token, *__saveptr, *__err_ptr = NULL;
		int rc = 0;

		calc_ctx->init_opcode = calc_op;
		calc_ctx->init_data_type = data_type;
		calc_ctx->opcode = IBV_EXP_CALC_OP_NUMBER;
		calc_ctx->data_type = IBV_EXP_CALC_DATA_TYPE_NUMBER;
		calc_ctx->data_size = IBV_EXP_CALC_DATA_SIZE_NUMBER;


		rc = __query_calc_cap(ctx->context, calc_ctx->init_opcode,
				calc_ctx->init_data_type, &op_per_gather, &max_num_op);
		if (rc) {
			VERBS_TRACE("-E- TODO: Unit-test does not support operation (opcode=%d datatype=%d) on %s.\n",
				calc_ctx->init_opcode,
				calc_ctx->init_data_type,
				ibv_get_device_name(ibv_dev));
			return rc;
		}

		ctx->str_input = str;
		ops_str = strdup(ctx->str_input);
		if (!ops_str) {
			VERBS_TRACE("You must choose an operation to perform.\n");
			return -1;
		}

		for (i = 0, num_operands = 1; i < (int)strlen(ops_str); i++) {
			if (ops_str[i] == ',')
				num_operands++;
		}

		calc_ctx->gather_list_size = num_operands;

		__gather_token = strtok_r(ops_str, ",", &__saveptr);
		if (!__gather_token)
			return -1;

		/* Build the gather list, assume one operand per sge. todo: improve for any nr of operands */
		for (i = 0; i < num_operands; i++) {
			/* copy the operands to the buffer */
			switch (calc_ctx->init_data_type) {
			case PP_DATA_TYPE_INT8:
			case PP_DATA_TYPE_UINT8:
				*((int8_t *)ctx->buf + i*16) = strtol(__gather_token, &__err_ptr, 0);
				break;

			case PP_DATA_TYPE_INT16:
			case PP_DATA_TYPE_UINT16:
				return EPERM;

			case PP_DATA_TYPE_INT32:
			case PP_DATA_TYPE_UINT32:
				*((int32_t *)ctx->buf + i*4) = strtol(__gather_token, &__err_ptr, 0);
				break;

			case PP_DATA_TYPE_INT64:
			case PP_DATA_TYPE_UINT64:
				*((int64_t *)ctx->buf + i*2) = strtoll(__gather_token, &__err_ptr, 0);
				break;

			case PP_DATA_TYPE_FLOAT32:
				*((float *)ctx->buf + i*4) = strtof(__gather_token, &__err_ptr);
				break;

			case PP_DATA_TYPE_FLOAT64:
				*((double *)ctx->buf + i*2) = strtof(__gather_token, &__err_ptr);
				break;

			default:
				return -1;
			}

			memset(&params, 0, sizeof(params));
			params.op = calc_ctx->init_opcode;
			params.type = calc_ctx->init_data_type;
			params.host_buf = (int64_t *) ctx->buf + i * 2;
			params.id = i;
			params.out_op = &calc_ctx->opcode;
			params.out_type = &calc_ctx->data_type;
			params.out_size = &calc_ctx->data_size;
			params.net_buf = (uint64_t *) ctx->net_buf + i * 2;

			if (__pack_data_for_calc(ctx->context, &params)) {
				VERBS_TRACE("Error in pack \n");
				return -1;
			}
			__gather_token = strtok_r(NULL, ",", &__saveptr);
			if (!__gather_token)
				break;

		}

		calc_ctx->gather_buff = ctx->net_buf;

		/* Data size is based on datatype returned from pack
		 * Note: INT16, INT32, INT64 -> INT64 (sz=8)
		 */
		sz = -1;
		sz = __calc_data_size_to_bytes(calc_ctx->data_size);

		if (num_operands < 0) {
			VERBS_TRACE("-E- failed parsing calc operators\n");
			return rc;
		}
		else {
			int num_sge;
			int i, gather_ix;
			struct ibv_sge *gather_list = NULL;

			num_sge = (num_operands / op_per_gather) + ((num_operands % op_per_gather) ? 1 : 0);

			gather_list = (struct ibv_sge*)calloc(num_sge, sizeof(*gather_list));
			if (!gather_list) {
				VERBS_TRACE("Failed to allocate %Zu bytes for gather_list\n",
						(num_sge * sizeof(*gather_list)));
				return -1;
			}

			/* Build the gather list */
			for (i = 0, gather_ix = 0; i < num_operands; i++) {
				if (!(i % op_per_gather)) {
					gather_list[gather_ix].addr   = (uint64_t)ctx->net_buf + ((sz + 8) * i);
					gather_list[gather_ix].length = (sz + 8) * op_per_gather;
					gather_list[gather_ix].lkey   = ctx->mr->lkey;

					gather_ix++;
				}
			}

			calc_ctx->gather_list = gather_list;
		}

		if (ops_str)
			free(ops_str);

		return rc;
	}
};
