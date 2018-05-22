/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems, Inc.  All rights reserved.
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
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <alloca.h>
#include <string.h>

#include "ibverbs.h"


/*
 * cmd.c experimental functions
 */
int ibv_exp_cmd_query_device(struct ibv_context *context,
			     struct ibv_exp_device_attr *device_attr,
			     uint64_t *raw_fw_ver,
			     struct ibv_exp_query_device *cmd, size_t cmd_size)
{
	struct ibv_exp_query_device_resp resp;
	struct ibv_query_device_resp *r_resp;
	uint32_t comp_mask = 0;

	memset(&resp, 0, sizeof(resp));
	r_resp = IBV_RESP_TO_VERBS_RESP_EX(&resp,
					   struct ibv_exp_query_device_resp,
					   struct ibv_query_device_resp);

	memset(cmd, 0, sizeof(*cmd));
	cmd->comp_mask = device_attr->comp_mask;
	IBV_INIT_CMD_RESP_EXP(QUERY_DEVICE, cmd, cmd_size, 0,
			      &resp, sizeof(resp), 0);
	if (write(context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(&resp, sizeof(resp));
	memset(device_attr->fw_ver, 0, sizeof(device_attr->fw_ver));
	copy_query_dev_fields((struct ibv_device_attr *)device_attr, r_resp,
				    raw_fw_ver);

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_WITH_TIMESTAMP_MASK) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_WITH_TIMESTAMP_MASK)) {
		device_attr->timestamp_mask = resp.timestamp_mask;
		comp_mask |= IBV_EXP_DEVICE_ATTR_WITH_TIMESTAMP_MASK;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_WITH_HCA_CORE_CLOCK) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_WITH_HCA_CORE_CLOCK)) {
		device_attr->hca_core_clock = resp.hca_core_clock;
		comp_mask |= IBV_EXP_DEVICE_ATTR_WITH_HCA_CORE_CLOCK;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS)) {
		device_attr->exp_device_cap_flags = (uint64_t)(((struct ibv_device_attr *)device_attr)->device_cap_flags);
		device_attr->exp_device_cap_flags |= resp.device_cap_flags2 << IBV_EXP_START_FLAG_LOC;
		comp_mask |= IBV_EXP_DEVICE_ATTR_EXP_CAP_FLAGS;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_DC_RD_REQ) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_DC_RD_REQ)) {
		device_attr->max_dc_req_rd_atom = resp.dc_rd_req;
		comp_mask |= IBV_EXP_DEVICE_DC_RD_REQ;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_DC_RD_RES) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_DC_RD_RES)) {
		device_attr->max_dc_res_rd_atom = resp.dc_rd_res;
		comp_mask |= IBV_EXP_DEVICE_DC_RD_RES;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_MAX_DCT) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_MAX_DCT)) {
		device_attr->max_dct = resp.max_dct;
		comp_mask |= IBV_EXP_DEVICE_ATTR_MAX_DCT;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ)) {
		device_attr->inline_recv_sz = resp.inline_recv_sz;
		comp_mask |= IBV_EXP_DEVICE_ATTR_INLINE_RECV_SZ;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_RSS_TBL_SZ) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_RSS_TBL_SZ)) {
		device_attr->max_rss_tbl_sz = resp.max_rss_tbl_sz;
		comp_mask |= IBV_EXP_DEVICE_ATTR_RSS_TBL_SZ;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_EXT_ATOMIC_ARGS) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_EXT_ATOMIC_ARGS)) {
		comp_mask |= IBV_EXP_DEVICE_ATTR_EXT_ATOMIC_ARGS;
		device_attr->ext_atom.log_atomic_arg_sizes = resp.log_atomic_arg_sizes;
		device_attr->ext_atom.max_fa_bit_boundary = resp.max_fa_bit_boundary;
		device_attr->ext_atom.log_max_atomic_inline = resp.log_max_atomic_inline;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_UMR) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_UMR)) {
		device_attr->umr_caps.max_klm_list_size = resp.umr_caps.max_klm_list_size;
		device_attr->umr_caps.max_send_wqe_inline_klms = resp.umr_caps.max_send_wqe_inline_klms;
		device_attr->umr_caps.max_umr_recursion_depth = resp.umr_caps.max_umr_recursion_depth;
		device_attr->umr_caps.max_umr_stride_dimension = resp.umr_caps.max_umr_stride_dimension;
		comp_mask |= IBV_EXP_DEVICE_ATTR_UMR;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_ODP) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_ODP)) {
		device_attr->odp_caps.general_odp_caps = resp.odp_caps.general_odp_caps;
		device_attr->odp_caps.per_transport_caps.rc_odp_caps =
			resp.odp_caps.per_transport_caps.rc_odp_caps;
		device_attr->odp_caps.per_transport_caps.uc_odp_caps =
			resp.odp_caps.per_transport_caps.uc_odp_caps;
		device_attr->odp_caps.per_transport_caps.ud_odp_caps =
			resp.odp_caps.per_transport_caps.ud_odp_caps;
		device_attr->odp_caps.per_transport_caps.dc_odp_caps =
			resp.odp_caps.per_transport_caps.dc_odp_caps;
		device_attr->odp_caps.per_transport_caps.xrc_odp_caps =
			resp.odp_caps.per_transport_caps.xrc_odp_caps;
		device_attr->odp_caps.per_transport_caps.raw_eth_odp_caps =
			resp.odp_caps.per_transport_caps.raw_eth_odp_caps;
		comp_mask |= IBV_EXP_DEVICE_ATTR_ODP;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_MAX_CTX_RES_DOMAIN) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_MAX_CTX_RES_DOMAIN)) {
		device_attr->max_ctx_res_domain = resp.max_ctx_res_domain;
		comp_mask |= IBV_EXP_DEVICE_ATTR_MAX_CTX_RES_DOMAIN;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_MAX_WQ_TYPE_RQ) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_MAX_WQ_TYPE_RQ)) {
		device_attr->max_wq_type_rq = resp.max_wq_type_rq;
		comp_mask |= IBV_EXP_DEVICE_ATTR_MAX_WQ_TYPE_RQ;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_RX_HASH) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_RX_HASH)) {
		device_attr->rx_hash_caps.max_rwq_indirection_tables = resp.rx_hash.max_rwq_indirection_tables;
		device_attr->rx_hash_caps.max_rwq_indirection_table_size = resp.rx_hash.max_rwq_indirection_table_size;
		device_attr->rx_hash_caps.supported_hash_functions = resp.rx_hash.supported_hash_functions;
		device_attr->rx_hash_caps.supported_packet_fields = resp.rx_hash.supported_packet_fields;
		device_attr->rx_hash_caps.supported_qps = resp.rx_hash.supported_qps;
		comp_mask |= IBV_EXP_DEVICE_ATTR_RX_HASH;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_MAX_DEVICE_CTX) &&
		(resp.comp_mask & IBV_EXP_DEVICE_ATTR_MAX_DEVICE_CTX)) {
		device_attr->max_device_ctx = resp.max_device_ctx;
		comp_mask |= IBV_EXP_DEVICE_ATTR_MAX_DEVICE_CTX;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_MP_RQ) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_MP_RQ)) {
		device_attr->mp_rq_caps.allowed_shifts = resp.mp_rq_caps.allowed_shifts;
		device_attr->mp_rq_caps.supported_qps = resp.mp_rq_caps.supported_qps;
		device_attr->mp_rq_caps.max_single_stride_log_num_of_bytes = resp.mp_rq_caps.max_single_stride_log_num_of_bytes;
		device_attr->mp_rq_caps.min_single_stride_log_num_of_bytes = resp.mp_rq_caps.min_single_stride_log_num_of_bytes;
		device_attr->mp_rq_caps.max_single_wqe_log_num_of_strides = resp.mp_rq_caps.max_single_wqe_log_num_of_strides;
		device_attr->mp_rq_caps.min_single_wqe_log_num_of_strides = resp.mp_rq_caps.min_single_wqe_log_num_of_strides;
		comp_mask |= IBV_EXP_DEVICE_ATTR_MP_RQ;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_VLAN_OFFLOADS) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_VLAN_OFFLOADS)) {
		device_attr->wq_vlan_offloads_cap = resp.wq_vlan_offloads_cap;
		comp_mask |= IBV_EXP_DEVICE_ATTR_VLAN_OFFLOADS;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_EC_CAPS) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_EC_CAPS)) {
		device_attr->ec_caps.max_ec_calc_inflight_calcs =
				resp.ec_caps.max_ec_calc_inflight_calcs;
		device_attr->ec_caps.max_ec_data_vector_count =
				resp.ec_caps.max_ec_data_vector_count;
		comp_mask |= IBV_EXP_DEVICE_ATTR_EC_CAPS;
		if (device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_EC_GF_BASE) {
			device_attr->ec_w_mask = resp.ec_w_mask;
			comp_mask |= IBV_EXP_DEVICE_ATTR_EC_GF_BASE;
		}
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_MASKED_ATOMICS) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_MASKED_ATOMICS)) {
		comp_mask |= IBV_EXP_DEVICE_ATTR_MASKED_ATOMICS;
		device_attr->masked_atomic.masked_log_atomic_arg_sizes =
			resp.masked_atomic_caps.masked_log_atomic_arg_sizes;
		device_attr->masked_atomic.masked_log_atomic_arg_sizes_network_endianness =
			resp.masked_atomic_caps.masked_log_atomic_arg_sizes_network_endianness;
		device_attr->masked_atomic.max_fa_bit_boundary =
			resp.max_fa_bit_boundary;
		device_attr->masked_atomic.log_max_atomic_inline =
			resp.log_max_atomic_inline;
	}
	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_RX_PAD_END_ALIGN) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_RX_PAD_END_ALIGN)) {
		device_attr->rx_pad_end_addr_align = resp.rx_pad_end_addr_align;
		comp_mask |= IBV_EXP_DEVICE_ATTR_RX_PAD_END_ALIGN;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_TSO_CAPS) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_TSO_CAPS)) {
		device_attr->tso_caps.max_tso = resp.tso_caps.max_tso;
		device_attr->tso_caps.supported_qpts =
				resp.tso_caps.supported_qpts;
		comp_mask |= IBV_EXP_DEVICE_ATTR_TSO_CAPS;
	}

	if ((device_attr->comp_mask & IBV_EXP_DEVICE_ATTR_PACKET_PACING_CAPS) &&
	    (resp.comp_mask & IBV_EXP_DEVICE_ATTR_PACKET_PACING_CAPS)) {
		device_attr->packet_pacing_caps.qp_rate_limit_min =
			resp.packet_pacing_caps.qp_rate_limit_min;
		device_attr->packet_pacing_caps.qp_rate_limit_max =
			resp.packet_pacing_caps.qp_rate_limit_max;
		device_attr->packet_pacing_caps.supported_qpts =
			resp.packet_pacing_caps.supported_qpts;
		comp_mask |= IBV_EXP_DEVICE_ATTR_PACKET_PACING_CAPS;
	}

	device_attr->comp_mask = comp_mask;

	return 0;
}

