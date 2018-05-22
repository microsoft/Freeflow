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

#include "gtest_cmn.h"
#include "gtest/gtest.h"


class tc_verbs_query_device : public verbs_test {};

/* verbs_query_device: [TI.1]
 * Check if ibv_query_device() returns information about Cross-Channel support
 */
TEST_F(tc_verbs_query_device, ti_1) {

	VERBS_INFO("============================================\n");
	VERBS_INFO("DEVICE     : %s\n", ibv_get_device_name(ibv_dev));
	VERBS_INFO("           : %s\n", ibv_dev->dev_name);
	VERBS_INFO("           : %s\n", ibv_dev->dev_path);
	VERBS_INFO("           : %s\n", ibv_dev->ibdev_path);
	VERBS_INFO("============================================\n");

	ASSERT_TRUE(device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL);
	if (device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL) {
		VERBS_INFO("device_attr.exp_device_cap_flags: 0x%lx\n",
				device_attr.exp_device_cap_flags);
		VERBS_INFO("CROSS_CHANNEL               : %s \n",
				(device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL ?
						"ON" : "OFF"));
	}
}

/* verbs_query_device: [TI.2]
 * Check if verbs_query_device() returns information about Cross-Channel support
 */
TEST_F(tc_verbs_query_device, ti_2) {

	ASSERT_TRUE(device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL);
	if (device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL) {
		VERBS_INFO("device_attr.exp_device_cap_flags: 0x%lx\n",
				device_attr.exp_device_cap_flags);
		VERBS_INFO("CROSS_CHANNEL               : %s \n",
				(device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL ?
						"ON" : "OFF"));
	}
}

/* verbs_query_device: [TI.3]
 * Check information about WR(CALC) supported data types / operations
 */
TEST_F(tc_verbs_query_device, ti_3) {

	int rc = EOK;
	struct ibv_exp_device_attr device_attr;

	memset(&device_attr, 0, sizeof(device_attr));
	device_attr.comp_mask = IBV_EXP_DEVICE_ATTR_RESERVED-1;

	rc = ibv_exp_query_device(ibv_ctx, &device_attr);
	ASSERT_EQ(rc, EOK);

	ASSERT_TRUE(device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL);
	if (device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL) {
		VERBS_INFO("device_attr.exp_device_cap_flags: 0x%lx\n",
				device_attr.exp_device_cap_flags);
		VERBS_INFO("CROSS_CHANNEL               : %s \n",
				(device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL ?
						"ON" : "OFF"));
		ASSERT_TRUE(device_attr.comp_mask & IBV_EXP_DEVICE_ATTR_CALC_CAP);
		if (device_attr.comp_mask & IBV_EXP_DEVICE_ATTR_CALC_CAP) {
			VERBS_INFO("device_attr.calc_cap:\n");
			VERBS_INFO("data_types  : 0x%016lX\n", device_attr.calc_cap.data_types);
			VERBS_INFO("data_sizes  : 0x%016lX\n", device_attr.calc_cap.data_sizes);
			VERBS_INFO("int_ops     : 0x%016lX\n", device_attr.calc_cap.int_ops);
			VERBS_INFO("uint_ops    : 0x%016lX\n", device_attr.calc_cap.uint_ops);
			VERBS_INFO("fp_ops      : 0x%016lX\n", device_attr.calc_cap.fp_ops);

			ASSERT_EQ(device_attr.calc_cap.data_types,
					(uint64_t)((1ULL << IBV_EXP_CALC_DATA_TYPE_INT) |
					(1ULL << IBV_EXP_CALC_DATA_TYPE_UINT) |
					(1ULL << IBV_EXP_CALC_DATA_TYPE_FLOAT)));
			ASSERT_EQ(device_attr.calc_cap.data_sizes,
					(uint64_t)(1ULL << IBV_EXP_CALC_DATA_SIZE_64_BIT));
			ASSERT_EQ(device_attr.calc_cap.int_ops,
					(uint64_t)((1ULL << IBV_EXP_CALC_OP_ADD) |
					(1ULL << IBV_EXP_CALC_OP_BAND) |
					(1ULL << IBV_EXP_CALC_OP_BXOR) |
					(1ULL << IBV_EXP_CALC_OP_BOR)));
			ASSERT_EQ(device_attr.calc_cap.uint_ops,
					(uint64_t)((1ULL << IBV_EXP_CALC_OP_ADD) |
					(1ULL << IBV_EXP_CALC_OP_BAND) |
					(1ULL << IBV_EXP_CALC_OP_BXOR) |
					(1ULL << IBV_EXP_CALC_OP_BOR)));
			ASSERT_EQ(device_attr.calc_cap.fp_ops,
					(uint64_t)((1ULL << IBV_EXP_CALC_OP_ADD) |
					(1ULL << IBV_EXP_CALC_OP_BAND) |
					(1ULL << IBV_EXP_CALC_OP_BXOR) |
					(1ULL << IBV_EXP_CALC_OP_BOR)));
		}
	}
}
