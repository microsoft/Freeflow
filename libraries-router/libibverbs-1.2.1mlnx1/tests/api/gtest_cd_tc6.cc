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
#include "gtest_cd.h"
#include "gtest/gtest.h"

class tc_verbs_post_send_calc : public verbs_test_cd {};

#define ITER_NUM	5

static int pp_test(struct test_context *ctx);


/* tc_verbs_post_send: [TI.1]
 * IBV_WR_CALC_SEND (LXOR)
 */
TEST_F(tc_verbs_post_send_calc, ti_1) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_LXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_LXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_LXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_LXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_LXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_LXOR, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_LXOR, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_LXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_LXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_LXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_LXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_LXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_LXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_LXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_LXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_LXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_LXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_LXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_LXOR, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0.0, 0.0", PP_CALC_LXOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_LXOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345678.9087", PP_CALC_LXOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-12345678.9087, 12345678.9087", PP_CALC_LXOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-12345678.9087, -12345678.9087", PP_CALC_LXOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.2]
 * IBV_WR_CALC_SEND (BXOR)
 */
TEST_F(tc_verbs_post_send_calc, ti_2) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_BXOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_BXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_BXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_BXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_BXOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_BXOR, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_BXOR, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_BXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_BXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_BXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_BXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_BXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_BXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_BXOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_BXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_BXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_BXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_BXOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_BXOR, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_BXOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.3]
 * IBV_WR_CALC_SEND (LOR)
 */
TEST_F(tc_verbs_post_send_calc, ti_3) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_LOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_LOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_LOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_LOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_LOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_LOR, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_LOR, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_LOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_LOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_LOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_LOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_LOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_LOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_LOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_LOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_LOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_LOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_LOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_LOR, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_LOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.4]
 * IBV_WR_CALC_SEND (BOR)
 */
TEST_F(tc_verbs_post_send_calc, ti_4) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_BOR, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_BOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_BOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_BOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_BOR, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_BOR, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_BOR, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_BOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_BOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_BOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_BOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_BOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_BOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_BOR, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_BOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_BOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_BOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_BOR, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_BOR, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_BOR, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.5]
 * IBV_WR_CALC_SEND (LAND)
 */
TEST_F(tc_verbs_post_send_calc, ti_5) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_LAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_LAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_LAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_LAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_LAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_LAND, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_LAND, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_LAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_LAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_LAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_LAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_LAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_LAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_LAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_LAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_LAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_LAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_LAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_LAND, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_LAND, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.6]
 * IBV_WR_CALC_SEND (BAND)
 */
TEST_F(tc_verbs_post_send_calc, ti_6) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_BAND, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_BAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_BAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_BAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_BAND, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_BAND, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_BAND, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_BAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_BAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_BAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_BAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_BAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_BAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_BAND, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_BAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_BAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_BAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_BAND, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_BAND, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_BAND, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.7]
 * IBV_WR_CALC_SEND (ADD)
 */
TEST_F(tc_verbs_post_send_calc, ti_7) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_ADD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_ADD, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_ADD, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_ADD, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_ADD, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_ADD, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_ADD, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_ADD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_ADD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_ADD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_ADD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_ADD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_ADD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_ADD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_ADD, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_ADD, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_ADD, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_ADD, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.987, 1234.567", PP_CALC_ADD, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_ADD, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.8]
 * IBV_WR_CALC_SEND (MAX)
 */
TEST_F(tc_verbs_post_send_calc, ti_8) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_MAX, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_MAX, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_MAX, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_MAX, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_MAX, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_MAX, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_MAX, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_MAX, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_MAX, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_MAX, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_MAX, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_MAX, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_MAX, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_MAX, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_MAX, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_MAX, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_MAX, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_MAX, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_MAX, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_MAX, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.9]
 * IBV_WR_CALC_SEND (MIN)
 */
