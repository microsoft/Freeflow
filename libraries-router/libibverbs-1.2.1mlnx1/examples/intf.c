/*
 * Copyright (c) 2015 Mellanox Technologies Ltd.  All rights reserved.
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
 *
 *
 * Author: Moshe Lazer <moshel@mellanox.com>
 */

#define _GNU_SOURCE

#include <pthread.h>
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
#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <semaphore.h>
#include <locale.h>

#include "get_clock.h"

#ifndef likely
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#else
#define likely(x)	(x)
#endif
#endif


#ifndef unlikely
#ifdef __GNUC__
#define unlikely(x)       __builtin_expect(!!(x), 0)
#else
#define unlikely(x)       (x)
#endif
#endif


enum input_send_intf {
	IN_NORMAL_SEND_INTF,
	IN_ACC_SEND_PENDING_INTF,
	IN_ACC_SEND_PENDING_INL_INTF,
	IN_ACC_SEND_PENDING_SG_LIST_INTF,
	IN_ACC_SEND_BURST_INTF,
	IN_NUM_SEND_INTF
};

enum input_recv_intf {
	IN_NORMAL_RECV_INTF,
	IN_ACC_RECV_BURST_INTF,
	IN_NUM_RECV_INTF,
};

enum input_poll_intf {
	IN_NORMAL_POLL_INTF,
	IN_ACC_POLL_CNT_INTF,
	IN_ACC_POLL_LENGTH_INTF,
	IN_NUM_POLL_INTF
};

enum qp_intf {
	NORMAL_SEND_INTF,
	NORMAL_RECV_INTF,
	ACC_SEND_PENDING_INTF,
	ACC_SEND_PENDING_INL_INTF,
	ACC_SEND_PENDING_SG_LIST_INTF,
	ACC_SEND_BURST_INTF,
	ACC_RECV_BURST_INTF,
};

enum cq_intf {
	NORMAL_POLL_INTF,
	ACC_POLL_CNT_INTF,
	ACC_POLL_LENGTH_INTF,
	ACC_POLL_LENGTH_INL_INTF,
};

struct qp_params {
	int wr_burst;
	int max_send_wr;
	int max_recv_wr;
	int max_inl_recv_data;
	enum input_send_intf verbs_send_intf;
	enum input_recv_intf verbs_recv_intf;
	enum input_poll_intf verbs_send_poll_intf;
	enum input_poll_intf verbs_recv_poll_intf;
};

struct send_params {
	int msg_size;
	int num_qp_msgs;
};

struct cpu_set {
	int min;
	int max;
};

#define MAX_CPU_SETS 4
struct thread_params {
	int num_threads;
	int num_cpu_sets;
	struct cpu_set cpu_sets[MAX_CPU_SETS];
};
#define MAX_DEV_NAME_SIZE 20
struct ib_data {
	char dev_name[MAX_DEV_NAME_SIZE];
	int ib_port_num;
	int sl;
	enum ibv_mtu mtu;
	int check_data;
	int use_res_domain;
};

#define MAX_SERVER_NAME_SIZE 128
struct server_data {
	char name[MAX_SERVER_NAME_SIZE];
	int port;
};

struct intf_input {
	struct server_data server_data;
	struct ib_data ib_data;
	struct qp_params qp_prms;
	struct send_params send_prms;
	struct thread_params thread_prms;
};

struct intf_input intf_default_input = {
		.server_data = {
			.name = "",
			.port = 18515
		},
		.ib_data = {
			.dev_name = "mlx4_0",
			.ib_port_num = 1,
			.sl = 0,
			.mtu = IBV_MTU_4096,
			.check_data = 0,
			.use_res_domain = 1,
		},
		.qp_prms = {
			.wr_burst = 10, /* burst-size: the number of messages to use in one send/receive transaction */
			.max_send_wr = 3*5*4*5*7, /* Defines the size of send queue in messages (must be multiplication of burst-size) */
			.max_recv_wr = 3*5*4*5*7, /* Defines the size of recive queue in messages (must be multiplication of burst-size) */
			.max_inl_recv_data = 0, /* max in-line receive data to use for QP creation */
			.verbs_send_intf = IN_ACC_SEND_PENDING_INTF, /* Defines which send interface to use */
			.verbs_recv_intf = IN_ACC_RECV_BURST_INTF, /* Defines which receive interface to use */
			.verbs_send_poll_intf = IN_ACC_POLL_CNT_INTF, /* Defines which poll interface to use for sent messages */
			.verbs_recv_poll_intf = IN_ACC_POLL_LENGTH_INTF, /* Defines which poll interface to use for received messages */
		},
		.send_prms = {
			.msg_size =	64, /* msg size */
			.num_qp_msgs =	1000000, /* Number of messages to send via each QP */
		},

		.thread_prms = {
			.num_threads = 1, /* number of threads to use */
			.num_cpu_sets = 2, /* This field and the cpu_sets field define on which CPUs application threads may run */
			.cpu_sets = { {0, 5}, {12, 17} }
		}
};
struct intf_input intf_input;

#define INVALID_DURATION ((unsigned long)(-1))
struct qp_data {
	int				remote_qpn;
	int				psn;
	int				msg_size;
	int				msg_stride;
	long				num_msgs;
	int				wr_burst;
	int				max_wrs;
	int				max_inl_recv_data;
	int				max_inline_data;
	enum qp_intf			qp_intf;
	unsigned long			total_ms;
	struct ibv_qp			*qp;
	struct ibv_sge			*sg_list;
	struct ibv_send_wr		*send_wr;
	struct ibv_recv_wr		*recv_wr;
	struct ibv_exp_qp_burst_family	*qp_burst_family;
	char				*buf;
	struct ibv_mr			*mr;
};

struct cq_data {
	int				wc_burst;
	int				cq_size;
	enum cq_intf			cq_intf;
	struct ibv_cq			*cq;
	struct ibv_wc			*wc;
	struct ibv_exp_cq_family	*cq_family;
};

struct qp_cq_data {
	int 		idx;
	struct qp_data 	qp;
	struct cq_data 	cq;
};

#define MAX_INLINE_RECV 512
struct intf_context;
struct intf_thread {
	struct intf_context		*ctx;
	struct ibv_exp_res_domain	*single_res_domain;
	char				inlr_buf[MAX_INLINE_RECV];
	uint32_t			use_inlr;
	int				qp_idx;
	int				thread_idx;
	int				cpu;
	unsigned long			cpu_freq;
};

#define MAX_MSG_SIZE 0x10000

struct ib_dest {
	int lid;
	union ibv_gid gid;
	int *qpn;
	int *psn;
};

struct intf_context {
	char				*servername;
	int				is_send;
	int				port;
	char				dev_name[MAX_DEV_NAME_SIZE];
	struct ibv_device		*ib_dev;
	struct ibv_context		*context;
	struct ibv_pd			*pd;
	int				ib_port_num;
	int				sl;
	enum ibv_mtu			mtu;
	int				num_qps_cqs;
	struct qp_cq_data		*qps_cqs;
	int				num_threads;
	struct intf_thread		*threads;
	sem_t				threads_sem;
	sem_t				threads_done_sem;
	int				thread_stop;
	struct ibv_exp_device_attr	dattr;
	struct ib_dest			remote_dst;
	struct ib_dest			local_dst;
	int				check_data;
	int				use_res_domain;
};

sem_t clk_sem;

static inline double clk_get_cpu_hz(int no_cpu_freq_fail)
{
	double cycles_in_sec;

	sem_wait(&clk_sem);
	cycles_in_sec = get_cpu_mhz(0) * 1000000;
	sem_post(&clk_sem);

	return cycles_in_sec;
}

static inline cycles_t clk_get_cycles(void)
{
	cycles_t cycles;

	sem_wait(&clk_sem);
	cycles = get_cycles();
	sem_post(&clk_sem);

	return cycles;
}

static inline void clk_init(void)
{
	sem_init(&clk_sem, 0, 1);
}

#define mmax(a, b) ((a) > (b) ? (a) : (b))
#define mmin(a, b) ((a) < (b) ? (a) : (b))

static void gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
{
	int i;
	uint32_t *raw = (uint32_t *)gid->raw;

	for (i = 0; i < 4; ++i)
		sprintf(&wgid[i * 8], "%08x",
			htonl(raw[i]));
}

static void wire_gid_to_gid(const char *wgid, const union ibv_gid *gid)
{
	char tmp[9];
	uint32_t v32;
	uint32_t *raw = (uint32_t *)gid->raw;
	int i;

	for (tmp[8] = 0, i = 0; i < 4; ++i) {
		memcpy(tmp, wgid + i * 8, 8);
		if (sscanf(tmp, "%x", &v32) != 1)
			v32 = 0;
		raw[i] = ntohl(v32);
	}
}

static int get_rand(int a, int b)
{
	return (lrand48() & 0xffffffff) % (b - a);
}

