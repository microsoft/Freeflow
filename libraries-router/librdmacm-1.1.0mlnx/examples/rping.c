/*
 * Copyright (c) 2005 Ammasso, Inc. All rights reserved.
 * Copyright (c) 2006 Open Grid Computing, Inc. All rights reserved.
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
 */

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <byteswap.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <inttypes.h>

#include <rdma/rdma_cma.h>
#include <infiniband/arch.h>

static int debug = 0;
#define DEBUG_LOG if (debug) printf

/*
 * rping "ping/pong" loop:
 * 	client sends source rkey/addr/len
 *	server receives source rkey/add/len
 *	server rdma reads "ping" data from source
 * 	server sends "go ahead" on rdma read completion
 *	client sends sink rkey/addr/len
 * 	server receives sink rkey/addr/len
 * 	server rdma writes "pong" data to sink
 * 	server sends "go ahead" on rdma write completion
 * 	<repeat loop>
 */

/*
 * These states are used to signal events between the completion handler
 * and the main client or server thread.
 *
 * Once CONNECTED, they cycle through RDMA_READ_ADV, RDMA_WRITE_ADV, 
 * and RDMA_WRITE_COMPLETE for each ping.
 */

static uint8_t type_mw;

enum test_state {
	IDLE = 1,
	CONNECT_REQUEST,
	ADDR_RESOLVED,
	ROUTE_RESOLVED,
	CONNECTED,
	RDMA_READ_ADV,
	RDMA_READ_COMPLETE,
	RDMA_WRITE_ADV,
	RDMA_WRITE_COMPLETE,
	DISCONNECTED,
	ERROR
};

struct rping_rdma_info {
	uint64_t buf;
	uint32_t rkey;
	uint32_t size;
};

#define RPING_BIND_WRID 1

/*
 * Default max buffer size for IO...
 */
#define RPING_BUFSIZE 64*1024
#define RPING_SQ_DEPTH 16

/* Default string for print data and
 * minimum buffer size
 */
#define _stringify( _x ) # _x
#define stringify( _x ) _stringify( _x )

#define RPING_MSG_FMT           "rdma-ping-%d: "
#define RPING_MIN_BUFSIZE       sizeof(stringify(INT_MAX)) + sizeof(RPING_MSG_FMT)

/*
 * Control block struct.
 */
struct rping_cb {
	int server;			/* 0 iff client */
	pthread_t cqthread;
	pthread_t persistent_server_thread;
	struct ibv_comp_channel *channel;
	struct ibv_cq *cq;
	struct ibv_pd *pd;
	struct ibv_qp *qp;

	struct ibv_recv_wr rq_wr;	/* recv work request record */
	struct ibv_sge recv_sgl;	/* recv single SGE */
	struct rping_rdma_info recv_buf;/* malloc'd buffer */
	struct ibv_mr *recv_mr;		/* MR associated with this buffer */

	struct ibv_send_wr sq_wr;	/* send work request record */
	struct ibv_sge send_sgl;
	struct rping_rdma_info send_buf;/* single send buf */
	struct ibv_mr *send_mr;

	struct ibv_send_wr rdma_sq_wr;	/* rdma work request record */
	struct ibv_sge rdma_sgl;	/* rdma single SGE */
	char *rdma_buf;			/* used as rdma sink */
	struct ibv_mr *rdma_mr;
	struct ibv_mw *rdma_mw;

	uint32_t remote_rkey;		/* remote guys RKEY */
	uint64_t remote_addr;		/* remote guys TO */
	uint32_t remote_len;		/* remote guys LEN */

	char *start_buf;		/* rdma read src */
	struct ibv_mr *start_mr;
	struct ibv_mw *start_mw;

	enum test_state state;		/* used for cond/signalling */
	sem_t sem;

	struct sockaddr_storage sin;
	uint16_t port;			/* dst port in NBO */
	int verbose;			/* verbose logging */
	int count;			/* ping count */
	int size;			/* ping data size */
	int validate;			/* validate ping data */
	int check_mw;			/* whether to use MW or not */

	/* CM stuff */
	pthread_t cmthread;
	struct rdma_event_channel *cm_channel;
	struct rdma_cm_id *cm_id;	/* connection on client side,*/
					/* listener on service side. */
	struct rdma_cm_id *child_cm_id;	/* connection on server side */
};

static int rping_cma_event_handler(struct rdma_cm_id *cma_id,
				    struct rdma_cm_event *event)
{
	int ret = 0;
	struct rping_cb *cb = cma_id->context;

	DEBUG_LOG("cma_event type %s cma_id %p (%s)\n",
		  rdma_event_str(event->event), cma_id,
		  (cma_id == cb->cm_id) ? "parent" : "child");

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		cb->state = ADDR_RESOLVED;
		ret = rdma_resolve_route(cma_id, 2000);
		if (ret) {
			cb->state = ERROR;
			perror("rdma_resolve_route");
			sem_post(&cb->sem);
		}
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		cb->state = ROUTE_RESOLVED;
		sem_post(&cb->sem);
		break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
		cb->state = CONNECT_REQUEST;
		cb->child_cm_id = cma_id;
		DEBUG_LOG("child cma %p\n", cb->child_cm_id);
		sem_post(&cb->sem);
		break;

	case RDMA_CM_EVENT_ESTABLISHED:
		DEBUG_LOG("ESTABLISHED\n");

		/*
		 * Server will wake up when first RECV completes.
		 */
		if (!cb->server) {
			cb->state = CONNECTED;
		}
		sem_post(&cb->sem);
		break;

	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
	case RDMA_CM_EVENT_REJECTED:
		fprintf(stderr, "cma event %s, error %d\n",
			rdma_event_str(event->event), event->status);
		sem_post(&cb->sem);
		ret = -1;
		break;