int ibv_exp_cmd_create_qp(struct ibv_context *context,
			  struct verbs_qp *qp, int vqp_sz,
			  struct ibv_exp_qp_init_attr *attr_exp,
			  void *cmd_buf, size_t lib_cmd_size, size_t drv_cmd_size,
			  void *resp_buf, size_t lib_resp_size, size_t drv_resp_size,
			  int force_exp)
{
	struct verbs_xrcd *vxrcd = NULL;
	struct ibv_exp_create_qp	*cmd_exp = NULL;
	struct ibv_exp_create_qp_resp	*resp_exp = NULL;
	struct ibv_create_qp		*cmd;
	struct ibv_create_qp_resp	*resp;
	int wsize;

	if (attr_exp->comp_mask >= IBV_EXP_QP_INIT_ATTR_RESERVED1)
		return ENOSYS;

	cmd = cmd_buf;
	resp = resp_buf;

	if (attr_exp->comp_mask >= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS || force_exp) {
		cmd_exp = cmd_buf;
		resp_exp = resp_buf;
		wsize = lib_cmd_size + drv_cmd_size;

		/*
		 * Cast extended command to legacy command using a fact
		 * that legacy header size is equal 'comp_mask' field size
		 * and 'comp_mask' field position is on top of the valuable
		 * fields
		 */
		cmd = (struct ibv_create_qp *)((void *)&cmd_exp->comp_mask - sizeof(cmd->response));
		/*
		 * Cast extended response to legacy response using a fact
		 * that 'comp_mask' field is added on top of legacy response
		 */
		resp = (struct ibv_create_qp_resp *)
				((uint8_t *)resp_exp +
					sizeof(resp_exp->comp_mask));

		IBV_INIT_CMD_RESP_EXP(CREATE_QP, cmd_exp, lib_cmd_size, drv_cmd_size,
				      resp_exp, lib_resp_size, drv_resp_size);
	} else {
		wsize = lib_cmd_size + drv_cmd_size;
		IBV_INIT_CMD_RESP(cmd, wsize, CREATE_QP, resp, lib_resp_size + drv_resp_size);
	}

	cmd->user_handle     = (uintptr_t) qp;

	if (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_XRCD) {
		/* XRC reciever side */
		vxrcd = container_of(attr_exp->xrcd, struct verbs_xrcd, xrcd);
		cmd->pd_handle	= vxrcd->handle;
	} else {
		if (!(attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_PD))
			return EINVAL;

		cmd->pd_handle	= attr_exp->pd->handle;
		if (!(attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_RX_HASH))
			cmd->send_cq_handle = attr_exp->send_cq->handle;
		/* XRC sender doesn't have a recieve cq */
		if (attr_exp->qp_type != IBV_QPT_XRC_SEND &&
			attr_exp->qp_type != IBV_QPT_XRC &&
			attr_exp->qp_type != IBV_EXP_QPT_DC_INI &&
			!(attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_RX_HASH)) {
			cmd->recv_cq_handle = attr_exp->recv_cq->handle;
			cmd->srq_handle = attr_exp->srq ? attr_exp->srq->handle : 0;
		}
	}

	cmd->max_send_wr     = attr_exp->cap.max_send_wr;
	cmd->max_recv_wr     = attr_exp->cap.max_recv_wr;
	cmd->max_send_sge    = attr_exp->cap.max_send_sge;
	cmd->max_recv_sge    = attr_exp->cap.max_recv_sge;
	cmd->max_inline_data = attr_exp->cap.max_inline_data;
	cmd->sq_sig_all          = attr_exp->sq_sig_all;
	cmd->qp_type            = (attr_exp->qp_type == IBV_QPT_XRC) ?
				IBV_QPT_XRC_SEND : attr_exp->qp_type;
	cmd->is_srq	       = !!attr_exp->srq;
	cmd->reserved	     = 0;

	if (cmd_exp) {
		cmd_exp->comp_mask = 0;
		if (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS) {
			if (attr_exp->exp_create_flags & ~IBV_EXP_QP_CREATE_MASK)
				return EINVAL;
			else {
				cmd_exp->comp_mask |= IBV_CREATE_QP_EX_CAP_FLAGS;
				cmd_exp->qp_cap_flags = attr_exp->exp_create_flags &
							IBV_EXP_CREATE_QP_KERNEL_FLAGS;
			}
		}
		if (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_INL_RECV) {
			cmd_exp->comp_mask |= IBV_EXP_CREATE_QP_INL_RECV;
			cmd_exp->max_inl_recv = attr_exp->max_inl_recv;
		}
		if (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_QPG) {
			struct ibv_exp_qpg *qpg = &attr_exp->qpg;

			switch (qpg->qpg_type) {
			case IBV_EXP_QPG_PARENT:
				cmd_exp->qpg.parent_attrib.rss_child_count =
					 qpg->parent_attrib.rss_child_count;
				cmd_exp->qpg.parent_attrib.tss_child_count =
					 qpg->parent_attrib.tss_child_count;
				break;
			case IBV_EXP_QPG_CHILD_RX:
			case IBV_EXP_QPG_CHILD_TX:
				cmd_exp->qpg.parent_handle =
							qpg->qpg_parent->handle;
				break;
			default:
				return -EINVAL;
			}
			cmd_exp->qpg.qpg_type = qpg->qpg_type;
			/* request a QP group */
			cmd_exp->comp_mask |= IBV_EXP_CREATE_QP_QPG;
		}

		if (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_MAX_INL_KLMS) {
			cmd_exp->max_inl_send_klms = attr_exp->max_inl_send_klms;
			cmd_exp->comp_mask |= IBV_EXP_CREATE_QP_MAX_INL_KLMS;
		}
		if (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_RX_HASH) {
			if (attr_exp->rx_hash_conf->rx_hash_key_len > sizeof(cmd_exp->rx_hash_info.rx_hash_key))
				return -EINVAL;

			cmd_exp->rx_hash_info.rx_hash_function = attr_exp->rx_hash_conf->rx_hash_function;
			cmd_exp->rx_hash_info.rx_hash_key_len = attr_exp->rx_hash_conf->rx_hash_key_len;
			cmd_exp->rx_hash_info.rx_hash_fields_mask = attr_exp->rx_hash_conf->rx_hash_fields_mask;
			memcpy(cmd_exp->rx_hash_info.rx_hash_key, attr_exp->rx_hash_conf->rx_hash_key,
			       attr_exp->rx_hash_conf->rx_hash_key_len);
			cmd_exp->rx_hash_info.rwq_ind_tbl_handle = attr_exp->rx_hash_conf->rwq_ind_tbl->ind_tbl_handle;
			cmd_exp->rx_hash_info.reserved = 0;
			/* no comp mask explicit bit is needed, hash function is used as an indicator */
		}
		if (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_PORT)
			cmd_exp->port_num = attr_exp->port_num;

		memset(cmd_exp->reserved_2, 0, sizeof(cmd_exp->reserved_2));
	}
	if (write(context->cmd_fd, cmd_buf, wsize) != wsize)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp_buf, lib_resp_size + drv_resp_size);

	if (abi_ver > 3) {
		attr_exp->cap.max_recv_sge    = resp->max_recv_sge;
		attr_exp->cap.max_send_sge    = resp->max_send_sge;
		attr_exp->cap.max_recv_wr     = resp->max_recv_wr;
		attr_exp->cap.max_send_wr     = resp->max_send_wr;
		attr_exp->cap.max_inline_data = resp->max_inline_data;
		if (resp_exp) {
			attr_exp->comp_mask &= IBV_EXP_QP_INIT_ATTR_RESERVED1 - 1;
			if ((resp_exp->comp_mask & IBV_EXP_CREATE_QP_RESP_INL_RECV) &&
			    (attr_exp->comp_mask & IBV_EXP_QP_INIT_ATTR_INL_RECV))
				attr_exp->max_inl_recv = resp_exp->max_inl_recv;
			else
				attr_exp->comp_mask &= ~IBV_EXP_QP_INIT_ATTR_INL_RECV;
		}
	}

	if (abi_ver == 4) {
		struct ibv_create_qp_resp_v4 *resp_v4 =
			(struct ibv_create_qp_resp_v4 *) resp;

		memmove((void *) resp + sizeof(*resp),
			(void *) resp_v4 + sizeof(*resp_v4),
			lib_resp_size - sizeof(*resp));
	} else if (abi_ver <= 3) {
		struct ibv_create_qp_resp_v3 *resp_v3 =
			(struct ibv_create_qp_resp_v3 *) resp;

		memmove((void *) resp + sizeof(*resp),
			(void *) resp_v3 + sizeof(*resp_v3),
			lib_resp_size - sizeof(*resp));
	}

	qp->qp.handle		= resp->qp_handle;
	qp->qp.qp_num		= resp->qpn;
	qp->qp.context		= context;
	qp->qp.qp_context	= attr_exp->qp_context;
	qp->qp.pd		= attr_exp->pd;
	qp->qp.send_cq		= attr_exp->send_cq;
	qp->qp.recv_cq		= attr_exp->recv_cq;
	qp->qp.srq		= attr_exp->srq;
	qp->qp.qp_type		= attr_exp->qp_type;
	qp->qp.state		= IBV_QPS_RESET;
	qp->qp.events_completed = 0;
	pthread_mutex_init(&qp->qp.mutex, NULL);
	pthread_cond_init(&qp->qp.cond, NULL);

	qp->comp_mask = 0;
	if (vext_field_avail(struct verbs_qp, xrcd, vqp_sz) &&
		(attr_exp->comp_mask & IBV_QP_INIT_ATTR_XRCD)) {
		qp->comp_mask |= VERBS_QP_XRCD;
		qp->xrcd = vxrcd;
	}

	return 0;
}

