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
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include "dc.h"

struct dc_ctx {
	struct ibv_qp		*qp;
	struct ibv_cq		*cq;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	struct ibv_ah		*ah;
	struct ibv_context	*ctx;
	void			*addr;
	size_t			length;
	int			port;
	int			lid;
	uint64_t		remote_dct_key;
	uint64_t		dct_key;
	int			local_key_defined;
	uint32_t		dct_number;
	struct ibv_port_attr	portinfo;
	int			ib_port;
	enum ibv_mtu		mtu;
	int			sl;
	uint16_t		gid_index;
	int			use_gid;
	union ibv_gid		dgid;
};

static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s <host>     connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>      listen on/connect to port <port> (default 18515)\n");
	printf("  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)\n");
	printf("  -i, --ib-port=<port>   use port <port> of IB device (default 1)\n");
	printf("  -s, --size=<size>      size of message to exchange (default 4096)\n");
	printf("  -n, --iters=<iters>    number of exchanges (default 1000)\n");
	printf("  -e, --events           sleep on CQ events (default poll)\n");
	printf("  -c, --contiguous-mr    use contiguous mr\n");
	printf("  -k, --dc-key           DC transport key\n");
	printf("  -m, --mtu              MTU of the DCI\n");
	printf("  -a, --check-nop        check NOP opcode\n");
	printf("  -g, --gid-index        gid index\n");
	printf("  -r, --dgid             remote gid. must be given if -g is used\n");
	printf("  -l, --sl               service level\n");
}

int send_nop(struct dc_ctx *ctx)
{
	struct ibv_exp_send_wr *bad_wr;
	struct ibv_exp_send_wr wr;
	struct ibv_exp_wc wc;
	int err;
	int n;

	memset(&wr, 0, sizeof(wr));

	wr.num_sge		= 0;
	wr.exp_opcode		= IBV_EXP_WR_NOP;
	wr.exp_send_flags	= IBV_EXP_SEND_SIGNALED;

	err = ibv_exp_post_send(ctx->qp, &wr, &bad_wr);
	if (err) {
		fprintf(stderr, "post nop failed\n");
		return err;
	}

	do {
		n = ibv_exp_poll_cq(ctx->cq, 1, &wc, sizeof(wc));
		if (n < 0) {
			fprintf(stderr, "poll CQ failed %d\n", n);
			return -1;
		}
	} while (!n);

	if (wc.status != IBV_WC_SUCCESS) {
		fprintf(stderr, "completion with error %d\n", wc.status);
		return -1;
	}

	return 0;
}