TEST_F(tc_verbs_post_send_calc, ti_9) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0, 0", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 5", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("5, 0", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, -5", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, 0", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-5, -5", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, 5", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, 5", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("3, -5", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-3, -5", PP_CALC_MIN, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0x80", PP_CALC_MIN, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x80, 0", PP_CALC_MIN, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF0, 0x01", PP_CALC_MIN, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE0, 0xF0", PP_CALC_MIN, PP_DATA_TYPE_UINT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_MIN, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_MIN, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0, 0", PP_CALC_MIN, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 0", PP_CALC_MIN, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 0", PP_CALC_MIN, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_MIN, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 1234567890", PP_CALC_MIN, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, 1234567890", PP_CALC_MIN, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("-1234567890, -1234567890", PP_CALC_MIN, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x00, 0x8000000000000000", PP_CALC_MIN, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8000000000000000, 0x00", PP_CALC_MIN, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xF000000000000000, 0x01", PP_CALC_MIN, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0xE000000000000000, 0xF000000000000000", PP_CALC_MIN, PP_DATA_TYPE_UINT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_MIN, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_MIN, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

/* tc_verbs_post_send: [TI.10]
 * IBV_WR_CALC_SEND (PROD)
 */
TEST_F(tc_verbs_post_send_calc, ti_10) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	rc = __create_data_for_calc("0x07, 0xF2", PP_CALC_PROD, PP_DATA_TYPE_INT8);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("0x8007, 0x07FF", PP_CALC_PROD, PP_DATA_TYPE_INT16);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567, 54321", PP_CALC_PROD, PP_DATA_TYPE_INT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("1234567890, 54321", PP_CALC_PROD, PP_DATA_TYPE_INT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345.0987, 1234.567", PP_CALC_PROD, PP_DATA_TYPE_FLOAT32);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);

	rc = __create_data_for_calc("12345678.9087, 12345.5678", PP_CALC_PROD, PP_DATA_TYPE_FLOAT64);
	rc = (rc == EOK ? pp_test(ctx) : (rc == EPERM ? EOK : rc));
	EXPECT_EQ(EOK, rc);
}