static inline char *enum_to_cq_intf_str(enum cq_intf cq_intf)
{
	switch (cq_intf) {
	case NORMAL_POLL_INTF:		return "NORMAL_POLL_INTF";
	case ACC_POLL_CNT_INTF:		return "ACC_POLL_CNT_INTF";
	case ACC_POLL_LENGTH_INTF:	return "ACC_POLL_LENGTH_INTF";
	case ACC_POLL_LENGTH_INL_INTF:	return "ACC_POLL_LENGTH_INL_INTF";
	default:			return "ERR_INTF";
	}
}

/*
 * qp_poll - poll for send/receive completions using the different verbs interfaces
 */
static inline int qp_poll(struct cq_data *cq_data, const enum cq_intf cq_intf, struct intf_thread *thread) __attribute__((always_inline));
static inline int qp_poll(struct cq_data *cq_data, const enum cq_intf cq_intf, struct intf_thread *thread)
{
	int consumed;
	int i;

	switch (cq_intf) {
	case NORMAL_POLL_INTF:
		consumed = ibv_poll_cq(cq_data->cq, cq_data->wc_burst, cq_data->wc);
		if (likely(consumed > 0)) {
			for (i = 0; i < consumed; i++) {
				if (cq_data->wc[i].status) {
					fprintf(stderr, "poll_cq erroneous status %d\n", cq_data->wc[i].status);

					return -1;
				}
			}
		}
		break;

	case ACC_POLL_CNT_INTF:
		consumed = cq_data->cq_family->poll_cnt(cq_data->cq, cq_data->wc_burst);
		break;

	case ACC_POLL_LENGTH_INTF:
		consumed = cq_data->cq_family->poll_length(cq_data->cq, NULL, NULL);
		if (consumed > 0)
			consumed = 1;
		break;

	case ACC_POLL_LENGTH_INL_INTF:
		consumed = cq_data->cq_family->poll_length(cq_data->cq, thread->inlr_buf, &thread->use_inlr);
		if (consumed > 0)
			consumed = 1;
		break;

	default:
		fprintf(stderr, "qp_poll - interface %d not supported\n", cq_intf);
		consumed = -1;
	}

	return consumed;
}

/*
 * qp_post - send/receive a burst of messages using the different verbs interfaces
 */
static inline int qp_post(struct qp_data *qp_data, struct ibv_sge *sg_list, int wr_idx, const enum qp_intf qp_intf) __attribute__((always_inline));
static inline int qp_post(struct qp_data *qp_data, struct ibv_sge *sg_list, int wr_idx, const enum qp_intf qp_intf)
{
	int i;
	int ret = 0;
	struct ibv_recv_wr *bad_rwr;
	struct ibv_send_wr *bad_swr;

	switch (qp_intf) {
	case NORMAL_SEND_INTF:
		ret = ibv_post_send(qp_data->qp, &qp_data->send_wr[wr_idx % qp_data->max_wrs], &bad_swr);
		break;

	case NORMAL_RECV_INTF:
		ret = ibv_post_recv(qp_data->qp, &qp_data->recv_wr[wr_idx % qp_data->max_wrs], &bad_rwr);
		break;

	case ACC_SEND_PENDING_INTF:
		for (i = 0; i < qp_data->wr_burst && !ret; i++) {
			struct ibv_sge *sg_l = sg_list + i;

			ret = qp_data->qp_burst_family->send_pending(qp_data->qp, sg_l->addr, sg_l->length, sg_l->lkey, IBV_EXP_QP_BURST_SIGNALED);
		}
		if (!ret)
			ret = qp_data->qp_burst_family->send_flush(qp_data->qp);
		break;

	case ACC_SEND_PENDING_INL_INTF:
		for (i = 0; i < qp_data->wr_burst && !ret; i++) {
			struct ibv_sge *sg_l = sg_list + i;

			ret = qp_data->qp_burst_family->send_pending_inline(qp_data->qp, (void *)(uintptr_t)sg_l->addr, sg_l->length, IBV_EXP_QP_BURST_SIGNALED);
		}
		if (!ret)
			ret = qp_data->qp_burst_family->send_flush(qp_data->qp);
		break;

	case ACC_SEND_PENDING_SG_LIST_INTF:
		for (i = 0; i < qp_data->wr_burst && !ret; i++) {
			struct ibv_sge *sg_l = sg_list + i;

			ret = qp_data->qp_burst_family->send_pending_sg_list(qp_data->qp, sg_l, 1, IBV_EXP_QP_BURST_SIGNALED);
		}
		if (!ret)
			ret = qp_data->qp_burst_family->send_flush(qp_data->qp);
		break;

	case ACC_SEND_BURST_INTF:
		ret = qp_data->qp_burst_family->send_burst(qp_data->qp, sg_list, qp_data->wr_burst, IBV_EXP_QP_BURST_SIGNALED);
		break;

	case ACC_RECV_BURST_INTF:
		ret = qp_data->qp_burst_family->recv_burst(qp_data->qp, sg_list, qp_data->wr_burst);
		break;
	}

	if (unlikely(ret)) {
		fprintf(stderr, "ibv_post_send failed in interface = %d, err = %d\n", qp_intf, ret);
		return -1;
	}

	return qp_data->wr_burst;
}

/* On each byte of the message put a nibble from the WR index
 * and a nibble from the QP index
 */
static char calc_msg_data(int wr_idx, int qp_idx)
{
	char data = (char)((wr_idx & 0xF) | (qp_idx << 4));

	return data;
}

/*
 * is_data_valid - Check received data
 * To keep performance it checks only one random byte from the
 * received (consumed) messages.
 */
static int is_data_valid(long *curr_poll_wr, int consumed, struct qp_cq_data *qp_cq_data, struct intf_thread *thread)
{
	char rand_data;
	int rand_wr = (*curr_poll_wr + get_rand(0, consumed)) % qp_cq_data->qp.max_wrs;
	int rand_idx = get_rand(0, qp_cq_data->qp.msg_size);
	char send_data = calc_msg_data(rand_wr, qp_cq_data->idx);

	if (thread->use_inlr)
		rand_data = thread->inlr_buf[rand_idx];
	else
		rand_data = (qp_cq_data->qp.buf + rand_wr * qp_cq_data->qp.msg_stride)[rand_idx];
	if (rand_data != send_data) {
		int wr, i;

		fprintf(stderr, "Received wrong data on thread = %d  expected value = 0x%x actual value = 0x%x\n",
		       thread->thread_idx, send_data, rand_data);
		fprintf(stderr, "   use_inlr %d, curr_poll_wr %ld(0x%lx), consumed %d, rand_wr = %d, rand_idx = %d msg_size = %d\n",
				thread->use_inlr, *curr_poll_wr, *curr_poll_wr, consumed, rand_wr, rand_idx, qp_cq_data->qp.msg_size);
		for (wr = 0; wr < rand_wr + 2; wr++) {
			int max_print = mmin(qp_cq_data->qp.msg_stride, 128);

			fprintf(stderr, "wr %d:", wr);
			for (i = 0; i < max_print; i++) {
				if (i == qp_cq_data->qp.msg_size)
					fprintf(stderr, " |");
				fprintf(stderr, " %x", *(qp_cq_data->qp.buf + wr * qp_cq_data->qp.msg_stride + i));
			}
			fprintf(stderr, "\n:");
		}
		return 0;
	}
	thread->use_inlr = 0;
	*curr_poll_wr += consumed;

	return 1;
}

/* send_recv - is a function to send/receive messages using one QP/CQ set.
 *
 * While there are more messages to send/receive it uses the following logic:
 * 1. Fill send/receive queue with burst of messages.
 * 2. Poll completion queue until there is enough space in the QP to post
 *    additional burst (go back to 1).
 */
static inline int send_recv(struct qp_cq_data *qp_cq_data,
			    struct intf_thread *thread, const int check_data,
			    const enum qp_intf qp_intf, enum cq_intf cq_intf)
{
	int msg = 0;
	int num_wrs;
	int free_wrs;
	int consumed;
	long curr_poll_wr = 0;
	struct ibv_sge *base_sg_list;

	num_wrs = qp_cq_data->qp.max_wrs;
	base_sg_list = qp_cq_data->qp.sg_list;

	free_wrs = mmin(num_wrs, qp_cq_data->qp.num_msgs);

	while (msg < qp_cq_data->qp.num_msgs) {
		/* Fill send/receive queue using bursts of messages*/
		while (free_wrs >= qp_cq_data->qp.wr_burst) {
			if (qp_post(&qp_cq_data->qp, base_sg_list + (msg % num_wrs), msg, qp_intf) < 0) {
				fprintf(stderr, "Post QP(%d) failed for thread %d\n", qp_intf, thread->thread_idx);
				return 1;
			}
			msg += qp_cq_data->qp.wr_burst;
			free_wrs -= qp_cq_data->qp.wr_burst;
		}

		/* In order to put another burst of messages we need first to
		 * make sure there is enough space in the send/receive queue.
		 * Poll on the completion queue until we get the required space
		 */
		do {
			consumed = qp_poll(&qp_cq_data->cq, cq_intf, thread);
			if (likely(consumed > 0)) {
				free_wrs += consumed;
				if (unlikely(check_data))
					if (!is_data_valid(&curr_poll_wr, consumed, qp_cq_data, thread))
						return 1;
			} else if (consumed < 0) {
				fprintf(stderr, "Poll CQ(%s) failed for thread %d\n", enum_to_cq_intf_str(cq_intf), thread->thread_idx);
				return 1;
			}
		} while (free_wrs < qp_cq_data->qp.wr_burst && msg < qp_cq_data->qp.num_msgs);
	}

	return 0;
}

