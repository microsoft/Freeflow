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

/*
 * Test case is disabled because of issue in FW
 */

class tc_verbs_post_recv_en : public verbs_test_cd {};

#define SEND_POST_COUNT		10
#define RECV_EN_WR_ID		((uint64_t)'R')

/* tc_verbs_post_recv_en: [TI.1]
 * IBV_WR_RECV_ENABLE does not return CQE for QP created
 * w/o IBV_EXP_QP_CREATE_MANAGED_RECV
 */
TEST_F(tc_verbs_post_recv_en, ti_1) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int routs;
		int rcnt, scnt;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		rcnt = 0;
		scnt = 0;
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				rc = __post_write(ctx, wrid, IBV_EXP_WR_RECV_ENABLE);
				ASSERT_NE(EOK, rc);
				++wrid;
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(0, s_poll_cq_count);
		EXPECT_EQ(0, r_poll_cq_count);
	}
}

/* tc_verbs_post_recv_en: [TI.2]
 * IBV_WR_SEND is posted to QP created with
 * IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV
 */
TEST_F(tc_verbs_post_recv_en, ti_2) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int routs;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				rc = __post_write(ctx, wrid, IBV_EXP_WR_SEND);
				ASSERT_EQ(EOK, rc);
				++wrid;
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(0, s_poll_cq_count);
		EXPECT_EQ(0, r_poll_cq_count);
	}
}

/* tc_verbs_post_recv_en: [TI.3]
 * IBV_EXP_QP_CREATE_MANAGED_RECV does not affect to the same QP
 */
TEST_F(tc_verbs_post_recv_en, ti_3) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int routs;
		int rcnt, scnt;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		rcnt = 0;
		scnt = 0;
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				rc = __post_write(ctx, wrid, ((wrid % 2) ? IBV_EXP_WR_RECV_ENABLE : IBV_EXP_WR_SEND));
				ASSERT_EQ(EOK, rc);
				++wrid;
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(0, s_poll_cq_count);
		EXPECT_EQ(0, r_poll_cq_count);
	}
}

/* tc_verbs_post_recv_en: [TI.4]
 * IBV_EXP_QP_CREATE_MANAGED_RECV sent from MQP is able to recieve sent requests
 * sequence SEND-RECV_EN-...
 */
TEST_F(tc_verbs_post_recv_en, ti_4) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int i;
		int routs;
		int rcnt, scnt;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		rcnt = 0;
		scnt = 0;
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				if (wrid % 2) {
					struct ibv_exp_send_wr wr_en;

					wr_en.wr_id = RECV_EN_WR_ID;
					wr_en.next = NULL;
					wr_en.sg_list = NULL;
					wr_en.num_sge = 0;
					wr_en.exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en.exp_send_flags = IBV_EXP_SEND_WAIT_EN_LAST;
					wr_en.ex.imm_data = 0;

					wr_en.task.wqe_enable.qp   = ctx->qp;
					wr_en.task.wqe_enable.wqe_count = 1;

					rc = ibv_exp_post_send(ctx->mqp, &wr_en, NULL);
					EXPECT_EQ(EOK, rc);
					ASSERT_EQ(EOK, rc);
				}
				else {
					rc = __post_write(ctx, TEST_SET_WRID(TEST_SEND_WRID, wrid), IBV_EXP_WR_SEND);
					ASSERT_EQ(EOK, rc);
				}
				++wrid;
			}

			/* poll the completion for a while before giving up of doing it .. */
			poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			s_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			r_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ((SEND_POST_COUNT/2), s_poll_cq_count);
		EXPECT_EQ((SEND_POST_COUNT/2), r_poll_cq_count);
		EXPECT_EQ(scnt, rcnt);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;
		EXPECT_EQ(0, m_poll_cq_count);
	}
}

/* tc_verbs_post_recv_en: [TI.5]
 * IBV_EXP_QP_CREATE_MANAGED_RECV sent from MQP is able to push send requests
 * sequence SEND-...-SEND-RECV_EN(all)
 */