	case RDMA_CM_EVENT_DISCONNECTED:
		fprintf(stderr, "%s DISCONNECT EVENT...\n",
			cb->server ? "server" : "client");
		cb->state = DISCONNECTED;
		sem_post(&cb->sem);
		break;

	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		fprintf(stderr, "cma detected device removal!!!!\n");
		ret = -1;
		break;

	default:
		fprintf(stderr, "unhandled event: %s, ignoring\n",
			rdma_event_str(event->event));
		break;
	}

	return ret;
}

static int server_recv(struct rping_cb *cb, struct ibv_exp_wc *wc)
{
	if (wc->byte_len != sizeof(cb->recv_buf)) {
		fprintf(stderr, "Received bogus data, size %d\n", wc->byte_len);
		return -1;
	}

	cb->remote_rkey = ntohl(cb->recv_buf.rkey);
	cb->remote_addr = ntohll(cb->recv_buf.buf);
	cb->remote_len  = ntohl(cb->recv_buf.size);
	DEBUG_LOG("Received rkey %x addr %" PRIx64 " len %d from peer\n",
		  cb->remote_rkey, cb->remote_addr, cb->remote_len);

	if (cb->state <= CONNECTED || cb->state == RDMA_WRITE_COMPLETE)
		cb->state = RDMA_READ_ADV;
	else
		cb->state = RDMA_WRITE_ADV;

	return 0;
}

static int client_recv(struct rping_cb *cb, struct ibv_exp_wc *wc)
{
	if (wc->byte_len != sizeof(cb->recv_buf)) {
		fprintf(stderr, "Received bogus data, size %d\n", wc->byte_len);
		return -1;
	}

	if (cb->state == RDMA_READ_ADV)
		cb->state = RDMA_WRITE_ADV;
	else
		cb->state = RDMA_WRITE_COMPLETE;

	return 0;
}

static int rping_cq_event_handler(struct rping_cb *cb)
{
	struct ibv_exp_wc wc;
	struct ibv_recv_wr *bad_wr;
	int ret;
	int flushed = 0;

	while ((ret = ibv_exp_poll_cq(cb->cq, 1, &wc, sizeof(wc))) == 1) {
		ret = 0;

		if (wc.status) {
			if (wc.status == IBV_WC_WR_FLUSH_ERR) {
				flushed = 1;
				continue;

			}
			fprintf(stderr,
				"cq completion failed status %d\n",
				wc.status);
			ret = -1;
			goto error;
		}

		switch (wc.exp_opcode) {
		case IBV_EXP_WC_SEND:
			DEBUG_LOG("send completion\n");
			break;

		case IBV_EXP_WC_RDMA_WRITE:
			DEBUG_LOG("rdma write completion\n");
			cb->state = RDMA_WRITE_COMPLETE;
			sem_post(&cb->sem);
			break;

		case IBV_EXP_WC_RDMA_READ:
			DEBUG_LOG("rdma read completion\n");
			cb->state = RDMA_READ_COMPLETE;
			sem_post(&cb->sem);
			break;

		case IBV_EXP_WC_RECV:
			DEBUG_LOG("recv completion\n");
			ret = cb->server ? server_recv(cb, &wc) :
					   client_recv(cb, &wc);
			if (ret) {
				fprintf(stderr, "recv wc error: %d\n", ret);
				goto error;
			}

			ret = ibv_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
			if (ret) {
				fprintf(stderr, "post recv error: %d\n", ret);
				goto error;
			}
			sem_post(&cb->sem);
			break;

		case IBV_EXP_WC_BIND_MW:
			DEBUG_LOG("bind completion\n");
			break;
		case IBV_EXP_WC_LOCAL_INV:
			DEBUG_LOG("local inv completion\n");
			break;

		default:
			DEBUG_LOG("unknown!!!!! completion\n");
			ret = -1;
			goto error;
		}
	}
	if (ret) {
		fprintf(stderr, "poll error %d\n", ret);
		goto error;
	}
	return flushed;

error:
	cb->state = ERROR;
	sem_post(&cb->sem);
	return ret;
}

static int rping_accept(struct rping_cb *cb)
{
	int ret;

	DEBUG_LOG("accepting client connection request\n");

	ret = rdma_accept(cb->child_cm_id, NULL);
	if (ret) {
		perror("rdma_accept");
		return ret;
	}

	sem_wait(&cb->sem);
	if (cb->state == ERROR) {
		fprintf(stderr, "wait for CONNECTED state %d\n", cb->state);
		return -1;
	}
	return 0;
}

static void rping_setup_wr(struct rping_cb *cb)
{
	cb->recv_sgl.addr = (uint64_t) (unsigned long) &cb->recv_buf;
	cb->recv_sgl.length = sizeof cb->recv_buf;
	cb->recv_sgl.lkey = cb->recv_mr->lkey;
	cb->rq_wr.sg_list = &cb->recv_sgl;
	cb->rq_wr.num_sge = 1;

	cb->send_sgl.addr = (uint64_t) (unsigned long) &cb->send_buf;
	cb->send_sgl.length = sizeof cb->send_buf;
	cb->send_sgl.lkey = cb->send_mr->lkey;

	cb->sq_wr.opcode = IBV_WR_SEND;
	cb->sq_wr.send_flags = IBV_SEND_SIGNALED;
	cb->sq_wr.sg_list = &cb->send_sgl;
	cb->sq_wr.num_sge = 1;

	cb->rdma_sgl.addr = (uint64_t) (unsigned long) cb->rdma_buf;
	cb->rdma_sgl.lkey = cb->rdma_mr->lkey;
	cb->rdma_sq_wr.send_flags = IBV_SEND_SIGNALED;
	cb->rdma_sq_wr.sg_list = &cb->rdma_sgl;
	cb->rdma_sq_wr.num_sge = 1;
}