static int pp_post_send(struct test_context *ctx)
{
	int ret;
	struct ibv_sge list;
	struct ibv_exp_send_wr wr;
	struct ibv_exp_send_wr *bad_wr;

	memset(&list, 0, sizeof(list));
	list.addr = (uintptr_t)ctx->net_buf;
	list.length = ctx->size;
	list.lkey = ctx->mr->lkey;

	/* prepare the send work request */
	memset(&wr, 0, sizeof(wr));
	wr.wr_id = TEST_SEND_WRID;
	wr.next = NULL;
	wr.sg_list = &list;
	wr.num_sge = 1;
	wr.exp_opcode = IBV_EXP_WR_SEND;
	wr.exp_send_flags = IBV_EXP_SEND_SIGNALED;
	wr.ex.imm_data = 0;

	VERBS_TRACE("outcome data:\n");
	sys_hexdump(ctx->net_buf, 32);

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

static int pp_post_read(struct test_context *ctx, int count)
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
	wr.wr_id = TEST_RECV_WRID;
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

static int pp_test(struct test_context *ctx)
{
	struct timeval		start, end;
	int			iters = ITER_NUM;
	int			rcnt, scnt;
	int			verbose = 1;
	int			verify = 1;
	unsigned long 		start_time_msec;
	unsigned long 		cur_time_msec;
	struct timeval 		cur_time;

	struct calc_unpack_input params;

	enum 		pp_wr_data_type	calc_data_type = ctx->calc_op.init_data_type;
	enum 		pp_wr_calc_op	calc_opcode = ctx->calc_op.init_opcode;
	struct 		ibv_wc wc[2];
	int 		ne, i;

	srand48(getpid() * time(NULL));

	memset(&params, 0, sizeof(params));

	memcpy(ctx->last_result, ctx->buf, sizeof(uint64_t));

	ctx->pending = TEST_RECV_WRID;

	EXPECT_LT(iters, ctx->qp_rx_depth);

	/*if (servername) */{
		if (pp_post_send(ctx)) {
			VERBS_TRACE("Couldn't post send\n");
			return 1;
		}
		ctx->pending |= TEST_SEND_WRID;

	}

	if (gettimeofday(&start, NULL)) {
		perror("gettimeofday");
		return 1;
	}

	rcnt = scnt = 0;
	while (rcnt < iters || scnt < iters) {
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			ne = ibv_poll_cq(ctx->scq, 1, wc);
			if (ne < 0) {
				VERBS_TRACE("poll CQ failed %d\n", ne);
				return 1;
			}

			ne += ibv_poll_cq(ctx->rcq, 1, (wc + ne));
			if (ne < 0) {
				VERBS_TRACE("poll CQ failed %d\n", ne);
				return 1;
			}
			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((ne < 1) && ((cur_time_msec - start_time_msec)
				< MAX_POLL_CQ_TIMEOUT));

		EXPECT_TRUE(ne > 0);
		if (ne < 1)
			return 1;

		for (i = 0; i < ne; ++i) {
			if (wc[i].status != IBV_WC_SUCCESS) {
				VERBS_TRACE("Failed %s status %s (%d v:%d) for count %d\n",
					wr_id2str(wc[i].wr_id),
					ibv_wc_status_str(wc[i].status), wc[i].status, wc[i].vendor_err,
					(int)(wc[i].wr_id == TEST_SEND_WRID ? scnt : rcnt));
				return 1;
			}

			switch ((int)wc[i].wr_id) {
			case TEST_SEND_WRID:
				++scnt;
				break;

			case TEST_RECV_WRID:
				params.op = calc_opcode;
				params.type = calc_data_type;
				params.net_buf = ctx->net_buf;
				params.id = NULL;
				params.host_buf = ctx->buf;

				VERBS_TRACE("income data:\n");
				sys_hexdump(ctx->net_buf, 16);

				if (__unpack_data_from_calc(ctx->context, &params)) {
					VERBS_TRACE("Error in unpack \n");
				}
				if (verbose) {
					VERBS_INFO("IN: type: %-8s op: %-8s\n",
							__data_type_to_str(calc_data_type),
							__calc_op_to_str(calc_opcode));
					switch (calc_data_type) {
					case PP_DATA_TYPE_INT8:
						VERBS_INFO("IN: data: %d, %d\n",
								*(int8_t *)ctx->last_result, *((int8_t *)ctx->buf + 16));
						VERBS_INFO("OUT: %d\n",
								*(int8_t *)ctx->buf);
						break;

					case PP_DATA_TYPE_UINT8:
						VERBS_INFO("IN: data: %u, %u\n",
								*(uint8_t *)ctx->last_result, *((uint8_t *)ctx->buf + 16));
						VERBS_INFO("OUT: %u\n",
								*(uint8_t *)ctx->buf);
						break;

					case PP_DATA_TYPE_INT32:
						VERBS_INFO("IN: data: %d, %d\n",
								*(int32_t *)ctx->last_result, *((int32_t *)ctx->buf + 4));
						VERBS_INFO("OUT: %d\n",
								*(int32_t *)ctx->buf);
						break;

					case PP_DATA_TYPE_UINT32:
						VERBS_INFO("IN: data: %u, %u\n",
								*(uint32_t *)ctx->last_result, *((uint32_t *)ctx->buf + 4));
						VERBS_INFO("OUT: %u\n",
								*(uint32_t *)ctx->buf);
						break;

					case PP_DATA_TYPE_INT64:
						VERBS_INFO("IN: data: %ld, %ld\n",
								*(int64_t *)ctx->last_result, *((int64_t *)ctx->buf + 2));
						VERBS_INFO("OUT: %ld\n",
								*(int64_t *)ctx->buf);
						break;

					case PP_DATA_TYPE_UINT64:
						VERBS_INFO("IN: data: %lu, %lu\n",
								*(uint64_t *)ctx->last_result, *((uint64_t *)ctx->buf + 2));
						VERBS_INFO("OUT: %lu\n",
								*(uint64_t *)ctx->buf);
						break;

					case PP_DATA_TYPE_FLOAT32:
						VERBS_INFO("IN: data: %f, %f\n",
								*(float*)ctx->last_result, *((float *)ctx->buf + 4));
						VERBS_INFO("OUT: %f\n",
								*(float *)ctx->buf);
						break;

					case PP_DATA_TYPE_FLOAT64:
						VERBS_INFO("IN: data: %lf, %lf\n",
								*(FLOAT64*)ctx->last_result, *((FLOAT64 *)ctx->buf + 2));
						VERBS_INFO("OUT: %lf\n",
								*(FLOAT64 *)ctx->buf);
						break;

					default:
						VERBS_INFO("IN: data: %ld, %ld\n",
								*(int64_t *)ctx->last_result, *((int64_t *)ctx->buf + 2));
						VERBS_INFO("OUT: 0x%016lx\n",
								*(int64_t *)ctx->buf);
					}
				}
				if (verify) {
					if (calc_verify(ctx, calc_data_type, calc_opcode)) {
						VERBS_TRACE("Calc verification failed result expected: 0x%016lx\n",
								(int64_t)calc_display(ctx, calc_data_type, calc_opcode));
						return 1;
					}
				}
				update_last_result(ctx, calc_data_type, calc_opcode);

				++rcnt;
				break;

			default:
				VERBS_TRACE("Completion for unknown wr_id %d\n",
					(int) wc[i].wr_id);
				return 1;
			}

			ctx->pending &= ~(int)wc[i].wr_id;
			if (scnt < iters && !ctx->pending) {
				if (pp_post_send(ctx)) {
					VERBS_TRACE("Couldn't post send\n");
					return 1;
				}
				ctx->pending = TEST_RECV_WRID | TEST_SEND_WRID;
			}
		} /* for (i = 0; i < ne; ++i) */
	} /* while (rcnt < iters || scnt < iters) */

	if (gettimeofday(&end, NULL)) {
		perror("gettimeofday");
		return 1;
	}

	/* post new WRs for next test calls */
	pp_post_read(ctx, iters);

#if 0
	{
		float usec = (end.tv_sec - start.tv_sec) * 1000000 +
			(end.tv_usec - start.tv_usec);
		long long bytes = (long long) ctx->size * iters * 2;

		VERBS_INFO("%lld bytes in %.2f seconds = %.2f Mbit/sec\n",
			   bytes, usec / 1000000., bytes * 8. / usec);
		VERBS_INFO("%d iters in %.2f seconds = %.2f usec/iter\n",
			   iters, usec / 1000000., usec / iters);
	}
#endif
	return 0;
}
