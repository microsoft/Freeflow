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


class tc_verbs_post_task : public verbs_test_cd {};

#define SEND_POST_COUNT		10

/* verbs_post_task: [TI.1] Correct */
TEST_F(tc_verbs_post_task, ti_1) {

	int rc = EOK;

	__init_test(IBV_EXP_QP_CREATE_CROSS_CHANNEL | IBV_EXP_QP_CREATE_MANAGED_SEND);

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		int m_poll_cq_count = 0;
		struct ibv_exp_task task[2];
		struct ibv_exp_task *task_bad;
		struct ibv_exp_task *task_p;

		EXPECT_TRUE(SEND_POST_COUNT >= 10);

		/* create TASK(SEND) for qp */
		memset(task, 0, sizeof(*task) * 2);
		{
			struct ibv_exp_send_wr wr[SEND_POST_COUNT];
			int i = 0;

			memset(wr, 0, sizeof(*wr) * SEND_POST_COUNT);
			for (i = 0; i < SEND_POST_COUNT; i++) {
				wr[i].wr_id = i;
				wr[i].next = ( i < (SEND_POST_COUNT - 1) ? &wr[i + 1] : NULL);
				wr[i].sg_list = NULL;
				wr[i].num_sge = 0;
				wr[i].exp_opcode = IBV_EXP_WR_SEND;
				wr[i].exp_send_flags = IBV_EXP_SEND_SIGNALED;
				wr[i].ex.imm_data = 0;
			}

			task[0].task_type = IBV_EXP_TASK_SEND;
			task[0].item.qp = ctx->qp;
			task[0].item.send_wr = wr;

			task[0].next = NULL;
			task_p = &task[0];
		}

		/* create TASK(WAIT) for mqp */
		{
			struct ibv_exp_send_wr wr[4];

			memset(wr, 0, sizeof(*wr) * 4);

			/* SEND_EN(QP,1) */
			wr[0].wr_id = 0;
			wr[0].next = &wr[1];
			wr[0].sg_list = NULL;
			wr[0].num_sge = 0;
			wr[0].exp_opcode = IBV_EXP_WR_SEND_ENABLE;
			wr[0].exp_send_flags = IBV_EXP_SEND_SIGNALED;
			wr[0].ex.imm_data = 0;

			wr[0].task.wqe_enable.qp   = ctx->qp;
			wr[0].task.wqe_enable.wqe_count = 1;

			/* WAIT(QP,1) */
			wr[1].wr_id = 1;
			wr[1].next = &wr[2];
			wr[1].sg_list = NULL;
			wr[1].num_sge = 0;
			wr[1].exp_opcode = IBV_EXP_WR_CQE_WAIT;
			wr[1].exp_send_flags = IBV_EXP_SEND_SIGNALED;
			wr[1].ex.imm_data = 0;

			wr[1].task.cqe_wait.cq = ctx->mcq;
			wr[1].task.cqe_wait.cq_count = 1;

			/* SEND_EN(QP,ALL) */
			wr[2].wr_id = 2;
			wr[2].next = &wr[3];
			wr[2].sg_list = NULL;
			wr[2].num_sge = 0;
			wr[2].exp_opcode = IBV_EXP_WR_SEND_ENABLE;
			wr[2].exp_send_flags = 0;
			wr[2].ex.imm_data = 0;

			wr[2].task.wqe_enable.qp   = ctx->qp;
			wr[2].task.wqe_enable.wqe_count = 0;

			/* WAIT(QP,SEND_POST_COUNT) */
			wr[3].wr_id = 3;
			wr[3].next = NULL;
			wr[3].sg_list = NULL;
			wr[3].num_sge = 0;
			wr[3].exp_opcode = IBV_EXP_WR_CQE_WAIT;
			wr[3].exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
			wr[3].ex.imm_data = 0;

			wr[3].task.cqe_wait.cq = ctx->rcq;
			wr[3].task.cqe_wait.cq_count = SEND_POST_COUNT;

			task[1].task_type = IBV_EXP_TASK_SEND;
			task[1].item.qp = ctx->mqp;
			task[1].item.send_wr = wr;

			task[1].next = NULL;
			task_p->next = &task[1];
			task_p = &task[1];
		}

		rc = ibv_exp_post_task(ibv_ctx, task, &task_bad);
		ASSERT_EQ(EOK, rc);

		sleep(2);

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);

		poll_result = ibv_poll_cq(ctx->mcq, 0x10, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		m_poll_cq_count += poll_result;

		EXPECT_EQ(3, m_poll_cq_count);
		EXPECT_EQ((uint64_t)0, ctx->wc[0].wr_id);
		EXPECT_EQ((uint64_t)1, ctx->wc[1].wr_id);
		EXPECT_EQ((uint64_t)3, ctx->wc[2].wr_id);
	}
}