TEST_F(tc_verbs_post_recv_en, ti_5) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int i;
		int routs;
		int rcnt, scnt;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		rcnt = 0;
		scnt = 0;
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				rc = __post_write(ctx, TEST_SET_WRID(TEST_SEND_WRID, wrid), IBV_EXP_WR_SEND);
				EXPECT_EQ(EOK, rc);

				if (wrid == (SEND_POST_COUNT - 1)) {
					struct ibv_exp_send_wr wr_en;

					wr_en.wr_id = RECV_EN_WR_ID;
					wr_en.next = NULL;
					wr_en.sg_list = NULL;
					wr_en.num_sge = 0;
					wr_en.exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en.exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
					wr_en.ex.imm_data = 0;

					wr_en.task.wqe_enable.qp   = ctx->qp;
					wr_en.task.wqe_enable.wqe_count = 0;

					rc = ibv_exp_post_send(ctx->mqp, &wr_en, NULL);
					EXPECT_EQ(EOK, rc);
					ASSERT_EQ(EOK, rc);
				}
				++wrid;
			}

			/* poll the completion for a while before giving up of doing it .. */
			poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			s_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			r_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);
		EXPECT_EQ(scnt, rcnt);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		EXPECT_EQ(1, m_poll_cq_count);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
		EXPECT_EQ(RECV_EN_WR_ID, ctx->wc[0].wr_id);
	}
}


/* tc_verbs_post_recv_en: [TI.6]
 * IBV_EXP_QP_CREATE_MANAGED_SEND sent from MQP is able to push send requests
 * sequence SEND-...-SEND-SEND_EN(3)
 * Expected THREE CQE on rcq
 */
TEST_F(tc_verbs_post_recv_en, ti_6) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int i;
		int routs;
		int rcnt, scnt;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		rcnt = 0;
		scnt = 0;
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				rc = __post_write(ctx, TEST_SET_WRID(TEST_SEND_WRID, wrid), IBV_EXP_WR_SEND);
				ASSERT_EQ(EOK, rc);

				if (wrid == (SEND_POST_COUNT - 1)) {
					struct ibv_exp_send_wr wr_en;

					wr_en.wr_id = RECV_EN_WR_ID;
					wr_en.next = NULL;
					wr_en.sg_list = NULL;
					wr_en.num_sge = 0;
					wr_en.exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en.exp_send_flags = IBV_EXP_SEND_WAIT_EN_LAST;
					wr_en.ex.imm_data = 0;

					wr_en.task.wqe_enable.qp   = ctx->qp;
					wr_en.task.wqe_enable.wqe_count = 3;

					rc = ibv_exp_post_send(ctx->mqp, &wr_en, NULL);
					EXPECT_EQ(EOK, rc);
					ASSERT_EQ(EOK, rc);
				}
				++wrid;
			}

			/* poll the completion for a while before giving up of doing it .. */
			poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			s_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					EXPECT_LT(3, wrid);
					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			r_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					EXPECT_LT(3, wrid);
					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(3, s_poll_cq_count);
		EXPECT_EQ(3, r_poll_cq_count);
		EXPECT_EQ(scnt, rcnt);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		EXPECT_EQ(0, m_poll_cq_count);
	}
}


/* tc_verbs_post_recv_en: [TI.7]
 * IBV_EXP_QP_CREATE_MANAGED_RECV sent from MQP is able to push send requests
 * Every RECV_EN sets  IBV_SEND_WAIT_EN_LAST
 * sequence SEND-...-SEND-SEND_EN^(2)-SEND_EN^(2)-SEND_EN^(2)
 * Expected SIX CQE on rcq
 * Note: internal index is relative previous post and incremented after special WAIT posted
 */