int ibv_exp_cmd_create_dct(struct ibv_context *context,
			   struct ibv_exp_dct *dct,
			   struct ibv_exp_dct_init_attr *attr,
			   struct ibv_exp_create_dct *cmd,
			   size_t lib_cmd_sz, size_t drv_cmd_sz,
			   struct ibv_exp_create_dct_resp *resp,
			   size_t lib_resp_sz, size_t drv_resp_sz)
{
	int wsize = lib_cmd_sz + drv_cmd_sz;

	IBV_INIT_CMD_RESP_EXP(CREATE_DCT, cmd, lib_cmd_sz, drv_cmd_sz, resp,
			      lib_resp_sz, drv_resp_sz);

	cmd->user_handle = (__u64)(uintptr_t)dct;
	cmd->pd_handle = attr->pd->handle;
	cmd->cq_handle = attr->cq->handle;
	cmd->srq_handle = attr->srq->handle;
	cmd->dc_key = attr->dc_key;
	cmd->port = attr->port;
	cmd->access_flags = attr->access_flags;
	cmd->min_rnr_timer = attr->min_rnr_timer;
	cmd->tclass = attr->tclass;
	cmd->flow_label = attr->flow_label;
	cmd->mtu = attr->mtu;
	cmd->pkey_index = attr->pkey_index;
	cmd->gid_index = attr->gid_index;
	cmd->hop_limit = attr->hop_limit;
	cmd->inline_size = attr->inline_size;
	if (~IBV_EXP_DCT_CREATE_FLAGS_MASK & attr->create_flags)
		return EINVAL;

	cmd->create_flags = attr->create_flags;
	if (write(context->cmd_fd, cmd, wsize) != wsize)
		goto err;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof(*resp));

	attr->inline_size = resp->inline_size;
	dct->events_completed = 0;
	pthread_mutex_init(&dct->mutex, NULL);
	pthread_cond_init(&dct->cond, NULL);

	return 0;

