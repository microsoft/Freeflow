/*
 * Copyright (c) 2006 Cisco Systems.  All rights reserved.
 * Copyright (c) 2009-2010 Mellanox Technologies.  All rights reserved.
 */

#ifndef IBV_CC_PINGPONG_H
#define IBV_CC_PINGPONG_H

#include <stdio.h>
#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>


#define FLOAT64 double

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

static struct {
	char size;
	const char str[32];
} pp_wr_data_type_str[] = {
	[PP_DATA_TYPE_INT8]     = { .size = 1,  .str = "INT8" },
	[PP_DATA_TYPE_INT16]    = { .size = 2,  .str = "INT16"},
	[PP_DATA_TYPE_INT32]    = { .size = 4,  .str = "INT32"},
	[PP_DATA_TYPE_INT64]    = { .size = 8,  .str = "INT64"},
	[PP_DATA_TYPE_UINT8]    = { .size = 1,  .str = "UINT8" },
	[PP_DATA_TYPE_UINT16]   = { .size = 2,  .str = "UINT16"},
	[PP_DATA_TYPE_UINT32]   = { .size = 4,  .str = "UINT32"},
	[PP_DATA_TYPE_UINT64]   = { .size = 8,  .str = "UINT64"},
	[PP_DATA_TYPE_FLOAT32]  = { .size = 4,  .str = "FLOAT32"},
	[PP_DATA_TYPE_FLOAT64]  = { .size = 8,  .str = "FLOAT64"},
};

static const char pp_wr_calc_op_str[][32] = {
	[PP_CALC_LXOR]    = "XOR",
	[PP_CALC_BXOR]    = "BXOR",
	[PP_CALC_LOR]     = "LOR",
	[PP_CALC_BOR]     = "BOR",
	[PP_CALC_LAND]    = "LAND",
	[PP_CALC_BAND]    = "BAND",
	[PP_CALC_ADD]     = "ADD",
	[PP_CALC_MAX]     = "MAX",
	[PP_CALC_MIN]     = "MIN",
	[PP_CALC_MAXLOC]  = "MAXLOC",
	[PP_CALC_MINLOC]  = "MINLOC",
	[PP_CALC_PROD]    = "PROD"
};

static inline int pp_calc_data_size_to_bytes(enum ibv_exp_calc_data_size data_size)
{
	switch (data_size) {
	case IBV_EXP_CALC_DATA_SIZE_64_BIT:     return 8;
	case IBV_EXP_CALC_DATA_SIZE_NUMBER: /* fall through */
	default:		       return -1;
	}
}

static inline int pp_query_calc_cap(struct ibv_context *context,
				 enum ibv_exp_calc_op calc_op,
				 enum ibv_exp_calc_data_type data_type,
				 enum ibv_exp_calc_data_size data_size,
				 int *operands_per_gather,
				 int *max_num_operands)
{
	/* TODO: check using pp_query_device() should be added */

	if (operands_per_gather)
		*operands_per_gather = 1;

	if (max_num_operands)
		*max_num_operands = 2;

	return 0;
}

static inline void pp_print_data_type(void)
{
	int i;

	for (i = 0; i < PP_DATA_TYPE_INVALID; i++)
		printf("\t%s\n", pp_wr_data_type_str[i].str);
}

static inline const char *pp_data_type_to_str(enum pp_wr_data_type data_type)
{
	if (data_type < sizeof(pp_wr_data_type_str)/sizeof(pp_wr_data_type_str[0]))
		return pp_wr_data_type_str[data_type].str;

	return "INVALID DATA TYPE";

}

static inline int pp_data_type_to_size(enum pp_wr_data_type data_type)
{
	if (data_type < sizeof(pp_wr_data_type_str)/sizeof(pp_wr_data_type_str[0]))
		return pp_wr_data_type_str[data_type].size;

	return -1;
}

static inline enum pp_wr_data_type pp_str_to_data_type(const char *data_type_str)
{
	int i;

	for (i = 0; i < sizeof(pp_wr_data_type_str)/sizeof(pp_wr_data_type_str[0]); i++) {
		if (!strcmp(data_type_str, pp_wr_data_type_str[i].str))
			return i;
	}

	return PP_DATA_TYPE_INVALID;
}

static inline void pp_print_calc_op(void)
{
	int i;

	for (i = 0; i < PP_CALC_INVALID; i++)
		printf("\t%s\n", pp_wr_calc_op_str[i]);
}

static inline const char *pp_calc_op_to_str(enum pp_wr_calc_op calc_op)
{
	if (calc_op < sizeof(pp_wr_calc_op_str)/sizeof(pp_wr_calc_op_str[0]))
		return pp_wr_calc_op_str[calc_op];

	return "INVALID OPERATION OPCODE";

}

static inline enum pp_wr_calc_op pp_str_to_calc_op(const char *calc_op)
{
	int i;

	for (i = 0; i < sizeof(pp_wr_calc_op_str)/sizeof(pp_wr_calc_op_str[0]); i++) {
		if (!strcmp(calc_op, pp_wr_calc_op_str[i]))
			return i;
	}

	return PP_CALC_INVALID;
}

static inline void pp_print_dev_calc_ops(struct ibv_context	*context)
{
	/* TODO: check using pp_query_device() should be added */
#if 0
	int i, j, flag, supp;

	for (i = 0; i < PP_CALC_INVALID; i++) {
		flag = 0;

		for (j = 0; j < PP_DATA_TYPE_INVALID; j++) {
			supp = pp_query_calc_cap(context, i, j, NULL, NULL);

			if (!supp) {
				if (!flag) {
					printf("\t%s:\n", pp_calc_op_to_str(i));
					flag = 1;
				}

				printf("\t\t%s\n", pp_data_type_to_str(j));
			}
		}
	}
#endif
}

static inline enum ibv_mtu pp_mtu_to_enum(int mtu)
{
	switch (mtu) {
	case 256:  return IBV_MTU_256;
	case 512:  return IBV_MTU_512;
	case 1024: return IBV_MTU_1024;
	case 2048: return IBV_MTU_2048;
	case 4096: return IBV_MTU_4096;
	default:   return -1;
	}
}

static inline uint16_t pp_get_local_lid(struct ibv_context *context, int port)
{
	struct ibv_port_attr attr;

	if (ibv_query_port(context, port, &attr))
		return 0;

	return attr.lid;
}

#endif /* IBV_CC_PINGPONG_H */