static int rping_setup_buffers(struct rping_cb *cb)
{
	int ret;

	DEBUG_LOG("rping_setup_buffers called on cb %p\n", cb);

	cb->recv_mr = ibv_reg_mr(cb->pd, &cb->recv_buf, sizeof cb->recv_buf,
				 IBV_ACCESS_LOCAL_WRITE);
	if (!cb->recv_mr) {
		fprintf(stderr, "recv_buf reg_mr failed\n");
		return errno;
	}

	cb->send_mr = ibv_reg_mr(cb->pd, &cb->send_buf, sizeof cb->send_buf, 0);
	if (!cb->send_mr) {
		fprintf(stderr, "send_buf reg_mr failed\n");
		ret = errno;
		goto err1;
	}

	cb->rdma_buf = malloc(cb->size);
	if (!cb->rdma_buf) {
		fprintf(stderr, "rdma_buf malloc failed\n");
		ret = -ENOMEM;
		goto err2;
	}

	int access = IBV_ACCESS_LOCAL_WRITE |
		     IBV_ACCESS_REMOTE_READ |
		     IBV_ACCESS_REMOTE_WRITE;

	if (cb->check_mw)
		access |= IBV_ACCESS_MW_BIND;

	cb->rdma_mr = ibv_reg_mr(cb->pd, cb->rdma_buf, cb->size, access);
	if (!cb->rdma_mr) {
		fprintf(stderr, "rdma_buf reg_mr failed\n");
		ret = errno;
		goto err3;
	}

	cb->rdma_mw = NULL;
	if (cb->check_mw) {
		cb->rdma_mw = ibv_alloc_mw(cb->pd, type_mw);
		if (!cb->rdma_mw) {
			fprintf(stderr, "rdma_buf alloc_mw failed\n");
			ret = -ENOMEM;
			goto err4;
		}
		DEBUG_LOG("rdma_mw rkey initial: %x\n", cb->rdma_mw->rkey);
	}

	if (!cb->server) {
		cb->start_buf = malloc(cb->size);
		if (!cb->start_buf) {
			fprintf(stderr, "start_buf malloc failed\n");
			ret = -ENOMEM;
			goto err5;
		}

		cb->start_mr = ibv_reg_mr(cb->pd, cb->start_buf,
					  cb->size, access);
		if (!cb->start_mr) {
			fprintf(stderr, "start_buf reg_mr failed\n");
			ret = errno;
			goto err6;
		}
		cb->start_mw = NULL;
		if (cb->check_mw) {
			cb->start_mw = ibv_alloc_mw(cb->pd, type_mw);
			if (!cb->start_mw) {
				fprintf(stderr, "start_buf alloc_mw failed\n");
				ret = -ENOMEM;
				goto err7;
			}
			DEBUG_LOG("start_mw rkey initial: %x\n",
				  cb->start_mw->rkey);
		}
	}

	rping_setup_wr(cb);
	DEBUG_LOG("allocated & registered buffers...\n");
	return 0;

err7:
	ibv_dereg_mr(cb->start_mr);
err6:
	free(cb->start_buf);
err5:
	if (cb->rdma_mw)
		ibv_dealloc_mw(cb->rdma_mw);
err4:
	ibv_dereg_mr(cb->rdma_mr);
err3:
	free(cb->rdma_buf);
err2:
	ibv_dereg_mr(cb->send_mr);
err1:
	ibv_dereg_mr(cb->recv_mr);
	return ret;
}

static void rping_free_buffers(struct rping_cb *cb)
{
	int ret;

	DEBUG_LOG("rping_free_buffers called on cb %p\n", cb);
	ibv_dereg_mr(cb->recv_mr);
	ibv_dereg_mr(cb->send_mr);
	if (cb->check_mw) {
		if (ibv_dealloc_mw(cb->rdma_mw))
			fprintf(stderr, "failed to dealloc rdma_mw\n");
		if (ibv_dealloc_mw(cb->start_mw))
			fprintf(stderr, "failed to dealloc start_mw\n");
	}
	ret = ibv_dereg_mr(cb->rdma_mr);
	if (ret)
		fprintf(stderr, "failed to dereg rdma_mr\n");
	free(cb->rdma_buf);
	if (!cb->server) {
		ret = ibv_dereg_mr(cb->start_mr);
		if (ret)
			fprintf(stderr, "failed to dereg start_mr\n");
		free(cb->start_buf);
	}
}

static int rping_verify_mw_cap(struct rdma_cm_id *cm_id)
{
	struct ibv_exp_device_attr device_attr = {};
	int and;

	device_attr.comp_mask = IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS;
	ibv_exp_query_device(cm_id->verbs, &device_attr);
	and = !!(device_attr.exp_device_cap_flags & IBV_EXP_DEVICE_MEM_WINDOW);
	if (!and) {
		fprintf(stderr, "Device do not support memory window\n");
		return 1;
	}
	return 0;
}