err:
	return errno;
}

int ibv_exp_cmd_destroy_dct(struct ibv_context *context,
			    struct ibv_exp_dct *dct,
			    struct ibv_exp_destroy_dct *cmd,
			    size_t lib_cmd_sz, size_t drv_cmd_sz,
			    struct ibv_exp_destroy_dct_resp *resp,
			    size_t lib_resp_sz, size_t drv_resp_sz)
{
	int wsize = lib_cmd_sz + drv_cmd_sz;

	IBV_INIT_CMD_RESP_EXP(DESTROY_DCT, cmd, lib_cmd_sz, drv_cmd_sz, resp, lib_resp_sz, drv_resp_sz);
	cmd->dct_handle = dct->handle;

	if (write(context->cmd_fd, cmd, wsize) != wsize)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof(*resp));
	pthread_mutex_lock(&dct->mutex);
	while (dct->events_completed != resp->events_reported)
		pthread_cond_wait(&dct->cond, &dct->mutex);
	pthread_mutex_unlock(&dct->mutex);

	return 0;
}

int ibv_exp_cmd_query_dct(struct ibv_context *context,
			  struct ibv_exp_query_dct *cmd,
			  size_t lib_cmd_sz, size_t drv_cmd_sz,
			  struct ibv_exp_query_dct_resp *resp,
			  size_t lib_resp_sz, size_t drv_resp_sz,
			  struct ibv_exp_dct_attr *attr)
{
	int wsize = lib_cmd_sz + drv_cmd_sz;