static int to_rts(struct dc_ctx *ctx)
{
	struct ibv_exp_qp_attr attr = {
		.qp_state        = IBV_QPS_INIT,
		.pkey_index      = 0,
		.port_num        = ctx->ib_port,
		.qp_access_flags = 0,
		.dct_key = ctx->dct_key,
	};

	if (ibv_exp_modify_qp(ctx->qp, &attr,
			      IBV_EXP_QP_STATE		|
			      IBV_EXP_QP_PKEY_INDEX	|
			      IBV_EXP_QP_PORT		|
			      IBV_EXP_QP_DC_KEY)) {
		fprintf(stderr, "Failed to modify QP to INIT\n");
		return 1;
	}

	attr.qp_state		= IBV_QPS_RTR;
	attr.max_dest_rd_atomic	= 0;
	attr.path_mtu		= ctx->mtu;
	attr.ah_attr.is_global	= !!ctx->use_gid;
	if (ctx->use_gid) {
		attr.ah_attr.grh.sgid_index = ctx->gid_index;
		attr.ah_attr.grh.hop_limit = 1;
		attr.ah_attr.grh.dgid = ctx->dgid;
	}

	attr.ah_attr.dlid	= ctx->lid;
	attr.ah_attr.port_num	= ctx->ib_port;
	attr.ah_attr.sl		= ctx->sl;
	attr.dct_key		= ctx->dct_key;

	if (ibv_exp_modify_qp(ctx->qp, &attr, IBV_EXP_QP_STATE			|
					      IBV_EXP_QP_PATH_MTU		|
					      IBV_EXP_QP_AV)) {
		fprintf(stderr, "Failed to modify QP to RTR\n");
		return 1;
	}

	attr.qp_state	    = IBV_QPS_RTS;
	attr.timeout	    = 14;
	attr.retry_cnt	    = 7;
	attr.rnr_retry	    = 7;
	attr.max_rd_atomic  = 1;
	if (ibv_exp_modify_qp(ctx->qp, &attr, IBV_EXP_QP_STATE	|
					      IBV_EXP_QP_TIMEOUT	|
					      IBV_EXP_QP_RETRY_CNT	|
					      IBV_EXP_QP_RNR_RETRY	|
					      IBV_EXP_QP_MAX_QP_RD_ATOMIC)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	return 0;
}

int pp_client_exch_dest(struct dc_ctx *ctx, const char *servername, int port)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof(MSG_FORMAT)];
	int n;
	int sockfd = -1;
	int err;

	if (asprintf(&service, "%d", port) < 0)
		return -1;

	n = getaddrinfo(servername, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername, port);
		free(service);
		return -1;
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
		return -1;
	}

	sprintf(msg, "%06x:%04x:0000000000000000", ctx->qp->qp_num, ctx->portinfo.lid);
	if (write(sockfd, msg, sizeof(msg)) != sizeof(msg)) {
		fprintf(stderr, "Couldn't send local address\n");
		err = -1;
	}

	err = read(sockfd, msg, sizeof(msg));
	if (err != sizeof(msg)) {
		perror("client read");
		fprintf(stderr, "Read %d/%zu\n", err, sizeof(msg));
		err = -1;
		goto out;
	}

	sscanf(msg, "%06x:%04x:%016" SCNx64, &ctx->dct_number, &ctx->lid, &ctx->remote_dct_key);
	printf("Remote address: DCTN %06x, LID %04x, DCT key %016" PRIx64 "\n",
	       ctx->dct_number, ctx->lid, ctx->remote_dct_key);

	if (!ctx->local_key_defined)
		ctx->dct_key = ctx->remote_dct_key;

	return 0;

out:
	close(sockfd);
	return err;
}