static enum qp_intf send_2_qp[IN_NUM_SEND_INTF] = {
		[IN_NORMAL_SEND_INTF]			= NORMAL_SEND_INTF,
		[IN_ACC_SEND_PENDING_INTF]		= ACC_SEND_PENDING_INTF,
		[IN_ACC_SEND_PENDING_INL_INTF]		= ACC_SEND_PENDING_INL_INTF,
		[IN_ACC_SEND_PENDING_SG_LIST_INTF]	= ACC_SEND_PENDING_SG_LIST_INTF,
		[IN_ACC_SEND_BURST_INTF]		= ACC_SEND_BURST_INTF,
};

static enum qp_intf recv_2_qp[IN_NUM_POLL_INTF] = {
		[IN_NORMAL_RECV_INTF]		= NORMAL_RECV_INTF,
		[IN_ACC_RECV_BURST_INTF]	= ACC_RECV_BURST_INTF,
};

static enum cq_intf poll_2_cq[IN_NUM_POLL_INTF] = {
		[IN_NORMAL_POLL_INTF]		= NORMAL_POLL_INTF,
		[IN_ACC_POLL_CNT_INTF]		= ACC_POLL_CNT_INTF,
		[IN_ACC_POLL_LENGTH_INTF]	= ACC_POLL_LENGTH_INTF,
};

static int run_thread_on_cpu(int cpu, int thread_idx) {
	int j;
	cpu_set_t cpuset;
	pthread_t pthread;

	pthread = pthread_self();

	/* Set the selected cpu for the thread */
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);

	/* Force the thread to run on the selected cpu */
	if (pthread_setaffinity_np(pthread, sizeof(cpu_set_t), &cpuset))
		return 1;

	/* Make sure the thread is running on the selected cpu */
	if (pthread_getaffinity_np(pthread, sizeof(cpu_set_t), &cpuset)) {
		fprintf(stderr, "Couldn't get thread(%d) affinity\n",
			thread_idx);
	} else {
		for (j = 0; j < CPU_SETSIZE; j++)
			if (CPU_ISSET(j, &cpuset) && (j != cpu))
				return 1;
	}

	return 0;
}


/* __thread_wrap - is the main function of all application threads.
 * The thread switch itself to the right cpu, waits for sync from main
 * thread to start send/receive messages, executes send/receive messages
 * and signals the main thread upon completion.
 */
static void *__thread_wrap(void *arg)
{
	cycles_t start;
	struct intf_thread *thread = (struct intf_thread *)arg;
	struct qp_cq_data *qp_cq = &thread->ctx->qps_cqs[thread->qp_idx];
	int check_data = thread->ctx->check_data;


	/* Run thread on selected cpu */
	if (run_thread_on_cpu(thread->cpu, thread->thread_idx)) {
		fprintf(stderr, "Couldn't run thread %d on cpu %d (errno = %d)\n",
			thread->thread_idx, thread->cpu, errno);
		goto thread_out;
	} else {
		printf("\tThread %d - Start on cpu %d\n", thread->thread_idx, thread->cpu);
	}

	/* Get the cpu clk frequency */
	thread->cpu_freq = (unsigned long)clk_get_cpu_hz(0);
	if (thread->cpu_freq == 0)
		fprintf(stderr, "Can't get cpu(%d) frequency\n", thread->cpu);

	/* Wait for signal to start the send/receive process */
	sem_wait(&thread->ctx->threads_sem);

	qp_cq->qp.total_ms = INVALID_DURATION;

	start = clk_get_cycles();

	/* Send/receive thread messages */
	if (send_recv(qp_cq, thread, check_data, qp_cq->qp.qp_intf, qp_cq->cq.cq_intf) ||
	    !thread->cpu_freq)
		/* Total exec time not valid if send_recv or cpu_freq failed */
		goto thread_out;

	/* calculate the total execution time in milli-seconds */
	qp_cq->qp.total_ms = ((clk_get_cycles() - start) * 1000) /
			      thread->cpu_freq;

thread_out:
	/* signal about thread completion */
	sem_post(&thread->ctx->threads_done_sem);
	printf("\tThread %d - done\n", thread->thread_idx);
	pthread_exit(NULL);
}

static inline char *send_enum_to_verbs_intf_str(enum input_send_intf verbs_intf)
{
	switch (verbs_intf) {
	case IN_NORMAL_SEND_INTF:		return "S_NORM";
	case IN_ACC_SEND_PENDING_INTF:		return "S_PEND";
	case IN_ACC_SEND_PENDING_INL_INTF:	return "S_PEND_INL";
	case IN_ACC_SEND_PENDING_SG_LIST_INTF:	return "S_PEND_SG_LIST";
	case IN_ACC_SEND_BURST_INTF:		return "S_BURST";
	default:				return "ERR_SEND_INTF";
	}
}

static inline char *recv_enum_to_verbs_intf_str(enum input_recv_intf verbs_intf)
{
	switch (verbs_intf) {
	case IN_NORMAL_RECV_INTF:	return "R_NORM";
	case IN_ACC_RECV_BURST_INTF:	return "R_BURST";
	default:			return "ERR_RECV_INTF";
	}
}

static inline char *poll_enum_to_verbs_intf_str(enum input_poll_intf verbs_intf)
{
	switch (verbs_intf) {
	case IN_NORMAL_POLL_INTF:	return "P_NORM";
	case IN_ACC_POLL_CNT_INTF:	return "P_CNT";
	case IN_ACC_POLL_LENGTH_INTF:	return "P_LEN";
	default:			return "ERR_POLL_INTF";
	}
}

static inline char *qp_intf_to_param_str(enum qp_intf verbs_intf)
{
	switch (verbs_intf) {
	case NORMAL_SEND_INTF:			return "S_NORM";
	case NORMAL_RECV_INTF:			return "R_NORM";
	case ACC_SEND_PENDING_INTF:		return "S_PEND";
	case ACC_SEND_PENDING_INL_INTF:		return "S_PEND_INL";
	case ACC_SEND_PENDING_SG_LIST_INTF:	return "S_PEND_SG_LIST";
	case ACC_SEND_BURST_INTF:		return "S_BURST";
	case ACC_RECV_BURST_INTF:		return "R_BURST";
	default:				return "ERR_QP_INTF";
	}
}

static inline char *cq_intf_to_param_str(enum cq_intf verbs_intf)
{
	switch (verbs_intf) {
	case NORMAL_POLL_INTF:		return "P_NORM";
	case ACC_POLL_CNT_INTF:		return "P_CNT";
	case ACC_POLL_LENGTH_INTF:	return "P_LEN";
	case ACC_POLL_LENGTH_INL_INTF:	return "P_LEN";
	default:			return "ERR_CQ_INTF";
	}
}

static void print_qp_report(struct qp_cq_data *qp_cq_data, int send)
{

	struct qp_data *qp_data = &qp_cq_data->qp;
	char *post_s = qp_intf_to_param_str(qp_data->qp_intf);
	char *poll_s = cq_intf_to_param_str(qp_cq_data->cq.cq_intf);
	long mps;

	if (!qp_data->total_ms || qp_data->total_ms == INVALID_DURATION) {
		if (qp_data->total_ms == INVALID_DURATION)
			printf("\tTest execution aborted!\n");
		else
			printf("\tTest execution time is too short to measure!\n");
		mps = 0;
	} else {
		mps = (qp_data->num_msgs * 1000) / qp_data->total_ms;
	}
	printf("\tmsg_size = %d, num_sge = 1, wr_burst = %d, intf = %s:%s, num_msgs = %'ld, time_ms = %'ld",
			qp_data->msg_size, qp_data->wr_burst, post_s, poll_s,
			qp_data->num_msgs, qp_data->total_ms);
	if (mps)
		printf(" mps = %'ld\n", mps);
	else
		printf("\n");
}

static void print_thread_report(struct intf_thread *thread)
{
	printf("Thread %d: CPU = %d MHz = %ld\n",
	       thread->thread_idx, thread->cpu, thread->cpu_freq/1000000);

	if (!thread->ctx->thread_stop) {
		printf("\t%s QP %d data:\n",
		       thread->ctx->is_send ? "Send" : "Recv",
		       thread->qp_idx);
		print_qp_report(&thread->ctx->qps_cqs[thread->qp_idx],
				thread->ctx->is_send);
	}
}

static void print_global_report(struct intf_context *ctx)
{
	printf("Global test parameters: check_data = %d use_res_domain = %d\n",
			ctx->check_data, ctx->use_res_domain);
}

