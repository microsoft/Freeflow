/*
 * Copyright (c) 2012 Topspin Communications.  All rights reserved.
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <malloc.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <infiniband/verbs.h>

#define DATA_BYTE_VALUE 0x7b

static int page_size;
static char	*servername;
static int no_rdma;

struct shared_mr_context {
	struct ibv_context	*context;
	struct ibv_pd		*pd;
	struct ibv_mr		*mr;
	void			*buf;
	size_t			size;
	int			sockfd;
};

struct shared_mr_info {
	uint32_t mr_handle;
};

static inline unsigned long align(unsigned long val, unsigned long align)
{
	return (val + align - 1) & ~(align - 1);
}

static struct shared_mr_info *shared_mr_client_exch_info(
					struct shared_mr_context *ctx,
					int port)
{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000"];
	int n;
	int sockfd = -1;
	struct shared_mr_info *rem_shared_mr_info = NULL;

	if (asprintf(&service, "%d", port) < 0)
		return NULL;

	n = getaddrinfo(servername, service, &hints, &res);

	if (n < 0) {
		fprintf(stderr, "%s for %s:%d\n", gai_strerror(n), servername,
			port);
		free(service);
		return NULL;
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
		fprintf(stderr, "Couldn't connect to %s:%d\n",
				servername, port);
		return NULL;
	}

	if (read(sockfd, msg, sizeof msg) != sizeof msg) {
		perror("client read");
		fprintf(stderr, "Couldn't read shared mr ID\n");
		goto out;
	}


	rem_shared_mr_info = malloc(sizeof *rem_shared_mr_info);
	if (!rem_shared_mr_info)
		goto out;

	sscanf(msg, "%x", &rem_shared_mr_info->mr_handle);
	ctx->sockfd = sockfd;
	return rem_shared_mr_info;

out:
	close(sockfd);
	return NULL;
}

static int shared_mr_server_exch_info(struct shared_mr_context *ctx,
				int port,
				const struct shared_mr_info *shared_mr_info)

{
	struct addrinfo *res, *t;
	struct addrinfo hints = {
		.ai_flags    = AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	char *service;
	char msg[sizeof "0000"];
	int n;
	int sockfd = -1, connfd;

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

			setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
				&n, sizeof n);

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

	sprintf(msg, "%04x", shared_mr_info->mr_handle);
	if (write(connfd, msg, sizeof msg) != sizeof msg) {
		fprintf(stderr, "Couldn't send shared mr ID\n");
		goto out;
	}

	ctx->sockfd = connfd;
	return 0;


out:
	close(connfd);
	return 1;
}

/* write some data on all buffer than send len to server */
static int shared_mr_client_write(struct shared_mr_context *ctx)
{

	memset(ctx->buf, DATA_BYTE_VALUE, ctx->size);
	if (write(ctx->sockfd, (char *)(&(ctx->size)), sizeof(ctx->size)) !=
		sizeof(ctx->size))
		return 1;
	fprintf(stderr, "shared_mr_client_write has succeeded\n");
	return 0;
}


static int shared_mr_server_read(struct shared_mr_context *ctx)
{
	char msg[sizeof(ctx->size)];
	size_t len;
	int i;
	char *data_buf;
	if (read(ctx->sockfd, msg, sizeof msg) != sizeof msg) {
		perror("server read");
		fprintf(stderr, "Couldn't read data len\n");
		return 1;
	}
	memcpy(&len, msg, sizeof len);
	if (ctx->size != len) {
		fprintf(stderr, "read data len missmatch (%zu,%zu)\n",
			len, ctx->size);
		return 1;
	}

	data_buf = ctx->buf;
	for (i = 0; i < len; i++) {
		if (data_buf[i] != DATA_BYTE_VALUE) {
			fprintf(stderr, "data mismatch, offset=%d\n", i);
			return 1;
		}
	}
	fprintf(stderr, "server - data match\n");
	return 0;
}
static int register_shared_mr(struct shared_mr_context *ctx,
				struct shared_mr_info *shared_mr_info)
{
	struct ibv_mr	*shared_mr;
	uint64_t access = IBV_EXP_ACCESS_LOCAL_WRITE;
	struct ibv_exp_reg_shared_mr_in shared_mr_in;

	memset(&shared_mr_in, 0, sizeof(shared_mr_in));
	shared_mr_in.mr_handle = shared_mr_info->mr_handle;
	shared_mr_in.pd = ctx->pd;
	/* shared_mr_in.addr is NULL as part of memset */

	if (no_rdma)
		access |= IBV_EXP_ACCESS_NO_RDMA;

	shared_mr_in.exp_access = access;
	shared_mr = ibv_exp_reg_shared_mr(&shared_mr_in);
	if (!shared_mr) {
		fprintf(stderr, "Failed via reg shared mr errno=%d\n", errno);
		return 1;
	}

	fprintf(stderr, "client registered successfully to shared mr %s\n",
		no_rdma ? "(non rdma)" : "");

	ctx->mr = shared_mr;
	ctx->buf = shared_mr->addr;
	ctx->size = shared_mr->length;
	return 0;
}

static int create_shared_mr(struct shared_mr_context *ctx,
				struct shared_mr_info *shared_mr_info)
{

	struct ibv_exp_reg_mr_in in;

	in.pd = ctx->pd;
	in.addr = ctx->buf;
	in.length = ctx->size;
	in.exp_access = IBV_EXP_ACCESS_LOCAL_WRITE |
			IBV_EXP_ACCESS_SHARED_MR_USER_WRITE |
			IBV_EXP_ACCESS_SHARED_MR_USER_READ;
	if (ctx->buf) {
		in.comp_mask = IBV_EXP_REG_MR_CREATE_FLAGS;
		in.create_flags = IBV_EXP_REG_MR_CREATE_CONTIG;
	} else {
		in.exp_access |= IBV_EXP_ACCESS_ALLOCATE_MR;
		in.comp_mask = 0;
	}