static int rping_create_qp(struct rping_cb *cb)
{
	struct ibv_qp_init_attr init_attr;
	int ret;

	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.cap.max_send_wr = RPING_SQ_DEPTH;
	init_attr.cap.max_recv_wr = 2;
	init_attr.cap.max_recv_sge = 1;
	init_attr.cap.max_send_sge = 1;
	init_attr.qp_type = IBV_QPT_RC;
	init_attr.send_cq = cb->cq;
	init_attr.recv_cq = cb->cq;

	if (cb->server) {
		ret = rdma_create_qp(cb->child_cm_id, cb->pd, &init_attr);
		if (!ret)
			cb->qp = cb->child_cm_id->qp;
	} else {
		ret = rdma_create_qp(cb->cm_id, cb->pd, &init_attr);
		if (!ret)
			cb->qp = cb->cm_id->qp;
	}

	return ret;
}

static void rping_free_qp(struct rping_cb *cb)
{
	ibv_destroy_qp(cb->qp);
	ibv_destroy_cq(cb->cq);
	ibv_destroy_comp_channel(cb->channel);
	ibv_dealloc_pd(cb->pd);
}

static int rping_setup_qp(struct rping_cb *cb, struct rdma_cm_id *cm_id)
{
	int ret;

	cb->pd = ibv_alloc_pd(cm_id->verbs);
	if (!cb->pd) {
		fprintf(stderr, "ibv_alloc_pd failed\n");
		return errno;
	}
	DEBUG_LOG("created pd %p\n", cb->pd);

	cb->channel = ibv_create_comp_channel(cm_id->verbs);
	if (!cb->channel) {
		fprintf(stderr, "ibv_create_comp_channel failed\n");
		ret = errno;
		goto err1;
	}
	DEBUG_LOG("created channel %p\n", cb->channel);

	cb->cq = ibv_create_cq(cm_id->verbs, RPING_SQ_DEPTH * 2, cb,
				cb->channel, 0);
	if (!cb->cq) {
		fprintf(stderr, "ibv_create_cq failed\n");
		ret = errno;
		goto err2;
	}
	DEBUG_LOG("created cq %p\n", cb->cq);

	ret = ibv_req_notify_cq(cb->cq, 0);
	if (ret) {
		fprintf(stderr, "ibv_create_cq failed\n");
		ret = errno;
		goto err3;
	}

	ret = rping_create_qp(cb);
	if (ret) {
		perror("rdma_create_qp");
		goto err3;
	}
	DEBUG_LOG("created qp %p\n", cb->qp);
	return 0;

err3:
	ibv_destroy_cq(cb->cq);
err2:
	ibv_destroy_comp_channel(cb->channel);
err1:
	ibv_dealloc_pd(cb->pd);
	return ret;
}

static void *cm_thread(void *arg)
{
	struct rping_cb *cb = arg;
	struct rdma_cm_event *event;
	int ret;

	while (1) {
		ret = rdma_get_cm_event(cb->cm_channel, &event);
		if (ret) {
			perror("rdma_get_cm_event");
			exit(ret);
		}
		ret = rping_cma_event_handler(event->id, event);
		rdma_ack_cm_event(event);
		if (ret)
			exit(ret);
	}
}

static void *cq_thread(void *arg)
{
	struct rping_cb *cb = arg;
	struct ibv_cq *ev_cq;
	void *ev_ctx;
	int ret;
	
	DEBUG_LOG("cq_thread started.\n");

	while (1) {	
		pthread_testcancel();

		ret = ibv_get_cq_event(cb->channel, &ev_cq, &ev_ctx);
		if (ret) {
			fprintf(stderr, "Failed to get cq event!\n");
			pthread_exit(NULL);
		}
		if (ev_cq != cb->cq) {
			fprintf(stderr, "Unknown CQ!\n");
			pthread_exit(NULL);
		}
		ret = ibv_req_notify_cq(cb->cq, 0);
		if (ret) {
			fprintf(stderr, "Failed to set notify!\n");
			pthread_exit(NULL);
		}
		ret = rping_cq_event_handler(cb);
		ibv_ack_cq_events(cb->cq, 1);
		if (ret)
			pthread_exit(NULL);
	}
}


static int rping_invalidate_mw(struct ibv_mw *mw, struct ibv_qp *qp)
{
	/* =========== invalidating last mw =========== */
	struct ibv_exp_send_wr inv_wr = { 0 };
	struct ibv_exp_send_wr *bad_inv_wr = NULL;

	inv_wr.exp_opcode = IBV_EXP_WR_LOCAL_INV;
	inv_wr.next = NULL;
	inv_wr.exp_send_flags = IBV_EXP_SEND_SIGNALED;
	inv_wr.ex.invalidate_rkey = mw->rkey;

	/* posting the message */
	return ibv_exp_post_send(qp, &inv_wr, &bad_inv_wr);
}