int run_threads(struct intf_context *ctx)
{
	int i, j;
	int err;
	pthread_t tid;

	sem_init(&ctx->threads_sem, 0, 0);
	sem_init(&ctx->threads_done_sem, 0, 0);
	clk_init();
	for (i = 0; i < ctx->num_threads; i++) {
		ctx->threads[i].thread_idx = i;
		ctx->threads[i].ctx = ctx;
		err = pthread_create(&tid, NULL, __thread_wrap, &ctx->threads[i]);
		if (err != 0) {
			fprintf(stderr, "Can't create thread :[%s]", strerror(err));
			goto clean_threads;
		}
	}

	for (i = 0; i < ctx->num_threads; i++)
		sem_post(&ctx->threads_sem);

	for (i = 0; i < ctx->num_threads; i++)
		sem_wait(&ctx->threads_done_sem);

	print_global_report(ctx);
	for (i = 0; i < ctx->num_threads; i++)
		print_thread_report(&ctx->threads[i]);

	return 0;

clean_threads:

	ctx->thread_stop = 1;
	for (j = i ; j > 0; j--)
		sem_post(&ctx->threads_sem);

	for (j = i ; j > 0; j--)
		sem_wait(&ctx->threads_done_sem);

	return 1;
}

static int connect_qp(struct ibv_qp *qp, int port, int my_psn,
		      enum ibv_mtu mtu, int sl,
		      union ibv_gid r_gid, int r_lid, int r_psn, int r_qpn,
		      int sgid_idx)
{
	struct ibv_qp_attr attr = {
		.qp_state		= IBV_QPS_RTR,
		.path_mtu		= mtu,
		.dest_qp_num		= r_qpn,
		.rq_psn			= r_psn,
		.max_dest_rd_atomic	= 1,
		.min_rnr_timer		= 12,
		.ah_attr		= {
			.is_global	= 0,
			.dlid		= r_lid,
			.sl		= sl,
			.src_path_bits	= 0,
			.port_num	= port
		}
	};

	if (r_gid.global.interface_id) {
		attr.ah_attr.is_global = 1;
		attr.ah_attr.grh.hop_limit = 1;
		attr.ah_attr.grh.dgid = r_gid;
		attr.ah_attr.grh.sgid_index = sgid_idx;
	}
	if (ibv_modify_qp(qp, &attr,
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
	if (ibv_modify_qp(qp, &attr,
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


static int create_cq(struct intf_context *ctx,
		     struct qp_cq_data *qps_cqs,
		     struct ibv_exp_cq_init_attr *init_attr,
		     struct ibv_exp_query_intf_params *intf_params)
{
	enum ibv_exp_query_intf_status intf_status;

	qps_cqs->cq.wc = calloc(1, sizeof(struct ibv_wc) * qps_cqs->cq.wc_burst);

	if (!qps_cqs->cq.wc)
		return 1;

	init_attr->res_domain = ctx->threads[qps_cqs->idx].single_res_domain;

	qps_cqs->cq.cq = ibv_exp_create_cq(ctx->context, qps_cqs->cq.cq_size,
					   NULL, NULL, 0, init_attr);
	if (!qps_cqs->cq.cq) {
		fprintf(stderr, "Couldn't create CQ (errno = %d)\n", errno);
		goto free_wc;
	}

	intf_params->intf = IBV_EXP_INTF_CQ;
	intf_params->obj = qps_cqs->cq.cq;
	if (qps_cqs->cq.cq_intf != NORMAL_POLL_INTF) {
		qps_cqs->cq.cq_family = ibv_exp_query_intf(ctx->context, intf_params, &intf_status);
		if (!qps_cqs->cq.cq_family)  {
			fprintf(stderr, "Couldn't create CQ family (intf_status = %d)\n", intf_status);
			goto destroy_cq;
		}
	}

	return 0;

destroy_cq:
	ibv_destroy_cq(qps_cqs->cq.cq);

free_wc:
	free(qps_cqs->cq.wc);

	return 1;
}

static void destroy_cq(struct intf_context *ctx,
		       struct qp_cq_data *qps_cqs,
		       struct ibv_exp_release_intf_params *rel_intf)
{
	ibv_exp_release_intf(ctx->context, qps_cqs->cq.cq_family, rel_intf);
	ibv_destroy_cq(qps_cqs->cq.cq);
	free(qps_cqs->cq.wc);
}

static int create_qp(struct intf_context *ctx,
		     struct qp_cq_data *qps_cqs,
		     struct ibv_qp_attr *attr,
		     struct ibv_exp_qp_init_attr *init_attr,
		     struct ibv_exp_query_intf_params *intf_params)
{
	enum ibv_exp_query_intf_status intf_status;
	struct qp_data *qp = &qps_cqs->qp;
	int max_wr;
	void *tmp;
	int j;

	init_attr->recv_cq = qps_cqs->cq.cq;
	init_attr->send_cq = qps_cqs->cq.cq;

	init_attr->pd = ctx->pd,
	init_attr->max_inl_recv = qp->max_inl_recv_data;
	init_attr->cap.max_send_wr  = qp->max_wrs,
	init_attr->cap.max_recv_wr  = qp->max_wrs,
	init_attr->cap.max_send_sge = 1,
	init_attr->cap.max_recv_sge = 1,
	init_attr->cap.max_inline_data = qp->max_inline_data,
	init_attr->qp_type = IBV_QPT_RC;

	qp->psn = lrand48() & 0xffffff;

	/* allocate WR, WC and sg list buffers */
	max_wr = qp->max_wrs;
	qp->sg_list = calloc(1, max_wr * sizeof(struct ibv_sge));
	if (ctx->is_send) {
		qp->send_wr = calloc(1, sizeof(struct ibv_send_wr) * max_wr);
		tmp = qp->send_wr;
	} else {
		qp->recv_wr = calloc(1, sizeof(struct ibv_recv_wr) * max_wr);
		tmp = qp->recv_wr;
	}
	if (!tmp || !qp->sg_list) {
		fprintf(stderr, "Couldn't allocate WRs/WCs buffers\n");
		goto clean_qp;
	}

	/* Create the QP */
	init_attr->res_domain = ctx->threads[qps_cqs->idx].single_res_domain;
	init_attr->max_inl_recv = qp->max_inl_recv_data;
	if (qp->max_inl_recv_data)
		init_attr->comp_mask |= IBV_EXP_QP_INIT_ATTR_INL_RECV;
	else
		init_attr->comp_mask &= ~IBV_EXP_QP_INIT_ATTR_INL_RECV;

	qp->qp = ibv_exp_create_qp(ctx->context, init_attr);
	if (!qp->qp)  {
		fprintf(stderr, "Couldn't create QP\n");
		goto clean_qp;
	}

	/* Create messages buffer */
	qp->msg_stride = mmax(qp->msg_size, 64);
	qp->buf = memalign(sysconf(_SC_PAGESIZE), max_wr * qp->msg_stride);
	if (!qp->buf) {
		fprintf(stderr, "Couldn't allocate recv/send buff for qp[%d]\n", qps_cqs->idx);
		goto destroy_qp;
	}
	qp->mr = ibv_reg_mr(ctx->pd, qp->buf, max_wr * qp->msg_stride,
			     IBV_ACCESS_LOCAL_WRITE);
	if (!qp->mr) {
		fprintf(stderr, "Couldn't allocate recv/send MR for qp[%d]\n", qps_cqs->idx);
		goto free_buf;
	}

	/* Modify QP to INIT */
	if (ibv_modify_qp(qp->qp, attr,
			  IBV_QP_STATE              |
			  IBV_QP_PKEY_INDEX         |
			  IBV_QP_PORT               |
			  IBV_QP_ACCESS_FLAGS)) {
		fprintf(stderr, "Failed to modify QP to INIT\n");
		goto dereg_mr;
	}


	/* Prepare WRs and SG lists */
	for (j = 0; j < max_wr; j++) {
		char *msg_buf = qp->buf + (j * qp->msg_stride);
		char send_data = calc_msg_data(j, qps_cqs->idx);
		int seg_size = qp->msg_size;

		if (ctx->is_send)
			memset(msg_buf, send_data, qp->msg_size);

		qp->sg_list[j].addr = (uintptr_t)msg_buf;
		qp->sg_list[j].length = seg_size;
		qp->sg_list[j].lkey = qp->mr->lkey;

		msg_buf += seg_size;
		if (ctx->is_send) {
			/* For sender prepare pre-defined send_wr */
			struct ibv_send_wr *send_wr = &qp->send_wr[j];

			if (j % qp->wr_burst != qp->wr_burst - 1)
				send_wr->next = &qp->send_wr[j + 1];
			send_wr->num_sge = 1;
			send_wr->opcode = IBV_WR_SEND;
			send_wr->send_flags = IBV_SEND_SIGNALED;
			send_wr->sg_list = &qp->sg_list[j];
		} else {
			/* For receiver prepare pre-defined recv_wr */
			struct ibv_recv_wr *recv_wr = &qp->recv_wr[j];

			if (j % qp->wr_burst != qp->wr_burst - 1)
				recv_wr->next = &qp->recv_wr[j + 1];
			recv_wr->num_sge = 1;
			recv_wr->sg_list = &qp->sg_list[j];
		}
	}

	/* Query for QP burst-family if selected intf is not the normal one */
	intf_params->intf = IBV_EXP_INTF_QP_BURST;
	intf_params->obj = qp->qp;
	if (qp->qp_intf != NORMAL_RECV_INTF && qp->qp_intf != NORMAL_SEND_INTF) {
		qp->qp_burst_family = ibv_exp_query_intf(ctx->context, intf_params, &intf_status);
		if (!qp->qp_burst_family)  {
			fprintf(stderr, "Fail to query QP burst family (intf_status = %d)\n", intf_status);
			goto dereg_mr;
		}
	}

	return 0;

dereg_mr:
	ibv_dereg_mr(qp->mr);

free_buf:
	free(qp->buf);

destroy_qp:
	ibv_destroy_qp(qp->qp);

clean_qp:
	if (!qp->send_wr)
		free(qp->send_wr);
	if (!qp->recv_wr)
		free(qp->recv_wr);
	free(qp->sg_list);

	return 1;
}

static void destroy_qp(struct intf_context *ctx,
		       struct qp_cq_data *qps_cqs,
		       struct ibv_exp_release_intf_params *rel_intf)
{
	struct qp_data *qp = &qps_cqs->qp;

	if (qp->qp_burst_family)
		ibv_exp_release_intf(ctx->context, qp->qp_burst_family, rel_intf);

	ibv_dereg_mr(qp->mr);
	free(qp->buf);
	ibv_destroy_qp(qp->qp);
	if (!qp->send_wr)
		free(qp->send_wr);
	if (!qp->recv_wr)
		free(qp->recv_wr);
	if (!qp->sg_list)
		free(qp->sg_list);
}

int init_qps_cqs(struct intf_context *ctx)
{
	int i;
	struct ibv_exp_qp_init_attr qp_init_attr;
	struct ibv_exp_cq_init_attr cq_init_attr;
	struct ibv_exp_query_intf_params intf_params;
	struct ibv_exp_release_intf_params rel_intf_params;
	struct ibv_qp_attr qp_attr;

	memset(&qp_attr, 0, sizeof(qp_attr));
	memset(&cq_init_attr, 0, sizeof(cq_init_attr));
	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	memset(&intf_params, 0, sizeof(intf_params));
	memset(&rel_intf_params, 0, sizeof(rel_intf_params));

	qp_init_attr.comp_mask = IBV_EXP_QP_INIT_ATTR_PD		|
				 IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS	|
				 IBV_EXP_QP_INIT_ATTR_INL_RECV;
	if (ctx->use_res_domain) {
		cq_init_attr.comp_mask = IBV_EXP_CQ_INIT_ATTR_RES_DOMAIN;
		qp_init_attr.comp_mask |= IBV_EXP_QP_INIT_ATTR_RES_DOMAIN;
	}

	qp_attr.qp_state        = IBV_QPS_INIT;
	qp_attr.pkey_index      = 0;
	qp_attr.port_num        = ctx->ib_port_num;
	qp_attr.qp_access_flags = 0;

	intf_params.intf_scope = IBV_EXP_INTF_GLOBAL;

	for (i = 0; i < ctx->num_qps_cqs; i++) {
		if (create_cq(ctx, &ctx->qps_cqs[i], &cq_init_attr, &intf_params))
			goto clean_qps_cqs;

		if (create_qp(ctx, &ctx->qps_cqs[i], &qp_attr, &qp_init_attr, &intf_params)) {
			destroy_cq(ctx, &ctx->qps_cqs[i], &rel_intf_params);
			goto clean_qps_cqs;
		}
	}

	return 0;

clean_qps_cqs:
	for (; i > 0; i--) {
		destroy_cq(ctx, &ctx->qps_cqs[i], &rel_intf_params);
		destroy_qp(ctx, &ctx->qps_cqs[i], &rel_intf_params);
	}

	return 1;
}

int destroy_qps_cqs(struct intf_context *ctx)
{
	struct ibv_exp_release_intf_params rel_intf_params;
	int i;

	memset(&rel_intf_params, 0, sizeof(rel_intf_params));
	for (i = 0; i < ctx->num_qps_cqs; i++) {
		destroy_cq(ctx, &ctx->qps_cqs[i], &rel_intf_params);
		destroy_qp(ctx, &ctx->qps_cqs[i], &rel_intf_params);
	}

	return 0;
}

int init_res_domains(struct intf_context *ctx)
{
	struct ibv_exp_destroy_res_domain_attr dest_res_dom_attr;
	struct ibv_exp_res_domain_init_attr res_domain_attr;
	int i;

	res_domain_attr.comp_mask = IBV_EXP_RES_DOMAIN_THREAD_MODEL | IBV_EXP_RES_DOMAIN_MSG_MODEL;
	res_domain_attr.thread_model = IBV_EXP_THREAD_SINGLE;
	res_domain_attr.msg_model = IBV_EXP_MSG_HIGH_BW;


	/* Create resource domain per thread */
	for (i = 0; i < ctx->num_threads; i++) {
		ctx->threads[i].single_res_domain = ibv_exp_create_res_domain(ctx->context, &res_domain_attr);
		if (!ctx->threads[i].single_res_domain) {
			fprintf(stderr, "Can't create resource domain for thread %d errno = %d\n", i, errno);
			goto cleanup;
		}
	}

	return 0;

cleanup:
	dest_res_dom_attr.comp_mask = 0;
	for (; i > 0; i--)
		ibv_exp_destroy_res_domain(ctx->context, ctx->threads[i - 1].single_res_domain, &dest_res_dom_attr);

	return 1;
}

int clean_res_domains(struct intf_context *ctx)
{
	struct ibv_exp_destroy_res_domain_attr dest_res_dom_attr;
	int i;

	dest_res_dom_attr.comp_mask = 0;

	for (i = 0; i < ctx->num_threads; i++)
		ibv_exp_destroy_res_domain(ctx->context, ctx->threads[i].single_res_domain, &dest_res_dom_attr);

	return 0;
}

int create_resources(struct intf_context *ctx)
{
	struct ibv_device **dev_list;
	int i;

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get IB devices list");
		return 1;
	}

	if (!ctx->dev_name) {
		ctx->ib_dev = *dev_list;
		if (!ctx->ib_dev) {
			fprintf(stderr, "No IB devices found\n");
			return 1;
		}
	} else {
		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]), ctx->dev_name)) {
				ctx->ib_dev = dev_list[i];
				break;
			}
		if (!ctx->ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ctx->dev_name);
			return 1;
		}
	}

	ctx->context = ibv_open_device(ctx->ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ctx->ib_dev));
		return 1;
	}

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_device;
	}

	if (ctx->use_res_domain) {
		ctx->dattr.comp_mask = IBV_EXP_DEVICE_ATTR_CALC_CAP		|
				       IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS	|
				       IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ	|
				       IBV_EXP_DEVICE_ATTR_EXT_ATOMIC_ARGS	|
				       IBV_EXP_DEVICE_ATTR_MAX_CTX_RES_DOMAIN;

		if (ibv_exp_query_device(ctx->context, &ctx->dattr)) {
			fprintf(stderr, "Couldn't query device capabilities.\n");
			goto clean_pd;
		}
		if (!(ctx->dattr.comp_mask & IBV_EXP_DEVICE_ATTR_MAX_CTX_RES_DOMAIN)) {
			fprintf(stderr, "query-device failed to retrieve max_ctx_res_domain\n");
			goto clean_pd;
		}
		if (ctx->num_threads > ctx->dattr.max_ctx_res_domain) {
			fprintf(stderr, "can't allocate resource domain per thread, required=%d, available=%d\n",
				ctx->num_threads, ctx->dattr.max_ctx_res_domain);
			goto clean_pd;
		}
		if (init_res_domains(ctx))
			goto clean_pd;
	}

	if (init_qps_cqs(ctx))
		goto clean_res_doms;

	return 0;

