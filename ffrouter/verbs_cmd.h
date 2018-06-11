// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef VERBS_CMD_H
#define VERBS_CMD_H

#include "rdma_api.h"
#include "ibverbs.h"

int ibv_exp_cmd_query_device_resp(int cmd_fd, void* cmd_in, void* resp_out);
int ibv_cmd_query_qp_resp(int cmd_fd, void* cmd_in, int cmd_size, void* resp_out);
int ibv_cmd_create_flow_resp(int cmd_fd, void* cmd_in, int written_size, int exp_flow, void* resp_out);
int ibv_cmd_destroy_flow_resp(int cmd_fd, void* cmd_in);

#endif /* VERBS_CMD_H */