	IBV_INIT_CMD_RESP_EXP(QUERY_DCT, cmd, lib_cmd_sz, drv_cmd_sz, resp, lib_resp_sz, drv_resp_sz);

	if (write(context->cmd_fd, cmd, wsize) != wsize)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof(*resp));
	attr->dc_key = resp->dc_key;
	attr->port = resp->port;
	attr->access_flags = resp->access_flags;
	attr->min_rnr_timer = resp->min_rnr_timer;
	attr->tclass = resp->tclass;
	attr->flow_label = resp->flow_label;
	attr->mtu = resp->mtu;
	attr->pkey_index = resp->pkey_index;
	attr->gid_index = resp->gid_index;
	attr->hop_limit = resp->hop_limit;
	attr->key_violations = resp->key_violations;
	attr->state = resp->state;

	return 0;
}

int ibv_exp_cmd_arm_dct(struct ibv_context *context,
			struct ibv_exp_arm_attr *attr,
			struct ibv_exp_arm_dct *cmd,
			size_t lib_cmd_sz, size_t drv_cmd_sz,
			struct ibv_exp_arm_dct_resp *resp,
			size_t lib_resp_sz, size_t drv_resp_sz)
{
	int wsize = lib_cmd_sz + drv_cmd_sz;

	if (attr->comp_mask) {
		errno = EINVAL;
		return errno;
	}

	IBV_INIT_CMD_RESP_EXP(ARM_DCT, cmd, lib_cmd_sz, drv_cmd_sz, resp, lib_resp_sz, drv_resp_sz);

	if (write(context->cmd_fd, cmd, wsize) != wsize)
		return errno;

	return 0;
}

int ibv_exp_cmd_modify_cq(struct ibv_cq *cq,
			  struct ibv_exp_cq_attr *attr,
			  int attr_mask,
			  struct ibv_exp_modify_cq *cmd, size_t cmd_size)
{
	IBV_INIT_CMD_EXP(MODIFY_CQ, cmd, cmd_size, 0);

	if (attr->comp_mask >= IBV_EXP_CQ_ATTR_RESERVED)
		return ENOSYS;

	cmd->comp_mask = 0;
	cmd->cq_handle = cq->handle;
	cmd->attr_mask = attr_mask;
	cmd->cq_count  = attr->moderation.cq_count;
	cmd->cq_period = attr->moderation.cq_period;

	if (attr->cq_cap_flags & ~IBV_EXP_CQ_CAP_MASK)
		return EINVAL;
	else
		cmd->cq_cap_flags = attr->cq_cap_flags;