TEST_F(tc_verbs_post_recv_en, ti_7) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int i;
		int routs;
		int rcnt, scnt;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		rcnt = 0;
		scnt = 0;
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				rc = __post_write(ctx, TEST_SET_WRID(TEST_SEND_WRID, wrid), IBV_EXP_WR_SEND);
				ASSERT_EQ(EOK, rc);

				if (wrid == (SEND_POST_COUNT - 1)) {
					struct ibv_exp_send_wr wr_en[3];

					wr_en[0].wr_id = RECV_EN_WR_ID + 1;
					wr_en[0].next = &wr_en[1];
					wr_en[0].sg_list = NULL;
					wr_en[0].num_sge = 0;
					wr_en[0].exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en[0].exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
					wr_en[0].ex.imm_data = 0;

					wr_en[1].wr_id = RECV_EN_WR_ID + 2;
					wr_en[1].next = &wr_en[2];
					wr_en[1].sg_list = NULL;
					wr_en[1].num_sge = 0;
					wr_en[1].exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en[1].exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
					wr_en[1].ex.imm_data = 0;

					wr_en[2].wr_id = RECV_EN_WR_ID + 3;
					wr_en[2].next = NULL;
					wr_en[2].sg_list = NULL;
					wr_en[2].num_sge = 0;
					wr_en[2].exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en[2].exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
					wr_en[2].ex.imm_data = 0;

					wr_en[0].task.wqe_enable.qp   = ctx->qp;
					wr_en[0].task.wqe_enable.wqe_count = 2;
					wr_en[1].task.wqe_enable.qp   = ctx->qp;
					wr_en[1].task.wqe_enable.wqe_count = 2;
					wr_en[2].task.wqe_enable.qp   = ctx->qp;
					wr_en[2].task.wqe_enable.wqe_count = 2;

					rc = ibv_exp_post_send(ctx->mqp, wr_en, NULL);
					EXPECT_EQ(EOK, rc);
					ASSERT_EQ(EOK, rc);
				}
				++wrid;
			}

			/* poll the completion for a while before giving up of doing it .. */
			poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			s_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					EXPECT_LT(3, wrid);
					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			r_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					EXPECT_LT(3, wrid);
					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(6, s_poll_cq_count);
		EXPECT_EQ(6, r_poll_cq_count);
		EXPECT_EQ(scnt, rcnt);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;
		EXPECT_EQ(3, m_poll_cq_count);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
		EXPECT_EQ(RECV_EN_WR_ID + 1, ctx->wc[0].wr_id);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
		EXPECT_EQ(RECV_EN_WR_ID + 2, ctx->wc[1].wr_id);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
		EXPECT_EQ(RECV_EN_WR_ID + 3, ctx->wc[2].wr_id);
	}
}


/* tc_verbs_post_recv_en: [TI.8]
 * IBV_EXP_QP_CREATE_MANAGED_RECV sent from MQP is able to push send requests
 * sequence SEND-...-SEND-RECV_EN(2)-RECV_EN(2)-RECV_EN^(2)
 * Only last RECV_EN sets  IBV_SEND_WAIT_EN_LAST
 * Expected TWO CQE on rcq
 * Note: internal index is relative previous post and incremented after special WAIT posted
 */