clean_res_doms:
	if (ctx->use_res_domain)
		clean_res_domains(ctx);

clean_pd:
	ibv_dealloc_pd(ctx->pd);

clean_device:
	ibv_close_device(ctx->context);

	return 1;
}
void destroy_resources(struct intf_context *ctx)
{
	destroy_qps_cqs(ctx);
	if (ctx->use_res_domain)
		clean_res_domains(ctx);
	ibv_dealloc_pd(ctx->pd);
	ctx->pd = NULL;
	ibv_close_device(ctx->context);
	ctx->context = NULL;
}

/* defines the sender/receiver sides */
static int is_send(struct intf_input *input)
{
	if (strlen(input->server_data.name) > 0)
		return 0;

	return 1;
}

#define PS_MAX_CPUS 128
/*
 * create_context - Allocate and initialize application database
 * according to application input.
 */
int create_context(struct intf_context *ctx, struct intf_input *input)
{
	int cpu_idx;
	int qp_idx;
	int i, j;
	int cpu_array[PS_MAX_CPUS];

	/* Set context user name */
	if (strlen(input->server_data.name) > 0)
		ctx->servername = input->server_data.name;

	ctx->is_send = is_send(input);
	ctx->port = input->server_data.port;

	/* Define the number of application QPs/CQs (1 QP/CQ per threads) */
	ctx->num_qps_cqs = input->thread_prms.num_threads;

	/* Define the number of application CQs (num_qps) */
	ctx->check_data = input->ib_data.check_data && !ctx->is_send;
	ctx->use_res_domain = input->ib_data.use_res_domain;
	ctx->num_threads = input->thread_prms.num_threads;
	cpu_idx = 0;

	/* create array of CPUs which application threads my run on */
	for (i = 0; i < input->thread_prms.num_cpu_sets; i++)
		for (j = input->thread_prms.cpu_sets[i].min; j <= input->thread_prms.cpu_sets[i].max; j++)
			if (cpu_idx < PS_MAX_CPUS)
				cpu_array[cpu_idx++] = j;
			else
				fprintf(stderr, "Supporting up to %d cpus (ignoring some of requested cpus)\n", PS_MAX_CPUS);

	ctx->ib_port_num = input->ib_data.ib_port_num;
	ctx->sl = input->ib_data.sl;
	ctx->mtu = input->ib_data.mtu;
	if (strlen(input->ib_data.dev_name) > 0 && strlen(input->ib_data.dev_name) < MAX_DEV_NAME_SIZE - 1)
		strcpy(ctx->dev_name, input->ib_data.dev_name);

	/* Allocate qps, cqs and threads arrays */
	ctx->qps_cqs = calloc(1, ctx->num_qps_cqs * sizeof(*ctx->qps_cqs));
	if (!ctx->qps_cqs)
		return 1;

	ctx->threads = calloc(1, ctx->num_threads * sizeof(*ctx->threads));
	if (!ctx->threads)
		goto free_qps_cqs;

	/* Update QPs ad CQs data */
	for (i = 0; i < ctx->num_qps_cqs; i++) {
		struct qp_cq_data *qp_cq_data = &ctx->qps_cqs[i];
		struct qp_data *qp_data = &qp_cq_data->qp;

		/* Update QP data */
		qp_cq_data->idx = i;
		qp_data->max_inl_recv_data = input->qp_prms.max_inl_recv_data;
		qp_data->msg_size = input->send_prms.msg_size;
		qp_data->num_msgs = input->send_prms.num_qp_msgs;
		qp_data->wr_burst = input->qp_prms.wr_burst;
		qp_data->max_inline_data = 0;

		if (ctx->is_send) {
			qp_data->qp_intf = send_2_qp[input->qp_prms.verbs_send_intf];
			qp_cq_data->cq.cq_intf = poll_2_cq[input->qp_prms.verbs_send_poll_intf];
			qp_data->max_wrs = input->qp_prms.max_send_wr;

			/* If SEND_PENDING_INL interface selected enable inline data */
			if (qp_data->qp_intf == ACC_SEND_PENDING_INL_INTF)
				qp_data->max_inline_data = input->send_prms.msg_size;
		} else {
			qp_data->qp_intf = recv_2_qp[input->qp_prms.verbs_recv_intf];
			qp_cq_data->cq.cq_intf = poll_2_cq[input->qp_prms.verbs_recv_poll_intf];

			/* If inline received supported chose the ACC_POLL_LENGTH_INL_INTF
			 * interface instead of ACC_POLL_LENGTH_INTF
			 */
			if (qp_cq_data->qp.max_inl_recv_data &&
			    qp_cq_data->cq.cq_intf == ACC_POLL_LENGTH_INTF)
				qp_cq_data->cq.cq_intf = ACC_POLL_LENGTH_INL_INTF;
			qp_data->max_wrs = input->qp_prms.max_recv_wr;
		}
		qp_cq_data->cq.cq_size = qp_data->max_wrs;
		qp_cq_data->cq.wc_burst = input->qp_prms.wr_burst;
	}

	/* Update threads data */
	for (i = 0, qp_idx = 0; i < ctx->num_threads; i++, qp_idx ++) {
		/* Update the range of QPs used by each thread */
		ctx->threads[i].qp_idx = qp_idx;
		/* Update the thread cpu */
		ctx->threads[i].cpu = cpu_array[i % cpu_idx];
	}

	return 0;

free_qps_cqs:
	free(ctx->qps_cqs);

	return 1;
}