	if (write(cq->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;

	return 0;
}

int ibv_exp_cmd_modify_qp(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
			  uint64_t exp_attr_mask, struct ibv_exp_modify_qp *cmd,
			  size_t cmd_size)
{
	if (attr->comp_mask >= IBV_EXP_QP_ATTR_RESERVED)
		return ENOSYS;

	IBV_INIT_CMD_EXP(MODIFY_QP, cmd, cmd_size, 0);


	cmd->qp_handle		 = qp->handle;
	cmd->attr_mask		 = (__u32)exp_attr_mask;
	cmd->qkey		 = attr->qkey;
	cmd->rq_psn		 = attr->rq_psn;
	cmd->sq_psn		 = attr->sq_psn;
	cmd->dest_qp_num	 = attr->dest_qp_num;
	cmd->qp_access_flags	 = attr->qp_access_flags;
	cmd->pkey_index		 = attr->pkey_index;
	cmd->alt_pkey_index	 = attr->alt_pkey_index;
	cmd->qp_state		 = attr->qp_state;
	cmd->cur_qp_state	 = attr->cur_qp_state;
	cmd->path_mtu		 = attr->path_mtu;
	cmd->path_mig_state	 = attr->path_mig_state;
	cmd->en_sqd_async_notify = attr->en_sqd_async_notify;
	cmd->max_rd_atomic	 = attr->max_rd_atomic;
	cmd->max_dest_rd_atomic	 = attr->max_dest_rd_atomic;
	cmd->min_rnr_timer	 = attr->min_rnr_timer;
	cmd->port_num		 = attr->port_num;
	cmd->timeout		 = attr->timeout;
	cmd->retry_cnt		 = attr->retry_cnt;
	cmd->rnr_retry		 = attr->rnr_retry;
	cmd->alt_port_num	 = attr->alt_port_num;
	cmd->alt_timeout	 = attr->alt_timeout;

	memcpy(cmd->dest.dgid, attr->ah_attr.grh.dgid.raw, 16);
	cmd->dest.flow_label	= attr->ah_attr.grh.flow_label;
	cmd->dest.dlid		= attr->ah_attr.dlid;
	cmd->dest.reserved	= 0;
	cmd->dest.sgid_index	= attr->ah_attr.grh.sgid_index;
	cmd->dest.hop_limit	= attr->ah_attr.grh.hop_limit;
	cmd->dest.traffic_class	= attr->ah_attr.grh.traffic_class;
	cmd->dest.sl		= attr->ah_attr.sl;
	cmd->dest.src_path_bits	= attr->ah_attr.src_path_bits;
	cmd->dest.static_rate	= attr->ah_attr.static_rate;
	cmd->dest.is_global	= attr->ah_attr.is_global;
	cmd->dest.port_num	= attr->ah_attr.port_num;

	memcpy(cmd->alt_dest.dgid, attr->alt_ah_attr.grh.dgid.raw, 16);
	cmd->alt_dest.flow_label    = attr->alt_ah_attr.grh.flow_label;
	cmd->alt_dest.dlid	    = attr->alt_ah_attr.dlid;
	cmd->alt_dest.reserved	    = 0;
	cmd->alt_dest.sgid_index    = attr->alt_ah_attr.grh.sgid_index;
	cmd->alt_dest.hop_limit	    = attr->alt_ah_attr.grh.hop_limit;
	cmd->alt_dest.traffic_class = attr->alt_ah_attr.grh.traffic_class;
	cmd->alt_dest.sl	    = attr->alt_ah_attr.sl;
	cmd->alt_dest.src_path_bits = attr->alt_ah_attr.src_path_bits;
	cmd->alt_dest.static_rate   = attr->alt_ah_attr.static_rate;
	cmd->alt_dest.is_global	    = attr->alt_ah_attr.is_global;
	cmd->alt_dest.port_num	    = attr->alt_ah_attr.port_num;
	cmd->dct_key		    = attr->dct_key;
	cmd->exp_attr_mask	    = (__u32)(exp_attr_mask >> IBV_EXP_START_FLAG_LOC);
	if (attr->comp_mask & IBV_EXP_QP_ATTR_FLOW_ENTROPY)
		cmd->flow_entropy	 = attr->flow_entropy;
	cmd->rate_limit		    = attr->rate_limit;
	cmd->reserved[0]	    = 0;
	cmd->reserved[1]	    = 0;
	cmd->comp_mask		    = attr->comp_mask;

	if (write(qp->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;

	if (exp_attr_mask & IBV_EXP_QP_STATE)
		qp->state = attr->qp_state;

	return 0;
}

int ibv_exp_cmd_create_cq(struct ibv_context *context, int cqe,
			  struct ibv_comp_channel *channel,
			  int comp_vector, struct ibv_cq *cq,
			  struct ibv_exp_create_cq *cmd, size_t lib_cmd_sz, size_t drv_cmd_sz,
			  struct ibv_create_cq_resp *resp, size_t lib_resp_sz, size_t drv_resp_sz,
			  struct ibv_exp_cq_init_attr *attr)
{
	int wsize = lib_cmd_sz + drv_cmd_sz;

	IBV_INIT_CMD_RESP_EXP(CREATE_CQ, cmd, lib_cmd_sz, drv_cmd_sz, resp,
			      lib_resp_sz, drv_resp_sz);

	cmd->comp_mask     = 0;
	cmd->user_handle   = (uintptr_t) cq;
	cmd->cqe           = cqe;
	cmd->comp_vector   = comp_vector;
	cmd->comp_channel  = channel ? channel->fd : -1;
	cmd->reserved      = 0;

	if (attr->comp_mask > IBV_EXP_CQ_INIT_ATTR_RESERVED1)
		return ENOSYS;

	if (attr->comp_mask & IBV_EXP_CQ_INIT_ATTR_FLAGS) {
		if (attr->flags & ~IBV_EXP_CQ_CREATE_FLAGS_MASK)
			return ENOSYS;

		cmd->comp_mask |= IBV_EXP_CREATE_CQ_CAP_FLAGS;
		cmd->create_flags = attr->flags;
	}

	if (write(context->cmd_fd, cmd, wsize) != wsize)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, lib_resp_sz + drv_resp_sz);

	cq->handle  = resp->cq_handle;
	cq->cqe     = resp->cqe;
	cq->context = context;

	return 0;
}

int ibv_exp_cmd_create_mr(struct ibv_exp_create_mr_in *in,
			  struct ibv_mr *mr,
			  struct ibv_exp_create_mr *cmd,
			  size_t lib_cmd_sz,
			  size_t drv_cmd_sz,
			  struct ibv_exp_create_mr_resp *resp,
			  size_t lib_resp_sz,
			  size_t drv_resp_sz)
{
	struct ibv_pd *pd = in->pd;
	struct ibv_context *context = pd->context;
	struct ibv_exp_mr_init_attr *mr_init_attr = &in->attr;

	int wsize = lib_cmd_sz + drv_cmd_sz;

	IBV_INIT_CMD_RESP_EXP(CREATE_MR, cmd, lib_cmd_sz, drv_cmd_sz, resp,
			      lib_resp_sz, drv_resp_sz);

	cmd->pd_handle = pd->handle;
	cmd->max_klm_list_size = mr_init_attr->max_klm_list_size;
	cmd->create_flags = mr_init_attr->create_flags;
	cmd->exp_access_flags = mr_init_attr->exp_access_flags;
	cmd->comp_mask = in->comp_mask;

	if (write(context->cmd_fd, cmd, wsize) != wsize)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof(*resp));

	mr->handle  = resp->handle;
	mr->lkey    = resp->lkey;
	mr->rkey    = resp->rkey;
	mr->context = pd->context;

	return 0;
}

int ibv_exp_cmd_query_mkey(struct ibv_context *context,
			   struct ibv_mr *mr,
			   struct ibv_exp_mkey_attr *mkey_attr,
			   struct ibv_exp_query_mkey *cmd, size_t lib_cmd_sz,
			   size_t drv_cmd_sz,
			   struct ibv_exp_query_mkey_resp *resp,
			   size_t lib_resp_sz, size_t drv_resp_sz)
{
	int wsize = lib_cmd_sz + drv_cmd_sz;

	IBV_INIT_CMD_RESP_EXP(QUERY_MKEY, cmd, lib_cmd_sz, drv_cmd_sz, resp,
			      lib_resp_sz, drv_resp_sz);

	cmd->handle = mr->handle;
	cmd->lkey = mr->lkey;
	cmd->rkey = mr->rkey;
	cmd->comp_mask = mkey_attr->comp_mask;
	cmd->reserved = 0;

	if (write(context->cmd_fd, cmd, wsize) != wsize)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof(*resp));

	mkey_attr->max_klm_list_size = resp->max_klm_list_size;

	return 0;
}

int ibv_cmd_exp_reg_mr(
	const struct ibv_exp_reg_mr_in *mr_init_attr,
	uint64_t hca_va, struct ibv_mr *mr,
	struct ibv_exp_reg_mr *cmd,
	size_t cmd_size,
	struct ibv_exp_reg_mr_resp *resp,
	size_t resp_size)
{
	struct ibv_pd *pd = mr_init_attr->pd;

	if (mr_init_attr->comp_mask >= IBV_EXP_REG_MR_RESERVED)
		return EINVAL;

	IBV_INIT_CMD_RESP_EXP(REG_MR, cmd, cmd_size, 0, resp, resp_size, 0);

	cmd->comp_mask			= 0;
	cmd->start			= (uintptr_t) mr_init_attr->addr;
	cmd->length			= mr_init_attr->length;
	cmd->hca_va			= hca_va;
	cmd->pd_handle			= pd->handle;
	cmd->reserved			= 0;
	cmd->exp_access_flags		= mr_init_attr->exp_access;

	if (write(pd->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, sizeof(*resp));

	mr->handle  = resp->mr_handle;
	mr->lkey    = resp->lkey;
	mr->rkey    = resp->rkey;
	mr->context = pd->context;

	return 0;
}

int ibv_cmd_exp_prefetch_mr(struct ibv_mr *mr,
		struct ibv_exp_prefetch_attr *attr)
{
	struct ibv_exp_prefetch_mr cmd;

	IBV_INIT_CMD_EXP(PREFETCH_MR, &cmd, sizeof(cmd), 0);

	if (attr->comp_mask >= IBV_EXP_PREFETCH_MR_RESERVED)
		return EINVAL;

	if (attr->flags & ~IBV_EXP_PREFETCH_WRITE_ACCESS)
		return EINVAL;

	cmd.comp_mask = 0;
	cmd.mr_handle = mr->handle;
	cmd.flags     = attr->flags;
	cmd.start     = (uintptr_t) attr->addr;
	cmd.length    = attr->length;

	if (write(mr->context->cmd_fd, &cmd, sizeof(cmd)) != sizeof(cmd))
		return errno;

	return 0;
}

int ibv_exp_cmd_create_wq(struct ibv_context *context,
			  struct ibv_exp_wq_init_attr *wq_init_attr,
			  struct ibv_exp_wq *wq,
			  struct ibv_exp_create_wq *cmd,
			  size_t cmd_core_size,
			  size_t cmd_size,
			  struct ibv_exp_create_wq_resp *resp,
			  size_t resp_core_size,
			  size_t resp_size)
{
	int err;

	IBV_INIT_CMD_RESP_EX_V(cmd, cmd_core_size, cmd_size,
			       EXP_CREATE_WQ, resp,
			       resp_core_size, resp_size);

	cmd->user_handle = (uintptr_t)wq;
	cmd->pd_handle = wq_init_attr->pd->handle;
	cmd->cq_handle = wq_init_attr->cq->handle;
	cmd->wq_type = wq_init_attr->wq_type;
	cmd->max_recv_sge = wq_init_attr->max_recv_sge;
	cmd->max_recv_wr = wq_init_attr->max_recv_wr;
	cmd->comp_mask = 0;

	err = write(context->cmd_fd, cmd, cmd_size);
	if (err != cmd_size)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);

	if (resp->response_length < resp_core_size)
		return EINVAL;

	wq->handle  = resp->wq_handle;
	wq_init_attr->max_recv_wr = resp->max_recv_wr;
	wq_init_attr->max_recv_sge = resp->max_recv_sge;
	wq->wq_num = resp->wqn;
	wq->context = context;
	wq->cq = wq_init_attr->cq;
	wq->pd = wq_init_attr->pd;
	wq->srq = wq_init_attr->srq;
	wq->wq_type = wq_init_attr->wq_type;

	return 0;
}

int ibv_exp_cmd_modify_wq(struct ibv_exp_wq *wq, struct ibv_exp_wq_attr *attr,
			  struct ib_exp_modify_wq *cmd, size_t cmd_size)
{
	IBV_INIT_CMD_EX(cmd, cmd_size, EXP_MODIFY_WQ);