TEST_F(tc_verbs_post_recv_en, ti_8) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_RECV);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int i;
		int routs;
		int rcnt, scnt;
		int64_t	 wrid = 0;
		unsigned long start_time_msec;
		unsigned long cur_time_msec;
		struct timeval cur_time;
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;

		routs = ctx->qp_rx_depth;

		rcnt = 0;
		scnt = 0;
		gettimeofday(&cur_time, NULL);
		start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
		do {
			if (wrid < SEND_POST_COUNT) {
				rc = __post_write(ctx, TEST_SET_WRID(TEST_SEND_WRID, wrid), IBV_EXP_WR_SEND);
				ASSERT_EQ(EOK, rc);

				if (wrid == (SEND_POST_COUNT - 1)) {
					struct ibv_exp_send_wr wr_en[3];

					wr_en[0].wr_id = RECV_EN_WR_ID + 1;
					wr_en[0].next = &wr_en[1];
					wr_en[0].sg_list = NULL;
					wr_en[0].num_sge = 0;
					wr_en[0].exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en[0].exp_send_flags = IBV_EXP_SEND_SIGNALED;
					wr_en[0].ex.imm_data = 0;

					wr_en[1].wr_id = RECV_EN_WR_ID + 2;
					wr_en[1].next = &wr_en[2];
					wr_en[1].sg_list = NULL;
					wr_en[1].num_sge = 0;
					wr_en[1].exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en[1].exp_send_flags = IBV_EXP_SEND_SIGNALED;
					wr_en[1].ex.imm_data = 0;

					wr_en[2].wr_id = RECV_EN_WR_ID + 3;
					wr_en[2].next = NULL;
					wr_en[2].sg_list = NULL;
					wr_en[2].num_sge = 0;
					wr_en[2].exp_opcode = IBV_EXP_WR_RECV_ENABLE;
					wr_en[2].exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
					wr_en[2].ex.imm_data = 0;


					wr_en[0].task.wqe_enable.qp   = ctx->qp;
					wr_en[0].task.wqe_enable.wqe_count = 2;
					wr_en[1].task.wqe_enable.qp   = ctx->qp;
					wr_en[1].task.wqe_enable.wqe_count = 2;
					wr_en[2].task.wqe_enable.qp   = ctx->qp;
					wr_en[2].task.wqe_enable.wqe_count = 2;

					rc = ibv_exp_post_send(ctx->mqp, wr_en, NULL);
					EXPECT_EQ(EOK, rc);
					ASSERT_EQ(EOK, rc);
				}
				++wrid;
			}

			/* poll the completion for a while before giving up of doing it .. */
			poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			s_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					EXPECT_LT(3, wrid);
					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
			ASSERT_TRUE(poll_result >= 0);

			r_poll_cq_count += poll_result;

			/* CQE found */
			for (i = 0; i < poll_result; ++i) {
				if (ctx->wc[i].status != IBV_WC_SUCCESS) {
					VERBS_TRACE("Failed status %s (%d) for wr_id %d\n",
						ibv_wc_status_str(ctx->wc[i].status),
						ctx->wc[i].status, (int) ctx->wc[i].wr_id);
					ASSERT_TRUE(0);
				}

				switch (TEST_GET_TYPE(ctx->wc[i].wr_id)) {
				case TEST_SEND_WRID:
					++scnt;
					break;

				case TEST_RECV_WRID:
					if (--routs <= 1) {
						routs += __post_read(ctx, ctx->qp_rx_depth - routs);
						if (routs < ctx->qp_rx_depth) {
							VERBS_TRACE("Couldn't post receive (%d)\n", routs);
							ASSERT_TRUE(0);
						}
					}

					EXPECT_LT(3, wrid);
					++rcnt;
					break;

				default:
					VERBS_TRACE("Completion for unknown wr_id %d\n",
							(int) ctx->wc[i].wr_id);
					break;
				}
				VERBS_INFO("%-10s : wr_id=%-4" PRIu64 " scnt=%-4d rcnt=%-4d poll=%d\n",
						wr_id2str(ctx->wc[i].wr_id),
						TEST_GET_WRID(ctx->wc[i].wr_id),
						scnt, rcnt, poll_result);
			}

			gettimeofday(&cur_time, NULL);
			cur_time_msec = (cur_time.tv_sec * 1000)
					+ (cur_time.tv_usec / 1000);
		} while ((wrid < SEND_POST_COUNT)
				|| ((cur_time_msec - start_time_msec)
						< MAX_POLL_CQ_TIMEOUT));

		EXPECT_EQ(SEND_POST_COUNT, wrid);
		EXPECT_EQ(2, s_poll_cq_count);
		EXPECT_EQ(2, r_poll_cq_count);
		EXPECT_EQ(scnt, rcnt);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;
		EXPECT_EQ(3, m_poll_cq_count);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
		EXPECT_EQ(RECV_EN_WR_ID + 1, ctx->wc[0].wr_id);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
		EXPECT_EQ(RECV_EN_WR_ID + 2, ctx->wc[1].wr_id);
		EXPECT_EQ(IBV_WC_SUCCESS, ctx->wc[0].status);
		EXPECT_EQ(RECV_EN_WR_ID + 3, ctx->wc[2].wr_id);
	}
}