void destroy_context(struct intf_context *ctx)
{
	free(ctx->qps_cqs);
	free(ctx->threads);
}

static int client_exch_dest(const char *servername, int port,
			    const struct ib_dest *my_dest,
			    struct ib_dest *rem_dest, int num_qps)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof("0000:00000000000000000000000000000000")];
	char qp_msg[sizeof("000000:000000")];
	int n;
	int sockfd = -1;
	char gid[33];
	int i;

	if (asprintf(&service, "%d", port) < 0)
		return 1;

	n = getaddrinfo(servername, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
		free(service);
		return 1;
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
		return 1;
	}

	gid_to_wire_gid(&my_dest->gid, gid);
	sprintf(msg, "%04x:%s", my_dest->lid, gid);
	if (write(sockfd, msg, sizeof(msg)) != sizeof(msg)) {
		fprintf(stderr, "Couldn't send local address\n");
		goto out;
	}

	if (recv(sockfd, msg, sizeof(msg), MSG_WAITALL) != sizeof(msg)) {
		perror("client read");
		fprintf(stderr, "Couldn't read remote address\n");
		goto out;
	}

	if (sscanf(msg, "%x:%s", &rem_dest->lid, gid) != 2)
		goto out;
	wire_gid_to_gid(gid, &rem_dest->gid);

	for (i = 0; i < num_qps; i++) {
		sprintf(qp_msg, "%06x:%06x", my_dest->qpn[i], my_dest->psn[i]);
		if (write(sockfd, qp_msg, sizeof(qp_msg)) != sizeof(qp_msg)) {
			fprintf(stderr, "Couldn't send local qp[%d] data\n", i);
			goto out;
		}

		if (recv(sockfd, qp_msg, sizeof(qp_msg), MSG_WAITALL) != sizeof(qp_msg)) {
			perror("client read");
			fprintf(stderr, "Couldn't read remote qp[%d] data\n", i);
			goto out;
		}
		if (sscanf(qp_msg, "%x:%x", &rem_dest->qpn[i], &rem_dest->psn[i]) != 2)
			goto out;
	}

	if (write(sockfd, "done", sizeof("done")) != sizeof("done")) {
		fprintf(stderr, "Couldn't send \"done\" msg\n");
		goto out;
	}

	close(sockfd);

	return 0;


out:
	close(sockfd);

	return 1;
}

static int server_exch_dest(struct intf_context *ctx,
			    int ib_port, enum ibv_mtu mtu,
			    int port, int sl,
			    const struct ib_dest *my_dest,
			    struct ib_dest *rem_dest,
			    int sgid_idx, int num_qps)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_flags    = AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof("0000:00000000000000000000000000000000")];
	char qp_msg[sizeof("000000:000000")];
	int n;
	int sockfd = -1, connfd;
	char gid[33];
	int i;

	if (asprintf(&service, "%d", port) < 0)
		return 1;

	n = getaddrinfo(NULL, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for port %d\n", gai_strerror(n), port);
		free(service);
		return 1;
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
		return 1;
	}

	listen(sockfd, 1);
	connfd = accept(sockfd, NULL, 0);
	close(sockfd);
	if (connfd < 0) {
		fprintf(stderr, "accept() failed\n");
		return 1;
	}

	n = recv(connfd, msg, sizeof(msg), MSG_WAITALL);
	if (n != sizeof(msg)) {
		perror("server read");
		fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int) sizeof(msg));
		goto out;
	}

	if (sscanf(msg, "%x:%s", &rem_dest->lid, gid) != 2)
		goto out;
	wire_gid_to_gid(gid, &rem_dest->gid);


	gid_to_wire_gid(&my_dest->gid, gid);
	sprintf(msg, "%04x:%s", my_dest->lid, gid);
	if (write(connfd, msg, sizeof(msg)) != sizeof(msg)) {
		fprintf(stderr, "Couldn't send local address\n");
		goto out;
	}

	for (i = 0; i < num_qps; i++) {
		if (recv(connfd, qp_msg, sizeof(qp_msg), MSG_WAITALL) != sizeof(qp_msg)) {
			perror("client read");
			fprintf(stderr, "Couldn't read remote qp[%d] data\n", i);
			goto out;
		}
		if (sscanf(qp_msg, "%x:%x", &rem_dest->qpn[i], &rem_dest->psn[i]) != 2)
			goto out;

		sprintf(qp_msg, "%06x:%06x", my_dest->qpn[i], my_dest->psn[i]);
		if (write(connfd, qp_msg, sizeof(qp_msg)) != sizeof(qp_msg)) {
			fprintf(stderr, "Couldn't send local qp[%d] data\n", i);
			goto out;
		}
	}


	/* expecting "done" msg */
	if (read(connfd, msg, sizeof(msg)) <= 0) {
		fprintf(stderr, "Couldn't read \"done\" msg\n");
		goto out;
	}

	close(connfd);

	return 0;

