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

class tc_verbs_post_send_wait : public verbs_test_cd {};

#define SEND_POST_COUNT		10

/* tc_verbs_post_send: [TI.1]
 * IBV_WR_CQE_WAIT
 * should return EINVAL error code for usual QP that does not
 * support Cross-Channel IO Operations
 */
TEST_F(tc_verbs_post_send_wait, ti_1) {

	int rc = EOK;

	__init_test();

	rc = __post_write(ctx, 0, IBV_EXP_WR_CQE_WAIT);
	EXPECT_NE(rc, EOK);
	EXPECT_TRUE((EINVAL == rc) || (EINVAL == errno));
}


/* tc_verbs_post_send: [TI.2]
 * IBV_WR_CQE_WAIT is posted to QP created with IBV_EXP_QP_CREATE_CROSS_CHANNEL
 * cqe_wait.cq_count < send request posted
 * Expected ONE CQE on mcq
 */
TEST_F(tc_verbs_post_send_wait, ti_2) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid == 0)
			{
				struct ibv_exp_send_wr wr;

				wr.wr_id = wrid;
				wr.next = NULL;
				wr.sg_list = NULL;
				wr.num_sge = 0;
				wr.exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr.exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
				wr.ex.imm_data = 0;

				wr.task.cqe_wait.cq = ctx->rcq;
				wr.task.cqe_wait.cq_count = 1;

				rc = ibv_exp_post_send(ctx->mqp, &wr, NULL);
				ASSERT_EQ(EOK, rc);
			}

			rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
			ASSERT_EQ(EOK, rc);
			++wrid;

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				&& ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		/* we need to be sure that all work request have been posted */
		sleep(2);

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		EXPECT_EQ(1, m_poll_cq_count);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
	}
}


/* tc_verbs_post_send: [TI.3]
 * IBV_WR_CQE_WAIT is posted to QP created with IBV_EXP_QP_CREATE_CROSS_CHANNEL
 * cqe_wait.cq_count > send request posted
 * Expected NONE CQE on mcq
 */
TEST_F(tc_verbs_post_send_wait, ti_3) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid == 0)
			{
				struct ibv_exp_send_wr wr;

				wr.wr_id = wrid;
				wr.next = NULL;
				wr.sg_list = NULL;
				wr.num_sge = 0;
				wr.exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr.exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
				wr.ex.imm_data = 0;

				wr.task.cqe_wait.cq = ctx->rcq;
				wr.task.cqe_wait.cq_count = (SEND_POST_COUNT + 1);

				rc = ibv_exp_post_send(ctx->mqp, &wr, NULL);
				ASSERT_EQ(EOK, rc);
			}

			rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
			ASSERT_EQ(EOK, rc);
			++wrid;

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				&& ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		/* we need to be sure that all work request have been posted */
		sleep(2);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);
		EXPECT_EQ(0, m_poll_cq_count);
	}
}


/* tc_verbs_post_send: [TI.4]
 * IBV_WR_CQE_WAIT is posted to QP created with IBV_EXP_QP_CREATE_CROSS_CHANNEL
 * cqe_wait.cq_count == send request posted
 * Expected ONE CQE on mcq
 */
TEST_F(tc_verbs_post_send_wait, ti_4) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid == 0)
			{
				struct ibv_exp_send_wr wr;

				wr.wr_id = wrid;
				wr.next = NULL;
				wr.sg_list = NULL;
				wr.num_sge = 0;
				wr.exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr.exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
				wr.ex.imm_data = 0;

				wr.task.cqe_wait.cq = ctx->rcq;
				wr.task.cqe_wait.cq_count = SEND_POST_COUNT;

				rc = ibv_exp_post_send(ctx->mqp, &wr, NULL);
				ASSERT_EQ(EOK, rc);
			}

			poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);
			m_poll_cq_count += poll_result;

			/* completion should not be raised */
			EXPECT_EQ(0, m_poll_cq_count);

			rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
			ASSERT_EQ(EOK, rc);
			++wrid;

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				&& ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		/* we need to be sure that all work request have been posted */
		sleep(2);

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		EXPECT_EQ(1, m_poll_cq_count);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
	}
}


