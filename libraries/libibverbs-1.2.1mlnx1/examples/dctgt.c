/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <infiniband/verbs_exp.h>
#include "dc.h"

struct dc_ctx {
	struct ibv_qp		*qp;
	struct ibv_cq		*cq;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_srq		*srq;
	struct ibv_context	*ctx;
	void			*addr;
	size_t			length;
	int			port;
	uint64_t		dct_key;
	unsigned		size;
	int			ib_port;
	enum ibv_mtu		mtu;
	int			rcv_idx;
	struct ibv_port_attr	portinfo;
	struct ibv_exp_dct       *dct;
	pthread_t		thread;
	int			thread_active;
	int			inl;
	pthread_t		poll_thread;
};

static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>      listen on/connect to port <port> (default 18515)\n");
	printf("  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)\n");
	printf("  -i, --ib-port=<port>   use port <port> of IB device (default 1)\n");
	printf("  -s, --size=<size>      size of message to exchange (default 4096)\n");
	printf("  -n, --iters=<iters>    number of exchanges (unlimited)\n");
	printf("  -e, --events           sleep on CQ events (default poll)\n");
	printf("  -c, --contiguous-mr    use contiguous mr\n");
	printf("  -k, --dc-key           DC transport key\n");
	printf("  -m, --mtu              MTU of the DCT\n");
	printf("  -l, --inline           Requested inline receive size\n");
}

static int post_recv(struct dc_ctx *ctx, int n)
{
	struct ibv_sge list = {
		.length = ctx->size,
		.lkey	= ctx->mr->lkey
	};
	struct ibv_recv_wr wr = {
		.sg_list    = &list,
		.num_sge    = 1,
	};
	struct ibv_recv_wr *bad_wr;
	int i;

	for (i = 0; i < n; ++i) {
		list.addr = (uintptr_t)ctx->addr + (ctx->size * (ctx->rcv_idx++ % 32));
		if (ibv_post_srq_recv(ctx->srq, &wr, &bad_wr))
			break;
	}

	return i;
}

static struct pingpong_dest *pp_server_exch_dest(struct dc_ctx *ctx,
						 const struct pingpong_dest *my_dest)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_flags    = AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof(MSG_FORMAT)];
	int n;
	int sockfd = -1, connfd;
	struct pingpong_dest *rem_dest = NULL;
	int err;

	if (asprintf(&service, "%d", ctx->port) < 0)
		return NULL;

	n = getaddrinfo(NULL, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for port %d\n", gai_strerror(n), ctx->port);
		free(service);
		return NULL;
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
		fprintf(stderr, "Couldn't listen to port %d\n", ctx->port);
		return NULL;
	}

	err = listen(sockfd, 1);
	if (err)
		return NULL;

	connfd = accept(sockfd, NULL, 0);
	close(sockfd);
	if (connfd < 0) {
		fprintf(stderr, "accept() failed\n");
		return NULL;
	}

	n = read(connfd, msg, sizeof(msg));
	if (n != sizeof(msg)) {
		perror("server read");
		fprintf(stderr, "%d/%d: Couldn't read remote address\n", n, (int)sizeof(msg));
		goto out;
	}

	rem_dest = malloc(sizeof(*rem_dest));
	if (!rem_dest)
		goto out;

	sscanf(msg, "%06x:%04x:%016" SCNx64, &rem_dest->rsn, &rem_dest->lid, &rem_dest->dckey);
	printf("Connection from: QPN %06x, LID %04x\n", rem_dest->rsn, rem_dest->lid);

	sprintf(msg, "%06x:%04x:%016" PRIx64, ctx->dct->dct_num, ctx->portinfo.lid, ctx->dct_key);

	if (write(connfd, msg, sizeof(msg)) != sizeof(msg)) {
		fprintf(stderr, "Couldn't send local address\n");
		free(rem_dest);
		rem_dest = NULL;
		goto out;
	}