static int rping_bind_mw(struct ibv_qp *qp, struct ibv_mw *mw,
			 struct ibv_mr *mr, char *buf, int size,
			 uint32_t new_rkey)
{
	int ret = 0;
	if (type_mw == IBV_MW_TYPE_2) {
		struct ibv_exp_send_wr bind_wr = { 0 };
		struct ibv_exp_send_wr *bad_bind_wr = NULL;

		bind_wr.exp_opcode = IBV_EXP_WR_BIND_MW;
		bind_wr.next = NULL;
		bind_wr.exp_send_flags = IBV_EXP_SEND_SIGNALED;

		bind_wr.bind_mw.mw = mw;
		bind_wr.bind_mw.rkey = new_rkey;
		bind_wr.bind_mw.bind_info.addr = (uint64_t) buf;
		bind_wr.bind_mw.bind_info.length = size;
		bind_wr.bind_mw.bind_info.mr = mr;
		bind_wr.bind_mw.bind_info.exp_mw_access_flags =
			IBV_EXP_ACCESS_REMOTE_READ | IBV_EXP_ACCESS_REMOTE_WRITE |
			IBV_EXP_ACCESS_REMOTE_ATOMIC;
		/* posting the message */
		ret = ibv_exp_post_send(qp, &bind_wr, &bad_bind_wr);
		/* updating the newly assigned rkey */
		mw->rkey = bind_wr.bind_mw.rkey;
	} else {
		struct ibv_exp_mw_bind_info bind_info;
		struct ibv_exp_mw_bind mw_bind;

		bind_info.mr = mr;
		bind_info.addr = (uint64_t) buf;
		bind_info.length = size;
		bind_info.exp_mw_access_flags = IBV_EXP_ACCESS_REMOTE_READ |
			IBV_EXP_ACCESS_REMOTE_WRITE |
			IBV_EXP_ACCESS_REMOTE_ATOMIC;

		mw_bind.wr_id = RPING_BIND_WRID;
		mw_bind.exp_send_flags = IBV_EXP_SEND_SIGNALED;
		mw_bind.bind_info = bind_info;
		mw_bind.qp = qp;
		mw_bind.mw = mw;
		mw_bind.comp_mask = 0;

		ret = ibv_exp_bind_mw(&mw_bind);
	}

	return ret;
}

static void rping_rebind_mw(struct ibv_qp *qp, struct ibv_mw *mw,
			    struct ibv_mr *mr, char *buf, int size)
{
	if (type_mw == IBV_MW_TYPE_2)
		rping_invalidate_mw(mw, qp);
	rping_bind_mw(qp, mw, mr, buf, size, ibv_inc_rkey(mw->rkey));
}


static void rping_format_send(struct rping_cb *cb, char *buf, uint32_t rkey)
{
	struct rping_rdma_info *info = &cb->send_buf;

	info->buf = htonll((uint64_t) (unsigned long) buf);
	info->rkey = htonl(rkey);
	info->size = htonl(cb->size);

	DEBUG_LOG("RDMA addr %" PRIx64" rkey %x len %d\n",
		  ntohll(info->buf), ntohl(info->rkey), ntohl(info->size));
}