int main(int argc, char *argv[])
{
	struct ibv_device	**dev_list;
	struct ibv_device	*ib_dev;
	char			*ib_devname = NULL;
	int			port = 18515;
	int			size = 4096;
	int			iters = 1000;
	int			use_event = 0;
	int			use_contig_mr;
	int			err;
	struct ibv_ah_attr	ah_attr;
	struct dc_ctx		ctx = {
		.ib_port	= 1,
		.mtu		= IBV_MTU_2048,
		.sl		= 0,
	};
	struct ibv_exp_send_wr wr;
	struct ibv_exp_send_wr *bad_wr;
	struct ibv_sge	sg_list;
	int i;
	char                    *servername = NULL;
	enum ibv_mtu		mtu;
	int			check_nop = 0;
	int			dgid_given = 0;

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
			{ .name = "contig-mr",  .has_arg = 0, .val = 'c' },
			{ .name = "dc-key",     .has_arg = 1, .val = 'k' },
			{ .name = "mtu",	.has_arg = 1, .val = 'm' },
			{ .name = "check-nop",	.has_arg = 0, .val = 'a' },
			{ .name = "sl",		.has_arg = 1, .val = 'l' },
			{ .name = "gid-index",  .has_arg = 1, .val = 'g' },
			{ .name = "dgid",       .has_arg = 1, .val = 'r' },
			{ 0 }
		};

		c = getopt_long(argc, argv, "p:d:i:s:n:ect:k:m:al:g:r:",
				long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'p':
			port = strtol(optarg, NULL, 0);
			if (port < 0 || port > 65535) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 'k':
			ctx.dct_key = strtoull(optarg, NULL, 0);
			ctx.local_key_defined = 1;
			break;

		case 'd':
			ib_devname = strdupa(optarg);
			break;

		case 'i':
			ctx.ib_port = strtol(optarg, NULL, 0);
			if (ctx.ib_port < 0) {
				usage(argv[0]);
				return 1;
			}
			break;

		case 's':
			size = strtol(optarg, NULL, 0);
			break;

		case 'n':
			iters = strtol(optarg, NULL, 0);
			break;

		case 'e':
			++use_event;
			break;

		case 'c':
			++use_contig_mr;
			break;

		case 'm':
			mtu = strtol(optarg, NULL, 0);
			if (to_ib_mtu(mtu, &ctx.mtu)) {
				printf("invalid MTU %d\n", mtu);
				return 1;
			}
			break;

		case 'a':
			check_nop = 1;
			break;

		case 'l':
			ctx.sl = strtol(optarg, NULL, 0);
			break;

		case 'g':
			ctx.gid_index = strtol(optarg, NULL, 0);
			ctx.use_gid = 1;
			break;

		case 'r':
			if (!inet_pton(AF_INET6, optarg, &ctx.dgid)) {
				usage(argv[0]);
				return 1;
			}
			dgid_given = 1;
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind == argc - 1) {
		servername = strdupa(argv[optind]);
		if (ctx.use_gid && !dgid_given) {
			usage(argv[0]);
			return 1;
		}
	} else if (optind < argc) {
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

	ctx.pd = ibv_alloc_pd(ctx.ctx);
	if (!ctx.pd) {
		fprintf(stderr, "failed to allocate pd\n");
		return 1;
	}

	ctx.length = size;
	ctx.addr = malloc(ctx.length);
	if (!ctx.addr) {
		fprintf(stderr, "failed to allocate memory\n");
		return -1;
	}

	if (ibv_query_port(ctx.ctx, ctx.ib_port, &ctx.portinfo)) {
		fprintf(stderr, "Couldn't get port info\n");
		return 1;
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
		struct ibv_qp_init_attr_ex attr = {
			.send_cq = ctx.cq,
			.recv_cq = ctx.cq,
			.cap     = {
				.max_send_wr  = 100,
				.max_send_sge = 1,
			},
			.qp_type = IBV_EXP_QPT_DC_INI,
			.pd = ctx.pd,
			.comp_mask = IBV_QP_INIT_ATTR_PD,
		};

		ctx.qp = ibv_create_qp_ex(ctx.ctx, &attr);
		if (!ctx.qp) {
			fprintf(stderr, "failed to create qp\n");
			return -1;
		}
	}

	if (pp_client_exch_dest(&ctx, servername, port)) {
		printf("failed to connect to target\n");
		return -1;
	}

	printf("local address: LID %04x, QPN %06x, DC_KEY %016" PRIx64 "\n",
	       ctx.portinfo.lid, ctx.qp->qp_num, ctx.dct_key);

	memset(&ah_attr, 0, sizeof(ah_attr));
	ah_attr.is_global     = 0;
	ah_attr.dlid	      = ctx.lid;
	ah_attr.sl            = ctx.sl;
	ah_attr.src_path_bits = 0;
	ah_attr.port_num      = ctx.ib_port;
	if (ctx.use_gid) {
		ah_attr.is_global = 1;
		ah_attr.grh.hop_limit = 1;
		ah_attr.grh.sgid_index = ctx.gid_index;
		ah_attr.grh.dgid = ctx.dgid;
	}
	ctx.ah = ibv_create_ah(ctx.pd, &ah_attr);
	if (!ctx.ah) {
		fprintf(stderr, "failed to create ah\n");
		return -1;
	}

	err = to_rts(&ctx);
	if (err) {
		fprintf(stderr, "failed to move to rts\n");
		return -1;
	}


	if (check_nop) {
		err = send_nop(&ctx);
		if (err) {
			fprintf(stderr, "nop operation failed\n");
			return err;
		}
	}

	for (i = 0; i < iters; ++i) {
		memset(&wr, 0, sizeof(wr));
		wr.num_sge = 1;
		wr.exp_opcode = IBV_EXP_WR_SEND;
		wr.exp_send_flags = IBV_EXP_SEND_SIGNALED;
		sg_list.addr = (uint64_t)(unsigned long)ctx.addr;
		sg_list.length = ctx.length;
		sg_list.lkey = ctx.mr->lkey;
		wr.sg_list = &sg_list;
		wr.dc.ah = ctx.ah;
		wr.dc.dct_access_key = ctx.dct_key;
		wr.dc.dct_number = ctx.dct_number;

		err = ibv_exp_post_send(ctx.qp, &wr, &bad_wr);
		if (err) {
			fprintf(stderr, "failed to post send request\n");
			return -1;
		} else {
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
				fprintf(stderr, "completion with error %d\n", wc.status);
				return -1;
			}
		}
	}
	printf("test finished successfully\n");

	return 0;
}