out:
	close(connfd);
	return rem_dest;
}

static void *handle_clients(void *arg)
{
	struct dc_ctx *ctx = arg;
	struct pingpong_dest my_dest;
	struct pingpong_dest *ret;

	while (ctx->thread_active) {
		ret = pp_server_exch_dest(ctx, &my_dest);
		if (!ret)
			exit(EXIT_FAILURE);
	}

	return NULL;
}

static const char *event_name_str(enum ibv_event_type event_type)
{
	switch (event_type) {
	case IBV_EVENT_DEVICE_FATAL:
		return "IBV_EVENT_DEVICE_FATAL";
	case IBV_EVENT_PORT_ACTIVE:
		return "IBV_EVENT_PORT_ACTIVE";
	case IBV_EVENT_PORT_ERR:
		return "IBV_EVENT_PORT_ERR";
	case IBV_EVENT_LID_CHANGE:
		return "IBV_EVENT_LID_CHANGE";
	case IBV_EVENT_PKEY_CHANGE:
		return "IBV_EVENT_PKEY_CHANGE";
	case IBV_EVENT_SM_CHANGE:
		return "IBV_EVENT_SM_CHANGE";
	case IBV_EVENT_CLIENT_REREGISTER:
		return "IBV_EVENT_CLIENT_REREGISTER";
	case IBV_EVENT_GID_CHANGE:
		return "IBV_EVENT_GID_CHANGE";
	case IBV_EXP_EVENT_DCT_KEY_VIOLATION:
		return "IBV_EXP_EVENT_DCT_KEY_VIOLATION";
	case IBV_EVENT_QP_ACCESS_ERR:
		return "IBV_EVENT_QP_ACCESS_ERR";

	case IBV_EVENT_CQ_ERR:
	case IBV_EVENT_QP_FATAL:
	case IBV_EVENT_QP_REQ_ERR:
	case IBV_EVENT_COMM_EST:
	case IBV_EVENT_SQ_DRAINED:
	case IBV_EVENT_PATH_MIG:
	case IBV_EVENT_PATH_MIG_ERR:
	case IBV_EVENT_SRQ_ERR:
	case IBV_EVENT_SRQ_LIMIT_REACHED:
	case IBV_EVENT_QP_LAST_WQE_REACHED:
	default:
		return "unexpected";
	}
}