static int rping_test_server(struct rping_cb *cb)
{
	struct ibv_send_wr *bad_wr;
	int ret;

	while (1) {
		/* Wait for client's Start STAG/TO/Len */
		sem_wait(&cb->sem);
		if (cb->state != RDMA_READ_ADV) {
			fprintf(stderr, "wait for RDMA_READ_ADV state %d\n",
				cb->state);
			ret = -1;
			break;
		}

		DEBUG_LOG("server received sink adv\n");

		/* Issue RDMA Read. */
		cb->rdma_sq_wr.opcode = IBV_WR_RDMA_READ;
		cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
		cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
		cb->rdma_sq_wr.sg_list->length = cb->remote_len;

		ret = ibv_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
		if (ret) {
			fprintf(stderr, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG("server posted rdma read req \n");

		/* Wait for read completion */
		sem_wait(&cb->sem);
		if (cb->state != RDMA_READ_COMPLETE) {
			fprintf(stderr, "wait for RDMA_READ_COMPLETE state %d\n",
				cb->state);
			ret = -1;
			break;
		}
		DEBUG_LOG("server received read complete\n");

		/* Display data in recv buf */
		if (cb->verbose)
			printf("server ping data: %s\n", cb->rdma_buf);

		/* Tell client to continue */
		ret = ibv_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			fprintf(stderr, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG("server posted go ahead\n");

		/* Wait for client's RDMA STAG/TO/Len */
		sem_wait(&cb->sem);
		if (cb->state != RDMA_WRITE_ADV) {
			fprintf(stderr, "wait for RDMA_WRITE_ADV state %d\n",
				cb->state);
			ret = -1;
			break;
		}
		DEBUG_LOG("server received sink adv\n");

		/* RDMA Write echo data */
		cb->rdma_sq_wr.opcode = IBV_WR_RDMA_WRITE;
		cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
		cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
		cb->rdma_sq_wr.sg_list->length = strlen(cb->rdma_buf) + 1;
		DEBUG_LOG("rdma write from lkey %x laddr %" PRIx64 " len %d\n",
			  cb->rdma_sq_wr.sg_list->lkey,
			  cb->rdma_sq_wr.sg_list->addr,
			  cb->rdma_sq_wr.sg_list->length);

		ret = ibv_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
		if (ret) {
			fprintf(stderr, "post send error %d\n", ret);
			break;
		}

		/* Wait for completion */
		ret = sem_wait(&cb->sem);
		if (cb->state != RDMA_WRITE_COMPLETE) {
			fprintf(stderr, "wait for RDMA_WRITE_COMPLETE state %d\n",
				cb->state);
			ret = -1;
			break;
		}
		DEBUG_LOG("server rdma write complete \n");

		/* Tell client to begin again */
		ret = ibv_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			fprintf(stderr, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG("server posted go ahead\n");
	}

	return (cb->state == DISCONNECTED) ? 0 : ret;
}

static int rping_bind_server(struct rping_cb *cb)
{
	int ret;

	if (cb->sin.ss_family == AF_INET)
		((struct sockaddr_in *) &cb->sin)->sin_port = cb->port;
	else
		((struct sockaddr_in6 *) &cb->sin)->sin6_port = cb->port;

	ret = rdma_bind_addr(cb->cm_id, (struct sockaddr *) &cb->sin);
	if (ret) {
		perror("rdma_bind_addr");
		return ret;
	}
	DEBUG_LOG("rdma_bind_addr successful\n");

	DEBUG_LOG("rdma_listen\n");
	ret = rdma_listen(cb->cm_id, 3);
	if (ret) {
		perror("rdma_listen");
		return ret;
	}

	return 0;
}

static struct rping_cb *clone_cb(struct rping_cb *listening_cb)
{
	struct rping_cb *cb = malloc(sizeof *cb);
	if (!cb)
		return NULL;
	*cb = *listening_cb;
	cb->child_cm_id->context = cb;
	return cb;
}

static void free_cb(struct rping_cb *cb)
{
	free(cb);
}

static void *rping_persistent_server_thread(void *arg)
{
	struct rping_cb *cb = arg;
	struct ibv_recv_wr *bad_wr;
	int ret;

	ret = rping_setup_qp(cb, cb->child_cm_id);
	if (ret) {
		fprintf(stderr, "setup_qp failed: %d\n", ret);
		goto err0;
	}

	ret = rping_setup_buffers(cb);
	if (ret) {
		fprintf(stderr, "rping_setup_buffers failed: %d\n", ret);
		goto err1;
	}

	ret = ibv_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
	if (ret) {
		fprintf(stderr, "ibv_post_recv failed: %d\n", ret);
		goto err2;
	}

	ret = pthread_create(&cb->cqthread, NULL, cq_thread, cb);
	if (ret) {
		perror("pthread_create");
		goto err2;
	}

	ret = rping_accept(cb);
	if (ret) {
		fprintf(stderr, "connect error %d\n", ret);
		goto err3;
	}

	rping_test_server(cb);
	rdma_disconnect(cb->child_cm_id);
	pthread_join(cb->cqthread, NULL);
	rping_free_buffers(cb);
	rping_free_qp(cb);
	rdma_destroy_id(cb->child_cm_id);
	free_cb(cb);
	return NULL;
err3:
	pthread_cancel(cb->cqthread);
	pthread_join(cb->cqthread, NULL);
err2:
	rping_free_buffers(cb);
err1:
	rping_free_qp(cb);
err0:
	free_cb(cb);
	return NULL;
}

static int rping_run_persistent_server(struct rping_cb *listening_cb)
{
	int ret;
	struct rping_cb *cb;
	pthread_attr_t attr;

	ret = rping_bind_server(listening_cb);
	if (ret)
		return ret;

	/*
	 * Set persistent server threads to DEATCHED state so
	 * they release all their resources when they exit.
	 */
	ret = pthread_attr_init(&attr);
	if (ret) {
		perror("pthread_attr_init");
		return ret;
	}
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret) {
		perror("pthread_attr_setdetachstate");
		return ret;
	}

	while (1) {
		sem_wait(&listening_cb->sem);
		if (listening_cb->state != CONNECT_REQUEST) {
			fprintf(stderr, "wait for CONNECT_REQUEST state %d\n",
				listening_cb->state);
			return -1;
		}

		cb = clone_cb(listening_cb);
		if (!cb)
			return -1;

		ret = pthread_create(&cb->persistent_server_thread, &attr, rping_persistent_server_thread, cb);
		if (ret) {
			perror("pthread_create");
			return ret;
		}
	}
	return 0;
}

static int rping_run_server(struct rping_cb *cb)
{
	struct ibv_recv_wr *bad_wr;
	int ret;

	ret = rping_bind_server(cb);
	if (ret)
		return ret;

	sem_wait(&cb->sem);
	if (cb->state != CONNECT_REQUEST) {
		fprintf(stderr, "wait for CONNECT_REQUEST state %d\n",
			cb->state);
		return -1;
	}

	ret = rping_setup_qp(cb, cb->child_cm_id);
	if (ret) {
		fprintf(stderr, "setup_qp failed: %d\n", ret);
		return ret;
	}

	ret = rping_setup_buffers(cb);
	if (ret) {
		fprintf(stderr, "rping_setup_buffers failed: %d\n", ret);
		goto err1;
	}

	ret = ibv_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
	if (ret) {
		fprintf(stderr, "ibv_post_recv failed: %d\n", ret);
		goto err2;
	}

	ret = pthread_create(&cb->cqthread, NULL, cq_thread, cb);
	if (ret) {
		perror("pthread_create");
		goto err2;
	}

	ret = rping_accept(cb);
	if (ret) {
		fprintf(stderr, "connect error %d\n", ret);
		goto err2;
	}

	ret = rping_test_server(cb);
	if (ret && ret != -1) {
		fprintf(stderr, "rping server failed: %d\n", ret);
		goto err3;
	}

	ret = 0;
err3:
	rdma_disconnect(cb->child_cm_id);
	pthread_join(cb->cqthread, NULL);
	rdma_destroy_id(cb->child_cm_id);
err2:
	rping_free_buffers(cb);
err1:
	rping_free_qp(cb);

	return ret;
}

static int rping_test_client(struct rping_cb *cb)
{
	int ping, start, cc, i, ret = 0;
	struct ibv_send_wr *bad_wr;
	unsigned char c;

	start = 65;

	if (cb->check_mw) {
		rping_bind_mw(cb->qp, cb->start_mw, cb->start_mr, cb->start_buf,
			      cb->size, ibv_inc_rkey(cb->start_mw->rkey));
		rping_bind_mw(cb->qp, cb->rdma_mw, cb->rdma_mr, cb->rdma_buf,
			      cb->size, ibv_inc_rkey(cb->rdma_mw->rkey));
		DEBUG_LOG("Binding\n");
		DEBUG_LOG("start_mw rkey: %x\n", cb->start_mw->rkey);
		DEBUG_LOG("rdma_mw rkey: %x\n", cb->rdma_mw->rkey);
	}

	for (ping = 0; !cb->count || ping < cb->count; ping++) {
		cb->state = RDMA_READ_ADV;

		/* Put some ascii text in the buffer. */
		cc = snprintf(cb->start_buf, cb->size, RPING_MSG_FMT, ping);
		for (i = cc, c = start; i < cb->size; i++) {
			cb->start_buf[i] = c;
			c++;
			if (c > 122)
				c = 65;
		}
		start++;
		if (start > 122)
			start = 65;
		cb->start_buf[cb->size - 1] = 0;

		if (cb->check_mw)
			rping_format_send(cb, cb->start_buf,
					  cb->start_mw->rkey);
		else
			rping_format_send(cb, cb->start_buf,
					  cb->start_mr->rkey);

		ret = ibv_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			fprintf(stderr, "post send error %d\n", ret);
			break;
		}

		/* Wait for server to ACK */
		sem_wait(&cb->sem);
		if (cb->state != RDMA_WRITE_ADV) {
			fprintf(stderr, "wait for RDMA_WRITE_ADV state %d\n",
				cb->state);
			ret = -1;
			break;
		}

		if (cb->check_mw)
			rping_format_send(cb, cb->rdma_buf, cb->rdma_mw->rkey);
		else
			rping_format_send(cb, cb->rdma_buf, cb->rdma_mr->rkey);
		ret = ibv_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			fprintf(stderr, "post send error %d\n", ret);
			break;
		}

		/* Wait for the server to say the RDMA Write is complete. */
		sem_wait(&cb->sem);
		if (cb->state != RDMA_WRITE_COMPLETE) {
			fprintf(stderr, "wait for RDMA_WRITE_COMPLETE state %d\n",
				cb->state);
			ret = -1;
			break;
		}

		if (cb->check_mw) {
			rping_rebind_mw(cb->qp, cb->start_mw, cb->start_mr,
					cb->start_buf, cb->size);
			rping_rebind_mw(cb->qp, cb->rdma_mw, cb->rdma_mr,
					cb->rdma_buf, cb->size);
			DEBUG_LOG("Rebinding\n");
			DEBUG_LOG("start_mw rkey: %x\n", cb->start_mw->rkey);
			DEBUG_LOG("rdma_mw rkey: %x\n", cb->rdma_mw->rkey);
		}

		if (cb->validate)
			if (memcmp(cb->start_buf, cb->rdma_buf, cb->size)) {
				fprintf(stderr, "data mismatch!\n");
				ret = -1;
				break;
			}

		if (cb->verbose)
			printf("ping data: %s\n", cb->rdma_buf);
	}

	return (cb->state == DISCONNECTED) ? 0 : ret;
}

static int rping_connect_client(struct rping_cb *cb)
{
	struct rdma_conn_param conn_param;
	int ret;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;
	conn_param.retry_count = 7;

	ret = rdma_connect(cb->cm_id, &conn_param);
	if (ret) {
		perror("rdma_connect");
		return ret;
	}

	sem_wait(&cb->sem);
	if (cb->state != CONNECTED) {
		fprintf(stderr, "wait for CONNECTED state %d\n", cb->state);
		return -1;
	}

	DEBUG_LOG("rmda_connect successful\n");
	return 0;
}

static int rping_bind_client(struct rping_cb *cb)
{
	int ret;

	if (cb->sin.ss_family == AF_INET)
		((struct sockaddr_in *) &cb->sin)->sin_port = cb->port;
	else
		((struct sockaddr_in6 *) &cb->sin)->sin6_port = cb->port;

	ret = rdma_resolve_addr(cb->cm_id, NULL, (struct sockaddr *) &cb->sin, 2000);
	if (ret) {
		perror("rdma_resolve_addr");
		return ret;
	}

	sem_wait(&cb->sem);
	if (cb->state != ROUTE_RESOLVED) {
		fprintf(stderr, "waiting for addr/route resolution state %d\n",
			cb->state);
		return -1;
	}

	DEBUG_LOG("rdma_resolve_addr - rdma_resolve_route successful\n");
	return 0;
}

static int rping_run_client(struct rping_cb *cb)
{
	struct ibv_recv_wr *bad_wr;
	int ret;

	ret = rping_bind_client(cb);
	if (ret)
		return ret;

	if (cb->check_mw) {
		ret = rping_verify_mw_cap(cb->cm_id);
		if (ret) {
			fprintf(stderr, "device doesn't support MW\n");
			return ret;
		}
	}

	ret = rping_setup_qp(cb, cb->cm_id);
	if (ret) {
		fprintf(stderr, "setup_qp failed: %d\n", ret);
		return ret;
	}

	ret = rping_setup_buffers(cb);
	if (ret) {
		fprintf(stderr, "rping_setup_buffers failed: %d\n", ret);
		goto err1;
	}

	ret = ibv_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
	if (ret) {
		fprintf(stderr, "ibv_post_recv failed: %d\n", ret);
		goto err2;
	}

	ret = pthread_create(&cb->cqthread, NULL, cq_thread, cb);
	if (ret) {
		perror("pthread_create");
		goto err2;
	}

	ret = rping_connect_client(cb);
	if (ret) {
		fprintf(stderr, "connect error %d\n", ret);
		goto err3;
	}

	ret = rping_test_client(cb);
	if (ret) {
		fprintf(stderr, "rping client failed: %d\n", ret);
		goto err4;
	}

	ret = 0;
err4:
	rdma_disconnect(cb->cm_id);
err3:
	pthread_join(cb->cqthread, NULL);
err2:
	rping_free_buffers(cb);
err1:
	rping_free_qp(cb);

	return ret;
}

static int get_addr(char *dst, struct sockaddr *addr)
{
	struct addrinfo *res;
	int ret;

	ret = getaddrinfo(dst, NULL, NULL, &res);
	if (ret) {
		printf("getaddrinfo failed - invalid hostname or IP address\n");
		return ret;
	}

	if (res->ai_family == PF_INET)
		memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
	else if (res->ai_family == PF_INET6)
		memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in6));
	else
		ret = -1;
	
	freeaddrinfo(res);
	return ret;
}