/* verbs_post_task: [TI.2] Bad case
 * Expected bad_task = task[1]
 */
TEST_F(tc_verbs_post_task, ti_2) {

	int rc = EOK;

	__init_test();

	/*
	 * Use the created QP for communication operations.
	 */
	{
		int poll_result;
		int s_poll_cq_count = 0;
		int r_poll_cq_count = 0;
		struct ibv_exp_task task[2];
		struct ibv_exp_task *task_bad;
		struct ibv_exp_task *task_p;

		EXPECT_TRUE(SEND_POST_COUNT >= 10);

		/* create TASK(SEND) for qp */
		memset(task, 0, sizeof(*task) * 2);
		{
			struct ibv_exp_send_wr wr[SEND_POST_COUNT];
			int i = 0;

			memset(wr, 0, sizeof(*wr) * SEND_POST_COUNT);
			for (i = 0; i < SEND_POST_COUNT; i++) {
				wr[i].wr_id = i;
				wr[i].next = ( i < (SEND_POST_COUNT - 1) ? &wr[i + 1] : NULL);
				wr[i].sg_list = NULL;
				wr[i].num_sge = 0;
				wr[i].exp_opcode = IBV_EXP_WR_SEND;
				wr[i].exp_send_flags = IBV_EXP_SEND_SIGNALED;
				wr[i].ex.imm_data = 0;
			}

			task[0].task_type = IBV_EXP_TASK_SEND;
			task[0].item.qp = ctx->qp;
			task[0].item.send_wr = wr;

			task[0].next = NULL;
			task_p = &task[0];
		}

		/* create TASK(WAIT) for mqp */
		{
			struct ibv_exp_send_wr wr;

			memset(&wr, 0, sizeof(wr));

			/* SEND_EN(QP,1) */
			wr.wr_id = 0;
			wr.next = NULL;
			wr.sg_list = NULL;
			wr.num_sge = 0;
			wr.exp_opcode = IBV_EXP_WR_SEND_ENABLE;
			wr.exp_send_flags = IBV_EXP_SEND_SIGNALED | IBV_EXP_SEND_WAIT_EN_LAST;
			wr.ex.imm_data = 0;

			wr.task.wqe_enable.qp   = ctx->qp;
			wr.task.wqe_enable.wqe_count = 1;

			task[1].task_type = IBV_EXP_TASK_SEND;
			task[1].item.qp = ctx->qp;
			task[1].item.send_wr = &wr;

			task[1].next = NULL;
			task_p->next = &task[1];
			task_p = &task[1];
		}

		rc = ibv_exp_post_task(ibv_ctx, task, &task_bad);
		ASSERT_NE(EOK, rc);
		ASSERT_EQ(task_bad, &task[1]);

		sleep(2);

		poll_result = ibv_poll_cq(ctx->scq, ctx->cq_tx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		s_poll_cq_count += poll_result;

		poll_result = ibv_poll_cq(ctx->rcq, ctx->cq_rx_depth, ctx->wc);
		ASSERT_TRUE(poll_result >= 0);
		r_poll_cq_count += poll_result;

		EXPECT_EQ(SEND_POST_COUNT, s_poll_cq_count);
		EXPECT_EQ(SEND_POST_COUNT, r_poll_cq_count);
	}
}