out:
	close(connfd);

	return 1;
}


int exchange_remote_data(struct intf_context *ctx)
{
	int i;
	struct ib_dest my_dest;
	struct ib_dest rem_dest;
	char gid[INET6_ADDRSTRLEN];
	struct ibv_port_attr portinfo;

	if (ibv_query_port(ctx->context, ctx->ib_port_num, &portinfo)) {
		fprintf(stderr, "Couldn't get port info\n");
		return 1;
	}

	if (portinfo.link_layer != IBV_LINK_LAYER_INFINIBAND) {
		fprintf(stderr, "link_layer != IBV_LINK_LAYER_INFINIBAND\n");
		return 1;
	}

	my_dest.lid = portinfo.lid;
	if (!my_dest.lid) {
		fprintf(stderr, "Couldn't get local LID\n");
		return 1;
	}

	memset(&my_dest.gid, 0, sizeof(my_dest.gid));

	my_dest.psn = calloc(1, sizeof(*my_dest.psn) * ctx->num_qps_cqs);
	my_dest.qpn = calloc(1, sizeof(*my_dest.qpn) * ctx->num_qps_cqs);
	rem_dest.psn = calloc(1, sizeof(*my_dest.psn) * ctx->num_qps_cqs);
	rem_dest.qpn = calloc(1, sizeof(*my_dest.qpn) * ctx->num_qps_cqs);

	if (!my_dest.psn || !my_dest.qpn || !rem_dest.psn || !rem_dest.qpn)
		goto free_buffs;

	for (i = 0; i < ctx->num_qps_cqs; i++) {
		my_dest.qpn[i] = ctx->qps_cqs[i].qp.qp->qp_num;
		my_dest.psn[i] = ctx->qps_cqs[i].qp.psn;
	}
	inet_ntop(AF_INET6, &my_dest.gid, gid, sizeof(gid));
	printf("  local address:  LID 0x%04x, GID %s\n", my_dest.lid, gid);

	if (ctx->servername) {
		if (client_exch_dest(ctx->servername, ctx->port,
				     &my_dest, &rem_dest, ctx->num_qps_cqs)) {
			fprintf(stderr, "Couldn't get remote LID\n");
			goto free_buffs;
		}
	} else {
		if (server_exch_dest(ctx, ctx->ib_port_num, ctx->mtu, ctx->port, ctx->sl,
				     &my_dest, &rem_dest, 0, ctx->num_qps_cqs)) {
			fprintf(stderr, "Couldn't get remote LID\n");
			goto free_buffs;
		}
	}

	for (i = 0; i < ctx->num_qps_cqs; i++) {
		ctx->qps_cqs[i].qp.remote_qpn = rem_dest.qpn[i];
		if (connect_qp(ctx->qps_cqs[i].qp.qp, ctx->ib_port_num, my_dest.psn[i], ctx->mtu, ctx->sl,
			       rem_dest.gid, rem_dest.lid, rem_dest.psn[i],
			       rem_dest.qpn[i], 0)) {
			fprintf(stderr, "Couldn't connect to remote qp[%d]\n", i);
			goto free_buffs;
		}
	}

	return 0;

free_buffs:
	if (my_dest.psn)
		free(my_dest.psn);
	if (my_dest.qpn)
		free(my_dest.qpn);
	if (rem_dest.psn)
		free(rem_dest.psn);
	if (rem_dest.qpn)
		free(rem_dest.qpn);

	return 1;
}

static inline enum ibv_mtu mtu_to_enum(int mtu)
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

static inline int enum_to_mtu(enum ibv_mtu mtu)
{
	switch (mtu) {
	case IBV_MTU_256:  return 256;
	case IBV_MTU_512:  return 512;
	case IBV_MTU_1024: return 1024;
	case IBV_MTU_2048: return 2048;
	case IBV_MTU_4096: return 4096;
	default:   return -1;
	}
}

static void usage(const char *argv0, struct intf_input *default_input)
{
	int i;

	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n", argv0);
	printf("  %s <host>     connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>         listen on/connect to port <port> (default %d)\n", default_input->server_data.port);
	printf("  -d, --ib-dev=<dev>        use IB device <dev> (default %s)\n", default_input->ib_data.dev_name);
	printf("  -i, --ib-port=<port>      use port <port> of IB device (default %d)\n", default_input->ib_data.ib_port_num);
	printf("  -s, --size=<size>         size of message (default %'d max size %'d)\n", default_input->send_prms.msg_size, MAX_MSG_SIZE);
	printf("  -m, --mtu=<size>          path MTU (default %'d)\n", enum_to_mtu(default_input->ib_data.mtu));
	printf("  -r, --rx-depth=<dep>      receive queue size (default %'d)\n", default_input->qp_prms.max_recv_wr);
	printf("  -n, --iters=<iters>       number of messages (default %'d)\n", default_input->send_prms.num_qp_msgs);
	printf("  -l, --sl=<sl>             service level value (default %d)\n", default_input->ib_data.sl);
	printf("  -t, --inline-recv=<size>  size of inline-recv (default %d)\n", default_input->qp_prms.max_inl_recv_data);
	printf("  -S, --send-verb=<verbs>   send verb interface to use S_NORM/S_PEND/S_PEND_INL/S_PEND_SG_LIST/S_BURST (default %s)\n",
			send_enum_to_verbs_intf_str(default_input->qp_prms.verbs_send_intf));
	printf("  -R, --recv-verb=<verbs>   recv verb interface to use R_NORM/R_BURST (default %s)\n",
			recv_enum_to_verbs_intf_str(default_input->qp_prms.verbs_recv_intf));
	printf("  -P, --poll-verb=<verbs>   poll verb interface to use P_NORM/P_CNT/P_LEN (default send: %s recv: %s)\n",
			poll_enum_to_verbs_intf_str(default_input->qp_prms.verbs_send_poll_intf),
			poll_enum_to_verbs_intf_str(default_input->qp_prms.verbs_recv_poll_intf));
	printf("  -c, --cpus-list=<list>    CPUs list to run on (default ");
	for (i = 0; i < default_input->thread_prms.num_cpu_sets; i++) {
		printf("[%d..%d]", default_input->thread_prms.cpu_sets[i].min, default_input->thread_prms.cpu_sets[i].max);
		if (i + 1 == default_input->thread_prms.num_cpu_sets)
			printf(")\n");
		else
			printf(",");
	}
	printf("  -b, --burst=<size>        size of send/recv wr burst (default %'d)\n", default_input->qp_prms.wr_burst);
	printf("  -T, --num-threads=<num>   Number of threads to run (default %'d)\n", default_input->thread_prms.num_threads);
	printf("  -C, --check-data          check the data received (default no-checks)\n");
	printf("  -A, --avoid-res-domain    avoid usage of resource domain (default use res-domain)\n");
}

int str_to_cpu_set(char *str, int *num_cpus, struct cpu_set *cpu_sets)
{
	char *p;
	char *t;
	int min, max;
	char stmp[64];
	int ncpus = 0;

	if (strlen(str) >= 64)
		return 1;

	strcpy(stmp, str);
	p = stmp;

	while (ncpus < MAX_CPU_SETS && p && strlen(p)) {
		t = strchr(p, ']');
		if (t) {
			t++;
			if (strlen(t)) {
				*t = 0;
				t++;
			}
		}

		if (sscanf(p, "[%d..%d]", &min, &max) != 2)
			return 1;
		p = t;

		if (min > max)
			return 1;

		cpu_sets[ncpus].min = min;
		cpu_sets[ncpus].max = max;
		ncpus++;
	}

	if (!ncpus)
		return 1;

	*num_cpus = ncpus;

	return 0;
}

/*
 * parse_input - Create input data for the test based on the
 * default_input and application parameters
 */
