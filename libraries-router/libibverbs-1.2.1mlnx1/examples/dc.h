/*
 * Copyright (c) 2013 Mellanox Technologies.  All rights reserved.
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

#ifndef __DC_H
#define __DC_H

#include <stdio.h>
#include <infiniband/verbs.h>

struct pingpong_dest {
	int		lid;
	int		rsn;
	uint64_t	dckey;
};

/*                  DCTN   LID  DCT KEY              */
#define MSG_FORMAT "000000:0000:0000000000000000"

static inline int to_ib_mtu(int mtu, enum ibv_mtu *ibmtu)
{
	switch (mtu) {
	case 256:
		*ibmtu = IBV_MTU_256;
		return 0;
	case 512:
		*ibmtu = IBV_MTU_512;
		return 0;
	case 1024:
		*ibmtu = IBV_MTU_1024;
		return 0;
	case 2048:
		*ibmtu = IBV_MTU_2048;
		return 0;
	case 4096:
		*ibmtu = IBV_MTU_4096;
		return 0;
	default:
		return -1;
	}
}

#endif /* __DC_H */