/* tc_verbs_post_send: [TI.5]
 * Three IBV_WR_CQE_WAIT are posted to QP created with IBV_EXP_QP_CREATE_CROSS_CHANNEL
 * cqe_wait.cq_count == send request posted
 * Expected THREE CQE on mcq
 * Note: every WAIT generates CQE
 */
TEST_F(tc_verbs_post_send_wait, ti_5) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid == 0)
			{
				struct ibv_exp_send_wr wr[3];

				/* clean up WCs */
				memset(ctx->wc, 0, 3 * sizeof(struct ibv_wc));

				wr[0].wr_id = 3;
				wr[0].next = &wr[1];
				wr[0].sg_list = NULL;
				wr[0].num_sge = 0;
				wr[0].exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr[0].exp_send_flags = IBV_EXP_SEND_SIGNALED;
				wr[0].ex.imm_data = 0;

				wr[1].wr_id = 2;
				wr[1].next = &wr[2];
				wr[1].sg_list = NULL;
				wr[1].num_sge = 0;
				wr[1].exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr[1].exp_send_flags = IBV_EXP_SEND_SIGNALED;
				wr[1].ex.imm_data = 0;

				wr[2].wr_id = 1;
				wr[2].next = NULL;
				wr[2].sg_list = NULL;
				wr[2].num_sge = 0;
				wr[2].exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr[2].exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
				wr[2].ex.imm_data = 0;

				wr[0].task.cqe_wait.cq = ctx->rcq;
				wr[0].task.cqe_wait.cq_count = SEND_POST_COUNT;
				wr[1].task.cqe_wait.cq = ctx->rcq;
				wr[1].task.cqe_wait.cq_count = SEND_POST_COUNT;
				wr[2].task.cqe_wait.cq = ctx->rcq;
				wr[2].task.cqe_wait.cq_count = SEND_POST_COUNT;

				rc = ibv_exp_post_send(ctx->mqp, wr, NULL);
				ASSERT_EQ(EOK, rc);
			}

			poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);
			m_poll_cq_count += poll_result;

			/* completion should not be raised */
			EXPECT_EQ(0, m_poll_cq_count);

			rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
			ASSERT_EQ(EOK, rc);
			++wrid;

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				&& ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		/* we need to be sure that all work request have been posted */
		sleep(2);

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		EXPECT_EQ(3, m_poll_cq_count);
		EXPECT_EQ((uint64_t)3, ctx->wc[0].wr_id);
		EXPECT_EQ(ctx->mqp->qp_num, ctx->wc[0].qp_num);
		EXPECT_EQ((uint64_t)2, ctx->wc[1].wr_id);
		EXPECT_EQ(ctx->mqp->qp_num, ctx->wc[1].qp_num);
		EXPECT_EQ((uint64_t)1, ctx->wc[2].wr_id);
		EXPECT_EQ(ctx->mqp->qp_num, ctx->wc[2].qp_num);
	}
}


/* tc_verbs_post_send: [TI.6]
 * Three IBV_WR_CQE_WAIT are posted to QP created with IBV_EXP_QP_CREATE_CROSS_CHANNEL
 * cqe_wait(1).cq_count == SEND_POST_COUNT - 1
 * cqe_wait(2).cq_count == SEND_POST_COUNT
 * cqe_wait(3).cq_count == SEND_POST_COUNT + 1
 * Expected TWO CQE on mcq
 * Note: internal index is relative previous post and incremented after special WAIT posted
 */
