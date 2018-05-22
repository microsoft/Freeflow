/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
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

#ifndef IB_VERBS_H
#define IB_VERBS_H

#include <pthread.h>

#include <infiniband/driver.h>
#include <infiniband/driver_exp.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H

#  include <valgrind/memcheck.h>

#  ifndef VALGRIND_MAKE_MEM_DEFINED
#    warning "Valgrind support requested, but VALGRIND_MAKE_MEM_DEFINED not available"
#  endif

#endif /* HAVE_VALGRIND_MEMCHECK_H */

#ifndef VALGRIND_MAKE_MEM_DEFINED
#  define VALGRIND_MAKE_MEM_DEFINED(addr, len)
#endif

#define HIDDEN		__attribute__((visibility ("hidden")))

#define INIT		__attribute__((constructor))
#define FINI		__attribute__((destructor))

#define DEFAULT_ABI	"IBVERBS_1.1"

#ifdef HAVE_SYMVER_SUPPORT
#  define symver(name, api, ver) \
	asm(".symver " #name "," #api "@" #ver)
#  define default_symver(name, api) \
	asm(".symver " #name "," #api "@@" DEFAULT_ABI)
#else
#  define symver(name, api, ver)
#  define default_symver(name, api) \
	extern __typeof(name) api __attribute__((alias(#name)))
#endif /* HAVE_SYMVER_SUPPORT */

#define PFX		"libibverbs: "

#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

struct ibv_abi_compat_v2 {
	struct ibv_comp_channel	channel;
	pthread_mutex_t		in_use;
};

extern HIDDEN int abi_ver;

HIDDEN int ibverbs_get_device_list(struct ibv_device ***list);
HIDDEN int ibverbs_init();
HIDDEN struct ibv_mr *__ibv_reg_shared_mr(struct ibv_exp_reg_shared_mr_in *in);
HIDDEN struct ibv_mr *__ibv_exp_reg_mr(struct ibv_exp_reg_mr_in *in);
HIDDEN struct ibv_qp *ibv_find_xrc_qp(uint32_t qpn);

#define IBV_INIT_CMD(cmd, size, opcode)					\
	do {								\
		if (abi_ver > 2)					\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode;	\
		else							\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode##_V2; \
		(cmd)->in_words  = (size) / 4;				\
		(cmd)->out_words = 0;					\
	} while (0)

#define IBV_INIT_CMD_RESP(cmd, size, opcode, out, outsize)		\
	do {								\
		if (abi_ver > 2)					\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode;	\
		else							\
			(cmd)->command = IB_USER_VERBS_CMD_##opcode##_V2; \
		(cmd)->in_words  = (size) / 4;				\
		(cmd)->out_words = (outsize) / 4;			\
		(cmd)->response  = (uintptr_t) (out);			\
	} while (0)

#define IBV_INIT_CMD_RESP_EX_V(cmd, cmd_size, size, opcode, out, resp_size,\
		outsize)						   \
	do {                                                               \
		size_t c_size = cmd_size - sizeof(struct ex_hdr);	   \
		if (abi_ver > 2)					   \
			(cmd)->hdr.command = IB_USER_VERBS_CMD_##opcode;   \
		else							   \
			(cmd)->hdr.command =				   \
				IB_USER_VERBS_CMD_##opcode##_V2;	   \
		(cmd)->hdr.in_words  = ((c_size) / 8);                     \
		(cmd)->hdr.out_words = ((resp_size) / 8);                  \
		(cmd)->hdr.provider_in_words   = (((size) - (cmd_size))/8);\
		(cmd)->hdr.provider_out_words  =			   \
			     (((outsize) - (resp_size)) / 8);              \
		(cmd)->hdr.response  = (uintptr_t) (out);                  \
		(cmd)->hdr.reserved  = 0;				   \
	} while (0)

#define IBV_INIT_CMD_RESP_EX_VCMD(cmd, cmd_size, size, opcode, out, outsize) \
	IBV_INIT_CMD_RESP_EX_V(cmd, cmd_size, size, opcode, out,	     \
			sizeof(*(out)), outsize)

#define IBV_INIT_CMD_RESP_EX(cmd, size, opcode, out, outsize)		     \
	IBV_INIT_CMD_RESP_EX_V(cmd, sizeof(*(cmd)), size, opcode, out,    \
			sizeof(*(out)), outsize)

#define IBV_INIT_CMD_EX(cmd, size, opcode)				     \
	IBV_INIT_CMD_RESP_EX_V(cmd, sizeof(*(cmd)), size, opcode, NULL, 0, 0)

#define IBV_INIT_CMD_RESP_EXP(opcode, cmd, cmd_size, drv_size, out, osize,   \
			      drv_osize)				     \
	do {								     \
		size_t c_size = cmd_size - sizeof(struct ex_hdr);	     \
		(cmd)->hdr.command = IB_USER_VERBS_EXP_CMD_##opcode +	     \
				     IB_USER_VERBS_EXP_CMD_FIRST;	     \
		(cmd)->hdr.in_words  = (c_size / 8);			     \
		(cmd)->hdr.out_words = (osize / 8);			     \
		(cmd)->hdr.provider_in_words   = (drv_size  / 8);	     \
		(cmd)->hdr.provider_out_words  = (drv_osize / 8);	     \
		(cmd)->hdr.response  = (uintptr_t) (out);		     \
		(cmd)->hdr.reserved  = 0;				     \
	} while (0)

#define IBV_INIT_CMD_EXP(opcode, cmd, cmd_size, drv_size)		\
	IBV_INIT_CMD_RESP_EXP(opcode, cmd, cmd_size, drv_size, 0, 0, 0)

#ifndef uninitialized_var
#define uninitialized_var(x) x = x
#endif

void ibv_set_huge_safe(void);
#endif /* IB_VERBS_H */