static void usage(char *name)
{
	printf("%s -s [-vVd] [-S size] [-C count] [-a addr] [-p port]\n", 
	       basename(name));
	printf("%s -c [-vVd] [-S size] [-C count] -a addr [-p port]\n", 
	       basename(name));
	printf("\t-c\t\tclient side\n");
	printf("\t-s\t\tserver side.  To bind to any address with IPv6 use -a ::0\n");
	printf("\t-v\t\tdisplay ping data to stdout\n");
	printf("\t-V\t\tvalidate ping data\n");
	printf("\t-d\t\tdebug printfs\n");
	printf("\t-S size \tping data size\n");
	printf("\t-C count\tping count times\n");
	printf("\t-a addr\t\taddress\n");
	printf("\t-p port\t\tport\n");
	printf("\t-P\t\tpersistent server mode allowing multiple connections\n");
	printf("\t-w type (1/2)\tuse memory window\n");
}

int main(int argc, char *argv[])
{
	struct rping_cb *cb;
	int op;
	int ret = 0;
	int persistent_server = 0;

	cb = malloc(sizeof(*cb));
	if (!cb)
		return -ENOMEM;

	memset(cb, 0, sizeof(*cb));
	cb->server = -1;
	cb->check_mw = 0;
	cb->state = IDLE;
	cb->size = 64;
	cb->sin.ss_family = PF_INET;
	cb->port = htons(7174);
	sem_init(&cb->sem, 0, 0);

	opterr = 0;
	while ((op=getopt(argc, argv, "a:Pp:C:S:t:scvVdw:")) != -1) {
		switch (op) {
		case 'a':
			ret = get_addr(optarg, (struct sockaddr *) &cb->sin);
			break;
		case 'P':
			persistent_server = 1;
			break;
		case 'p':
			cb->port = htons(atoi(optarg));
			DEBUG_LOG("port %d\n", (int) atoi(optarg));
			break;
		case 's':
			cb->server = 1;
			DEBUG_LOG("server\n");
			break;
		case 'c':
			cb->server = 0;
			DEBUG_LOG("client\n");
			break;
		case 'S':
			cb->size = atoi(optarg);
			if ((cb->size < RPING_MIN_BUFSIZE) ||
			    (cb->size > (RPING_BUFSIZE - 1))) {
				fprintf(stderr, "Invalid size %d "
				       "(valid range is %Zd to %d)\n",
				       cb->size, RPING_MIN_BUFSIZE, RPING_BUFSIZE);
				ret = EINVAL;
			} else
				DEBUG_LOG("size %d\n", (int) atoi(optarg));
			break;
		case 'C':
			cb->count = atoi(optarg);
			if (cb->count < 0) {
				fprintf(stderr, "Invalid count %d\n",
					cb->count);
				ret = EINVAL;
			} else
				DEBUG_LOG("count %d\n", (int) cb->count);
			break;
		case 'v':
			cb->verbose++;
			DEBUG_LOG("verbose\n");
			break;
		case 'V':
			cb->validate++;
			DEBUG_LOG("validate data\n");
			break;
		case 'd':
			debug++;
			break;
		case 'w':
			DEBUG_LOG("using memory window\n");
			++cb->check_mw;
			type_mw = (uint8_t) atoi(optarg);
			DEBUG_LOG("type of memory window %d\n", type_mw);
			break;
		default:
			usage("rping");
			ret = EINVAL;
			goto out;
		}
	}
	if (ret)
		goto out;

	if (cb->server == -1) {
		usage("rping");
		ret = EINVAL;
		goto out;
	}

	if (cb->check_mw && cb->server) {
		ret = EINVAL;
		errno = EINVAL;
		perror("server cannot have MW flag");
		goto out;
	}

	if (cb->check_mw && type_mw != 1 && type_mw != 2) {
		ret = EINVAL;
		errno = EINVAL;
		perror("MW type can accept values 1 or 2");
		goto out;
	}

	cb->cm_channel = rdma_create_event_channel();
	if (!cb->cm_channel) {
		ret = errno;
		errno = ENOMEM;
		perror("rdma_create_event_channel");
		goto out;
	}

	ret = rdma_create_id(cb->cm_channel, &cb->cm_id, cb, RDMA_PS_TCP);
	if (ret) {
		ret = errno;
		perror("rdma_create_id");
		goto out2;
	}
	DEBUG_LOG("created cm_id %p\n", cb->cm_id);

	ret = pthread_create(&cb->cmthread, NULL, cm_thread, cb);
	if (ret) {
		perror("pthread_create");
		goto out2;
	}

	if (cb->server) {
		if (persistent_server)
			ret = rping_run_persistent_server(cb);
		else
			ret = rping_run_server(cb);
	} else {
		ret = rping_run_client(cb);
	}

	DEBUG_LOG("destroy cm_id %p\n", cb->cm_id);
	rdma_destroy_id(cb->cm_id);
out2:
	rdma_destroy_event_channel(cb->cm_channel);
out:
	free(cb);
	return ret;
}
