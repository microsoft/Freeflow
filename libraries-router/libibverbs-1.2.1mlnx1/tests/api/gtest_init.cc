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
#include "gtest/gtest.h"


/* ibv_get_device_list: [TI.1] Correct */
TEST(tc_ibv_get_device_list, ti_1) {

	struct ibv_device **dev_list;
	int num_devices = 0;

	errno = EOK;
	dev_list = ibv_get_device_list(&num_devices);
	EXPECT_TRUE(errno == EOK);
	EXPECT_TRUE(dev_list != NULL);
	EXPECT_TRUE(num_devices);

	ibv_free_device_list(dev_list);
}

/* ibv_get_device_name: [TI.1] Correct */
TEST(tc_ibv_get_device_name, ti_1) {

	struct ibv_device **dev_list;
	int num_devices = 0;
	int i = 0;

	errno = EOK;
	dev_list = ibv_get_device_list(&num_devices);
	EXPECT_TRUE(errno == EOK);
	EXPECT_TRUE(dev_list != NULL);
	EXPECT_TRUE(num_devices);

	for (i = 0; i < num_devices; ++i) {
		VERBS_INFO("    %-16s\t%016llx\n",
		       ibv_get_device_name(dev_list[i]),
		       (unsigned long long) ntohll(ibv_get_device_guid(dev_list[i])));
	}

	ibv_free_device_list(dev_list);
}

/* ibv_open_device: [TI.1] Correct */
TEST(tc_ibv_open_device, ti_1) {

	struct ibv_device **dev_list;
	struct ibv_device *ibv_dev;
	struct ibv_context*ibv_ctx;
	int num_devices = 0;

	errno = EOK;
	dev_list = ibv_get_device_list(&num_devices);
	ASSERT_TRUE(errno == EOK);
	ASSERT_TRUE(dev_list != NULL);
	ASSERT_TRUE(num_devices);

	ibv_dev = dev_list[0];

	ibv_ctx = ibv_open_device(ibv_dev);
	ASSERT_TRUE(ibv_ctx != NULL);

	ibv_close_device(ibv_ctx);
	ibv_free_device_list(dev_list);
}