	ctx->mr = ibv_exp_reg_mr(&in);

	if (!ctx->mr) {
		fprintf(stderr, "Couldn't register MR\n");
		return 1;
	}

	ctx->buf = ctx->mr->addr;
	shared_mr_info->mr_handle = ctx->mr->handle;
	return 0;
}

static struct shared_mr_context *shared_mr_init_ctx(struct ibv_device *ib_dev,
						    size_t size, void *contig_addr)
{
	struct shared_mr_context *ctx;

	ctx = calloc(1, sizeof *ctx);
	if (!ctx)
		return NULL;

	ctx->size = size;
	ctx->buf  = contig_addr;

	ctx->context = ibv_open_device(ib_dev);
	if (!ctx->context) {
		fprintf(stderr, "Couldn't get context for %s\n",
			ibv_get_device_name(ib_dev));
		goto clean_ctx;
	}

	ctx->pd = ibv_alloc_pd(ctx->context);
	if (!ctx->pd) {
		fprintf(stderr, "Couldn't allocate PD\n");
		goto clean_device;
	}

	return ctx;

clean_device:
	ibv_close_device(ctx->context);

clean_ctx:
	free(ctx);
	return NULL;
}

int shared_mr_close_ctx(struct shared_mr_context *ctx)
{

	if (ctx->mr) {
		if (ibv_dereg_mr(ctx->mr)) {
			fprintf(stderr, "Couldn't deregister MR\n");
			return 1;
		}
	}

	if (ctx->pd) {
		if (ibv_dealloc_pd(ctx->pd)) {
			fprintf(stderr, "Couldn't deallocate PD\n");
			return 1;
		}
	}


	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	if (ctx->sockfd)
		close(ctx->sockfd);

	free(ctx);
	return 0;
}


static void usage(const char *argv0)
{
	printf("Usage:\n");
	printf("  %s            start a server and wait for connection\n",
						argv0);
	printf("  %s <host>     connect to server at <host>\n",
						argv0);
	printf("\n");
	printf("Options:\n");
	printf("  -p, --port=<port>      listen on/connect to port <port> (default 18515)\n");
	printf("  -d, --ib-dev=<dev>     use IB device <dev> (default first device found)\n");
	printf("  -s, --size=<size>      size of message to exchange (default 4096)\n");
	printf("  -n, --no-rdma          no rdma on shared mr\n");
	printf("  -a, --contig_addr      ask for specific contiguous addr\n");

}

int main(int argc, char *argv[])
{
	struct ibv_device      **dev_list;
	struct ibv_device	*ib_dev;
	struct shared_mr_context *ctx;
	struct shared_mr_info     shared_mr_info;
	struct shared_mr_info    *rem_shared_mr_info = NULL;
	char                    *ib_devname = NULL;
	int                      port = 18515;
	size_t                   size = 4096;
	int rc = 0;
	void *contig_addr = NULL;


	while (1) {
		int c;

		static struct option long_options[] = {
			{ .name = "port",		.has_arg = 1, .val = 'p' },
			{ .name = "ib-dev",		.has_arg = 1, .val = 'd' },
			{ .name = "size",		.has_arg = 1, .val = 's' },
			{ .name = "no-rdma",		.has_arg = 0, .val = 'n' },
			{ .name = "contig_addr",	.has_arg = 1, .val = 'a' },
			{ 0 }
		};

		c = getopt_long(argc, argv, "p:d:s:na:", long_options, NULL);
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

		case 'd':
			ib_devname = strdup(optarg);
			break;

		case 's':
			size = strtoull(optarg, NULL, 0);
			break;

		case 'n':
			++no_rdma;
			break;

		case 'a':
			contig_addr = (void *)(uintptr_t)strtoull(optarg, NULL, 0);
			break;

		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind == argc - 1)
		servername = strdup(argv[optind]);
	else if (optind < argc) {
		usage(argv[0]);
		return 1;
	}

	page_size = sysconf(_SC_PAGESIZE);
	size = align(size, page_size);

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
			if (!strcmp(ibv_get_device_name(dev_list[i]),
					ib_devname))
				break;
		ib_dev = dev_list[i];
		if (!ib_dev) {
			fprintf(stderr, "IB device %s not found\n", ib_devname);
			return 1;
		}
	}

	ctx = shared_mr_init_ctx(ib_dev, size, contig_addr);
	if (!ctx)
		return 1;

	if (servername) {
		rem_shared_mr_info = shared_mr_client_exch_info(ctx, port);
		if (!rem_shared_mr_info) {
			rc = 1;
			goto cleanup;
		}

		if (register_shared_mr(ctx, rem_shared_mr_info)) {
			rc = 1;
			goto cleanup;
		}

		if (shared_mr_client_write(ctx)) {
			rc = 1;
			goto cleanup;

		}
	} else {
		if (create_shared_mr(ctx, &shared_mr_info)) {
			rc = 1;
			goto cleanup;
		}
		if (shared_mr_server_exch_info(ctx, port, &shared_mr_info)) {
			rc = 1;
			goto cleanup;
		}
		if (shared_mr_server_read(ctx)) {
			rc = 1;
			goto cleanup;

		}
	}


cleanup:

	if (shared_mr_close_ctx(ctx))
		return 1;

	ibv_free_device_list(dev_list);
	if (rem_shared_mr_info)
		free(rem_shared_mr_info);

	return rc;
}