int parse_input(struct intf_input *input, struct intf_input *default_input, int argc, char *argv[])
{
	int tmp;
	enum ibv_mtu mtu;
	char *ib_devname = NULL;
	char *vrbs_intf = NULL;
	char *cpus_str = NULL;
	unsigned long long size;

	memcpy(input, default_input, sizeof(*input));

	while (1) {
		int c;

		static struct option long_options[] = {
			{ .name = "port",             .has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",           .has_arg = 1, .val = 'd' },
			{ .name = "ib-port",          .has_arg = 1, .val = 'i' },
			{ .name = "size",             .has_arg = 1, .val = 's' },
			{ .name = "mtu",              .has_arg = 1, .val = 'm' },
			{ .name = "rx-depth",         .has_arg = 1, .val = 'r' },
			{ .name = "iters",            .has_arg = 1, .val = 'n' },
			{ .name = "sl",               .has_arg = 1, .val = 'l' },
			{ .name = "inline-recv",      .has_arg = 1, .val = 't' },
			{ .name = "send-verb",        .has_arg = 1, .val = 'S' },
			{ .name = "recv-verb",        .has_arg = 1, .val = 'R' },
			{ .name = "poll-verb",        .has_arg = 1, .val = 'P' },
			{ .name = "cpus-list",        .has_arg = 1, .val = 'c' },
			{ .name = "burst",            .has_arg = 1, .val = 'b' },
			{ .name = "num-threads",      .has_arg = 1, .val = 'T' },
			{ .name = "check-data",       .has_arg = 0, .val = 'C' },
			{ .name = "avoid-res-domain", .has_arg = 0, .val = 'A' },
			{ 0 }
		};

		c = getopt_long(argc, argv, "p:d:i:s:m:r:n:l:t:c:S:R:P:b:T:CA",
				long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			tmp = strtol(optarg, NULL, 0);
			if (tmp < 0 || tmp > 65535)
				goto print_usage;
			input->server_data.port = tmp;

			break;

		case 'd':
			ib_devname = strdupa(optarg);
			if (strlen(ib_devname) >= MAX_DEV_NAME_SIZE - 1) {
				fprintf(stderr, "Device name too long (max %d)\n", MAX_DEV_NAME_SIZE - 1);
				goto print_usage;
			}

			strcpy(input->ib_data.dev_name, ib_devname);
			break;

		case 'i':
			tmp = strtol(optarg, NULL, 0);
			if (tmp < 0)
				goto print_usage;
			input->ib_data.ib_port_num = tmp;
			break;

		case 's':
			size = strtoll(optarg, NULL, 0);

			if (size < 0 || size > MAX_MSG_SIZE)
				goto print_usage;

			input->send_prms.msg_size = size;
			break;

		case 'm':
			mtu = mtu_to_enum(strtol(optarg, NULL, 0));
			if (mtu < 0)
				goto print_usage;
			input->ib_data.mtu = mtu;
			break;

		case 'r':
			tmp = strtol(optarg, NULL, 0);
			input->qp_prms.max_recv_wr = tmp;
			break;

		case 'n':
			tmp = strtol(optarg, NULL, 0);
			input->send_prms.num_qp_msgs = tmp;
			break;

		case 'l':
			tmp = strtol(optarg, NULL, 0);
			input->ib_data.sl = tmp;
			break;

		case 't':
			tmp = strtol(optarg, NULL, 0);
			input->qp_prms.max_inl_recv_data = tmp;
			if (input->qp_prms.max_inl_recv_data > MAX_INLINE_RECV) {
				fprintf(stderr, "Max allowed inline-recv = %d\n", MAX_INLINE_RECV);
				goto print_usage;
			}
			break;

		case 'S':
			vrbs_intf = strdupa(optarg);
			if (!strcmp(vrbs_intf, "S_NORM")) {
				input->qp_prms.verbs_send_intf = IN_NORMAL_SEND_INTF;
			} else if (!strcmp(vrbs_intf, "S_PEND")) {
				input->qp_prms.verbs_send_intf = IN_ACC_SEND_PENDING_INTF;
			} else if (!strcmp(vrbs_intf, "S_PEND_INL")) {
				input->qp_prms.verbs_send_intf = IN_ACC_SEND_PENDING_INL_INTF;
			} else if (!strcmp(vrbs_intf, "S_PEND_SG_LIST")) {
				input->qp_prms.verbs_send_intf = IN_ACC_SEND_PENDING_SG_LIST_INTF;
			} else if (!strcmp(vrbs_intf, "S_BURST")) {
				input->qp_prms.verbs_send_intf = IN_ACC_SEND_BURST_INTF;
			} else {
				fprintf(stderr, "Send interface name %s not supported\n", vrbs_intf);
				goto print_usage;
			}
			break;

		case 'R':
			vrbs_intf = strdupa(optarg);
			if (!strcmp(vrbs_intf, "R_NORM")) {
				input->qp_prms.verbs_recv_intf = IN_NORMAL_RECV_INTF;
			} else if (!strcmp(vrbs_intf, "R_BURST")) {
				input->qp_prms.verbs_recv_intf = IN_ACC_RECV_BURST_INTF;
			} else {
				fprintf(stderr, "Receive interface name %s not supported\n", vrbs_intf);
				goto print_usage;
			}
			break;

		case 'P':
			vrbs_intf = strdupa(optarg);
			if (!strcmp(vrbs_intf, "P_NORM")) {
				input->qp_prms.verbs_recv_poll_intf = IN_NORMAL_POLL_INTF;
				input->qp_prms.verbs_send_poll_intf = IN_NORMAL_POLL_INTF;
			} else if (!strcmp(vrbs_intf, "P_CNT")) {
				input->qp_prms.verbs_recv_poll_intf = IN_ACC_POLL_CNT_INTF;
				input->qp_prms.verbs_send_poll_intf = IN_ACC_POLL_CNT_INTF;
			} else if (!strcmp(vrbs_intf, "P_LEN")) {
				input->qp_prms.verbs_recv_poll_intf = IN_ACC_POLL_LENGTH_INTF;
				input->qp_prms.verbs_send_poll_intf = IN_ACC_POLL_LENGTH_INTF;
			} else {
				fprintf(stderr, "Poll interface name %s not supported\n", vrbs_intf);
				goto print_usage;
			}
			break;

		case 'c':
			cpus_str = strdupa(optarg);
			if (str_to_cpu_set(cpus_str, &input->thread_prms.num_cpu_sets, input->thread_prms.cpu_sets)) {
					fprintf(stderr, "Wrong cpus list: %s\n", cpus_str);
					goto print_usage;
			}
			break;

		case 'b':
			tmp = strtol(optarg, NULL, 0);
			if (tmp < 0 || tmp > 65535)
				goto print_usage;

			input->qp_prms.wr_burst = tmp;
			break;

		case 'T':
			tmp = strtol(optarg, NULL, 0);
			if (tmp < 0 || tmp > 65535)
				goto print_usage;

			input->thread_prms.num_threads = tmp;
			break;

		case 'C':
			input->ib_data.check_data = 1;
			break;

		case 'A':
			input->ib_data.use_res_domain = 0;
			break;

		default:
			goto print_usage;
		}
	}

	if (optind == argc - 1) {
		if (strlen(argv[optind]) > 0 && strlen(argv[optind]) < MAX_SERVER_NAME_SIZE - 1)
			strcpy(input->server_data.name, argv[optind]);
	} else if (optind < argc) {
		goto print_usage;
	}

	if (is_send(input) && input->qp_prms.wr_burst * 2 >= input->qp_prms.max_send_wr) {
		fprintf(stderr, "Invalid input, max_send_wr(%d) should be at least twice the size of burst size(%d)\n",
			input->qp_prms.max_send_wr, input->qp_prms.wr_burst);
		return 1;
	}

	if (!is_send(input) && input->qp_prms.wr_burst * 2 >= input->qp_prms.max_recv_wr) {
		fprintf(stderr, "Invalid input, max_recv_wr(%d) should be at least twice the size of burst size(%d)\n",
			input->qp_prms.max_recv_wr, input->qp_prms.wr_burst);
		return 1;
	}

	if (is_send(input) && input->qp_prms.max_send_wr % input->qp_prms.wr_burst) {
		fprintf(stderr, "Invalid input modulo(max_send_wr(%d), burst size(%d)) != 0\n",
			input->qp_prms.max_send_wr, input->qp_prms.wr_burst);
		return 1;
	}

	if (!is_send(input) && input->qp_prms.max_recv_wr % input->qp_prms.wr_burst) {
		fprintf(stderr, "Invalid input modulo(max_recv_wr(%d), burst size(%d)) != 0\n",
			input->qp_prms.max_recv_wr, input->qp_prms.wr_burst);
		return 1;
	}

	/* We can't use IN_ACC_POLL_LENGTH_INTF to poll for send messages completion */
	if (is_send(input) && input->qp_prms.verbs_send_poll_intf == IN_ACC_POLL_LENGTH_INTF) {
		fprintf(stderr, "It is not allowed to use poll-length(P_LEN) for send messages.\n");
		fprintf(stderr, "Use -P P_CNT or -P P_NORM options to poll for send messages completion\n");
		return 1;
	}

	return 0;

print_usage:
	usage(argv[0], default_input);
	return 1;
}

int main(int argc, char *argv[])
{
	struct intf_context *ctx;
	int ret = 0;

	setlocale(LC_NUMERIC, "");
	srand48(getpid() * time(NULL));

	if (parse_input(&intf_input, &intf_default_input, argc, argv)) {
		fprintf(stderr, "Failed to update and validate test inputs\n");
		return 1;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx)
		return 1;

	if (create_context(ctx, &intf_input)) {
		fprintf(stderr, "Failed to create test context\n");
		ret = 1;
		goto free_ctx;
	}

	if (create_resources(ctx)) {
		fprintf(stderr, "Failed to create test resources\n");
		ret = 1;
		goto destroy_context;
	}

	if (exchange_remote_data(ctx)) {
		fprintf(stderr, "Failed to create test context and resources\n");
		ret = 1;
		goto destroy_resources;
	}

	if (run_threads(ctx)) {
		fprintf(stderr, "Failed in test execution\n");
		ret = 1;
		goto destroy_resources;
	}

destroy_resources:
	destroy_resources(ctx);

destroy_context:
	destroy_context(ctx);

free_ctx:
	free(ctx);

	return ret;
}