	cmd->curr_wq_state = attr->curr_wq_state;
	cmd->wq_state = attr->wq_state;
	cmd->wq_handle = wq->handle;
	/* Turn off IBV_EXP_CREATE_WQ_VLAN_OFFLOADS since it will be passed
	 * in the vendor data part.
	 */
	cmd->comp_mask = attr->attr_mask & (~IBV_EXP_CREATE_WQ_VLAN_OFFLOADS);

	if (write(wq->context->cmd_fd, cmd, cmd_size) != cmd_size)
		return errno;

	if (attr->attr_mask & IBV_EXP_WQ_ATTR_STATE)
		wq->state = attr->wq_state;

	return 0;
}

int ibv_exp_cmd_destroy_wq(struct ibv_exp_wq *wq)
{
	struct ib_exp_destroy_wq cmd;
	struct ibv_destroy_wq_resp resp;
	int ret = 0;

	memset(&cmd, 0, sizeof(cmd));
	memset(&resp, 0, sizeof(resp));
	IBV_INIT_CMD_RESP_EX(&cmd, sizeof(cmd), EXP_DESTROY_WQ, &resp, sizeof(resp));
	cmd.wq_handle = wq->handle;

	if (write(wq->context->cmd_fd, &cmd, sizeof(cmd)) != sizeof(cmd))
		ret = errno;

	return ret;
}

int ibv_exp_cmd_create_rwq_ind_table(struct ibv_context *context,
				     struct ibv_exp_rwq_ind_table_init_attr *init_attr,
				     struct ibv_exp_rwq_ind_table *rwq_ind_table,
				     struct ibv_exp_create_rwq_ind_table *cmd,
				     size_t cmd_core_size,
				     size_t cmd_size,
				     struct ibv_exp_create_rwq_ind_table_resp *resp,
				     size_t resp_core_size,
				     size_t resp_size)
{
	int err, i;
	uint32_t required_tbl_size, alloc_tbl_size;
	uint32_t *tbl_start;
	int num_tbl_entries;

	alloc_tbl_size = cmd_core_size - sizeof(*cmd);
	num_tbl_entries = 1 << init_attr->log_ind_tbl_size;

	/* Data must be u64 aligned */
	required_tbl_size = (num_tbl_entries * sizeof(uint32_t)) < sizeof(uint64_t) ?
			sizeof(uint64_t) : (num_tbl_entries * sizeof(uint32_t));

	if (alloc_tbl_size < required_tbl_size)
		return EINVAL;

	tbl_start = (uint32_t *)((uint8_t *)cmd + sizeof(*cmd));
	for (i = 0; i < num_tbl_entries; i++)
		tbl_start[i] = init_attr->ind_tbl[i]->handle;

	IBV_INIT_CMD_RESP_EX_V(cmd, cmd_core_size, cmd_size,
			       EXP_CREATE_RWQ_IND_TBL, resp,
			       resp_core_size, resp_size);

	cmd->log_ind_tbl_size = init_attr->log_ind_tbl_size;
	cmd->comp_mask = 0;

	err = write(context->cmd_fd, cmd, cmd_size);
	if (err != cmd_size)
		return errno;

	VALGRIND_MAKE_MEM_DEFINED(resp, resp_size);

	if (resp->response_length < resp_core_size)
		return EINVAL;

	rwq_ind_table->ind_tbl_handle = resp->ind_tbl_handle;
	rwq_ind_table->ind_tbl_num = resp->ind_tbl_num;
	rwq_ind_table->context = context;
	return 0;
}

int ibv_exp_cmd_destroy_rwq_ind_table(struct ibv_exp_rwq_ind_table *rwq_ind_table)
{
	struct ibv_exp_destroy_rwq_ind_table cmd;
	int ret = 0;

	memset(&cmd, 0, sizeof(cmd));
	IBV_INIT_CMD_EX(&cmd, sizeof(cmd), EXP_DESTROY_RWQ_IND_TBL);
	cmd.ind_tbl_handle = rwq_ind_table->ind_tbl_handle;

	if (write(rwq_ind_table->context->cmd_fd, &cmd, sizeof(cmd)) != sizeof(cmd))
		ret = errno;

	return ret;
}