TEST_F(tc_verbs_post_send_wait, ti_6) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid == 0)
			{
				struct ibv_exp_send_wr wr[3];

				/* clean up WCs */
				memset(ctx->wc, 0, 3 * sizeof(struct ibv_wc));

				wr[0].wr_id = 3;
				wr[0].next = &wr[1];
				wr[0].sg_list = NULL;
				wr[0].num_sge = 0;
				wr[0].exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr[0].exp_send_flags = IBV_EXP_SEND_SIGNALED;
				wr[0].ex.imm_data = 0;

				wr[0].task.cqe_wait.cq = ctx->rcq;
				wr[0].task.cqe_wait.cq_count = SEND_POST_COUNT - 1;

				wr[1].wr_id = 2;
				wr[1].next = &wr[2];
				wr[1].sg_list = NULL;
				wr[1].num_sge = 0;
				wr[1].exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr[1].exp_send_flags = IBV_EXP_SEND_SIGNALED;
				wr[1].ex.imm_data = 0;

				wr[1].task.cqe_wait.cq = ctx->rcq;
				wr[1].task.cqe_wait.cq_count = SEND_POST_COUNT;

				wr[2].wr_id = 1;
				wr[2].next = NULL;
				wr[2].sg_list = NULL;
				wr[2].num_sge = 0;
				wr[2].exp_opcode = IBV_EXP_WR_CQE_WAIT;
				wr[2].exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
				wr[2].ex.imm_data = 0;

				wr[2].task.cqe_wait.cq = ctx->rcq;
				wr[2].task.cqe_wait.cq_count = SEND_POST_COUNT + 1;

				rc = ibv_exp_post_send(ctx->mqp, wr, NULL);
				ASSERT_EQ(EOK, rc);
			}

			poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);
			m_poll_cq_count += poll_result;

			/* completion should not be raised */
			EXPECT_EQ(0, m_poll_cq_count);

			rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
			ASSERT_EQ(EOK, rc);
			++wrid;

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				&& ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		/* we need to be sure that all work request have been posted */
		sleep(2);

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		EXPECT_EQ(2, m_poll_cq_count);
		EXPECT_EQ((uint64_t)3, ctx->wc[0].wr_id);
		EXPECT_EQ(ctx->mqp->qp_num, ctx->wc[0].qp_num);
		EXPECT_EQ((uint64_t)2, ctx->wc[1].wr_id);
		EXPECT_EQ(ctx->mqp->qp_num, ctx->wc[1].qp_num);
	}
}


/* tc_verbs_post_send: [TI.7]
 * Set cq_count that exceeds SCQ deep
 */
TEST_F(tc_verbs_post_send_wait, ti_7) {

	int rc = EOK;
	int poll_result;
	int64_t	 wrid = 0;

	__init_test( 0, 0x1F, 0x1F,
		     IBV_EXP_CQ_IGNORE_OVERRUN, 0x0F,
		     0, 0x1F);
	ASSERT_EQ(0x1F, ctx->qp_tx_depth);
	ASSERT_EQ(0x0F, ctx->cq_tx_depth);

	{
		struct ibv_exp_send_wr wr;

		wr.wr_id = 777;
		wr.next = NULL;
		wr.sg_list = NULL;
		wr.num_sge = 0;
		wr.exp_opcode = IBV_EXP_WR_CQE_WAIT;
		wr.exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
		wr.ex.imm_data = 0;

		wr.task.cqe_wait.cq = ctx->rcq;
		wr.task.cqe_wait.cq_count = ctx->cq_tx_depth + 3;

		rc = ibv_exp_post_send(ctx->mqp, &wr, NULL);
		ASSERT_EQ(EOK, rc);
	}

	/*
	 * Post number of WRs that
	 * equal maximum number of CQE in SCQ
	 */
	{
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;

		/* Post number of WRs that exceeds maximum of CQE in CQ */
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
			ASSERT_EQ(EOK, rc);
			++wrid;

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < (ctx->cq_tx_depth + 2))
				&& ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));
	}

	sleep(2);

	poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth + SEND_POST_COUNT, ctx->wc);
	EXPECT_EQ(0, poll_result);

	poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth + SEND_POST_COUNT, ctx->wc);
	EXPECT_EQ(ctx->cq_tx_depth + 2, poll_result);

	poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
	EXPECT_EQ(0, poll_result);

	/*
	 * Post number of WRs that
	 * greater than Maximum number of CQE in SCQ
	 */
	{
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;

		/* Post number of WRs that exceeds maximum of CQE in CQ */
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
			ASSERT_EQ(EOK, rc);
			++wrid;

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < (ctx->cq_tx_depth + SEND_POST_COUNT))
				&& ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));
	}

	sleep(2);

	poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth + SEND_POST_COUNT, ctx->wc);
	/* After exceeding maximum numaber of CQE in SCQ ibv_poll_cq() returns 0 */
	EXPECT_EQ(0, poll_result);

	poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth + SEND_POST_COUNT, ctx->wc);
	EXPECT_EQ(SEND_POST_COUNT - 2, poll_result);

	poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
	EXPECT_EQ(1, poll_result);
	EXPECT_EQ((uint64_t)777, ctx->wc[0].wr_id);
	EXPECT_EQ(ctx->mqp->qp_num, ctx->wc[0].qp_num);
}