static void *poll_async(void *arg)
{
	struct dc_ctx *ctx = arg;
	struct ibv_async_event event;
	struct ibv_exp_arm_attr attr;
	int err;

	while (1) {
		attr.comp_mask = 0;
		err = ibv_exp_arm_dct(ctx->dct, &attr);
		if (err) {
			fprintf(stderr, "arm dct failed %d\n", err);
			return NULL;
		}
		if (ibv_get_async_event(ctx->ctx, &event))
			return NULL;

		printf("  event_type %s (%d)\n",
		       event_name_str(event.event_type),
		       event.event_type);

		ibv_ack_async_event(&event);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	struct ibv_device	**dev_list;
	struct ibv_device	*ib_dev;
	char			*ib_devname = NULL;
	int			iters = 0;
	int			use_event = 0;
	int			err;
	struct dc_ctx		ctx = {
		.port		= 18515,
		.ib_port	= 1,
		.dct_key	= 0x1234,
		.size		= 4096,
		.mtu		= IBV_MTU_2048,
		.inl		= 0,
	};
	int i;
	uint32_t srqn;
	int mtu;
	struct ibv_exp_device_attr dattr;

	srand48(getpid() * time(NULL));

	while (1) {
		int c;

		static struct option long_options[] = {
			{ .name = "port",       .has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",     .has_arg = 1, .val = 'd' },
			{ .name = "ib-port",    .has_arg = 1, .val = 'i' },
			{ .name = "size",       .has_arg = 1, .val = 's' },
			{ .name = "iters",      .has_arg = 1, .val = 'n' },
			{ .name = "events",     .has_arg = 0, .val = 'e' },
			{ .name = "dc-key",     .has_arg = 1, .val = 'k' },
			{ .name = "mtu",	.has_arg = 1, .val = 'm' },
			{ .name = "inline",	.has_arg = 1, .val = 'l' },
			{ 0 }
		};

		c = getopt_long(argc, argv, "p:d:i:s:n:ek:m:l:", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			ctx.port = strtol(optarg, NULL, 0);
			if (ctx.port < 0 || ctx.port > 65535) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'd':
			ib_devname = strdupa(optarg);
			break;

		case 'l':
			ctx.inl = strtol(optarg, NULL, 0);
			if (ctx.inl < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'i':
			ctx.ib_port = strtol(optarg, NULL, 0);
			if (ctx.ib_port < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'm':
			mtu = strtol(optarg, NULL, 0);
			if (to_ib_mtu(mtu, &ctx.mtu)) {
				printf("invalid MTU %d\n", mtu);
				return 1;
			}
			break;

		case 's':
			ctx.size = strtol(optarg, NULL, 0);
			break;

		case 'n':
			iters = strtol(optarg, NULL, 0);
			break;

		case 'e':
			++use_event;
			break;

		case 'k':
			ctx.dct_key = strtoull(optarg, NULL, 0);
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind < argc) {
		usage(argv[0]);
		return 1;
	}

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("Failed to get IB devices list");
		return 1;
	}

	if (!ib_devname) {
		ib_dev = *dev_list;
		if (!ib_dev) {
			fprintf(stderr, "No IB devices found\n");
			return 1;
		}
	} else {
		int i;
		for (i = 0; dev_list[i]; ++i)
			if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname))
				break;
		ib_dev = dev_list[i];
		if (!ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ib_devname);
			return 1;
		}
	}

	ctx.ctx = ibv_open_device(ib_dev);
	if (!ctx.ctx) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		return 1;
	}

	dattr.comp_mask = IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS |
		IBV_EXP_DEVICE_DC_RD_REQ |
		IBV_EXP_DEVICE_DC_RD_RES;
	err = ibv_exp_query_device(ctx.ctx, &dattr);
	if (err) {
		printf("couldn't query device extended attributes\n");
		return -1;
	} else {
		if (!(dattr.comp_mask & IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS)) {
			printf("no extended capability flgas\n");
			return -1;
		}
		if (!(dattr.exp_device_cap_flags & IBV_EXP_DEVICE_DC_TRANSPORT)) {
			printf("DC transport not enabled\n");
			return -1;
		}

		if (!(dattr.comp_mask & IBV_EXP_DEVICE_DC_RD_REQ)) {
			printf("no report on max requestor rdma/atomic resources\n");
			return -1;
		}

		if (!(dattr.comp_mask & IBV_EXP_DEVICE_DC_RD_RES)) {
			printf("no report on max responder rdma/atomic resources\n");
			return -1;
		}
	}

	ctx.pd = ibv_alloc_pd(ctx.ctx);
	if (!ctx.pd) {
		fprintf(stderr, "failed to allocate pd\n");
		return 1;
	}

	ctx.length = 32 * ctx.size;
	ctx.addr = malloc(ctx.length);
	if (!ctx.addr) {
		fprintf(stderr, "failed to allocate memory\n");
		return -1;
	}

	ctx.mr = ibv_reg_mr(ctx.pd, ctx.addr, ctx.length,
			IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);
	if (!ctx.mr) {
		fprintf(stderr, "failed to create mr\n");
		return -1;
	}

	ctx.cq = ibv_create_cq(ctx.ctx, 128, NULL, NULL, 0);
	if (!ctx.cq) {
		fprintf(stderr, "failed to create cq\n");
		return -1;
	}


	{
		struct ibv_srq_init_attr attr = {
			.attr = {
				.max_wr  = 100,
				.max_sge = 1
			}
		};

		ctx.srq = ibv_create_srq(ctx.pd, &attr);
		if (!ctx.srq)  {
			fprintf(stderr, "Couldn't create SRQ\n");
			return -1;
		}
		ibv_get_srq_num(ctx.srq, &srqn);
	}

	err = post_recv(&ctx, 100);
	if (err != 100) {
		fprintf(stderr, "posted %d out of %d receive buffers\n", err, 100);
		return -1;
	}

	{
		struct ibv_exp_dct_init_attr dctattr = {
			.pd = ctx.pd,
			.cq = ctx.cq,
			.srq = ctx.srq,
			.dc_key = ctx.dct_key,
			.port = ctx.ib_port,
			.access_flags = IBV_ACCESS_REMOTE_WRITE,
			.min_rnr_timer = 2,
			.tclass = 0,
			.flow_label = 0,
			.mtu = ctx.mtu,
			.pkey_index = 0,
			.gid_index = 0,
			.hop_limit = 1,
			.create_flags = 0,
			.inline_size = ctx.inl,
		};

		ctx.dct = ibv_exp_create_dct(ctx.ctx, &dctattr);
		if (!ctx.dct) {
			printf("create dct failed\n");
			return -1;
		}

		{
			struct ibv_exp_dct_attr dcqattr;

			dcqattr.comp_mask = 0;
			err = ibv_exp_query_dct(ctx.dct, &dcqattr);
			if (err) {
				printf("query dct failed\n");
				return -1;
			} else if (dcqattr.dc_key != ctx.dct_key) {
				printf("queried dckry (0x%llx) is different then provided at create (0x%llx)\n",
				       (unsigned long long)dcqattr.dc_key,
				       (unsigned long long)ctx.dct_key);
				return -1;
			} else if (dcqattr.state != IBV_EXP_DCT_STATE_ACTIVE) {
				printf("state is not active %d\n", dcqattr.state);
				return -1;
			}
		}

		printf("local address: DCTN 0x%06x, SRQN 0x%06x, DCKEY 0x%016llx\n",
		       ctx.dct->dct_num, srqn, (unsigned long long)ctx.dct_key);
	}

	if (ibv_query_port(ctx.ctx, ctx.ib_port, &ctx.portinfo)) {
		fprintf(stderr, "Couldn't get port info\n");
		return 1;
	}

	ctx.thread_active = 1;
	err = pthread_create(&ctx.thread, NULL, handle_clients, &ctx);
	if (err) {
		perror("thread create faild:");
		return -1;
	}

	err = pthread_create(&ctx.poll_thread, NULL, poll_async, &ctx);
	if (err) {
		perror("thread create faild:");
		return -1;
	}

	for (i = 0; i < iters || iters == 0; ++i) {
		int num;
		struct ibv_wc wc;

		do {
			num = ibv_poll_cq(ctx.cq, 1, &wc);
			if (num < 0) {
				fprintf(stderr, "failed to poll cq\n");
				return -1;
			}
		} while (!num);
		if (wc.status != IBV_WC_SUCCESS) {
			fprintf(stderr, "completion with error:\n");
			fprintf(stderr, "status:     %d\n", wc.status);
		} else {
			if (post_recv(&ctx, 1) != 1) {
				fprintf(stderr, "failed to post receive buffer\n");
				return -1;
			}
		}
	}
	printf("test finished successfully\n");
	ctx.thread_active = 0;
	if (pthread_cancel(ctx.thread))
		printf("pthread_cancel failed\n");

	if (pthread_cancel(ctx.poll_thread))
		printf("pthread_cancel failed\n");

	err = pthread_join(ctx.thread, NULL);
	if (err) {
		perror("thread join faild:");
		return -1;
	}
	if (ibv_exp_destroy_dct(ctx.dct))
		printf("destroy dct failed\n");

	return 0;
}
