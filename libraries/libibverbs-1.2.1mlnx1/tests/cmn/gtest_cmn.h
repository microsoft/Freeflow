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

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>   /* printf PRItn */
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <inttypes.h>   /* printf PRItn */
#include <fcntl.h>
#include <poll.h>
#include <ctype.h>
#include <malloc.h>
#include <math.h>
#include <complex.h>

#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <infiniband/arch.h>

#include "gtest/gtest.h"


#define INLINE  __inline

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)P)
#endif

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))


/* Platform specific 16-byte alignment macro switch.
   On Visual C++ it would substitute __declspec(align(16)).
   On GCC it substitutes __attribute__((aligned (16))).
*/

#if defined(_MSC_VER)
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned (x)))
#endif

#pragma pack( push, 1 )
typedef struct _DATA128_TYPE
{
   uint64_t    field1;
   uint64_t    field2;
}  DATA128_TYPE;
#pragma pack( pop )


#if !defined( EOK )
#  define EOK 0         /* no error */
#endif

enum {
	GTEST_LOG_FATAL		= 1 << 0,
	GTEST_LOG_ERR		= 1 << 1,
	GTEST_LOG_WARN		= 1 << 2,
	GTEST_LOG_INFO		= 1 << 3,
	GTEST_LOG_TRACE		= 1 << 4
};

extern uint32_t gtest_debug_mask;
extern char *gtest_dev_name;


#define VERBS_INFO(fmt, ...)  \
	do {                                                           \
		if (gtest_debug_mask & GTEST_LOG_INFO)                 \
			printf("\033[0;3%sm" "[     INFO ] " fmt "\033[m", "4", ##__VA_ARGS__);    \
	} while(0)

#define VERBS_TRACE(fmt, ...) \
	do {                                                           \
		if (gtest_debug_mask & GTEST_LOG_TRACE)                 \
			printf("\033[0;3%sm" "[    TRACE ] " fmt "\033[m", "7", ##__VA_ARGS__);    \
	} while(0)


void sys_hexdump(void *ptr, int buflen);
uint32_t sys_inet_addr(char* ip);


static INLINE void sys_getenv(void)
{
	char *env;

	env = getenv("IBV_TEST_MASK");
	if (env)
		gtest_debug_mask = strtol(env, NULL, 0);

	env = getenv("IBV_TEST_DEV");
	gtest_dev_name = strdup(env ? env : "mlx");
}

static INLINE int sys_is_big_endian(void)
{
	return( htonl(1) == 1 );
}

static INLINE double sys_gettime(void)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (double)(tv.tv_sec * 1000000 + tv.tv_usec);
}

static INLINE uint64_t sys_rdtsc(void)
{
	unsigned long long int result=0;

#if defined(__i386__)
	__asm volatile(".byte 0x0f, 0x31" : "=A" (result) : );

#elif defined(__x86_64__)
	unsigned hi, lo;
	__asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	result = hi;
	result = result<<32;
	result = result|lo;

#elif defined(__powerpc__)
	unsigned long int hi, lo, tmp;
	__asm volatile(
	    "0:                 \n\t"
	    "mftbu   %0         \n\t"
	    "mftb    %1         \n\t"
	    "mftbu   %2         \n\t"
	    "cmpw    %2,%0      \n\t"
	    "bne     0b         \n"
	    : "=r"(hi),"=r"(lo),"=r"(tmp)
	    );
	result = hi;
	result = result<<32;
	result = result|lo;

#endif

	return (result);
}

/**
 * Base class for ibverbs test fixtures.
 * Initialize and close ibverbs.
 */
class verbs_test : public testing::Test {
protected:
	virtual void SetUp() {
		int num_devices = 0;
		int i = 0;
		int rc = 0;

		errno = EOK;
		/*
		 * First you must retrieve the list of available IB devices on the local host.
		 * Every device in this list contains both a name and a GUID.
		 * For example the device names can be: mthca0, mlx4_1.
		 */
		dev_list = ibv_get_device_list(&num_devices);
		/*  Disable untill Bullseye will fixe errno problem.
		ASSERT_TRUE(errno == EOK);
		*/
		ASSERT_TRUE(dev_list != NULL);
		ASSERT_TRUE(num_devices);

		/*
		 * Iterate over the device list, choose a device according to its GUID or name
		 * and open it.
		 */
		for (i = 0; i < num_devices; ++i) {
			if (!strncmp(ibv_get_device_name(dev_list[i]),
				     gtest_dev_name, strlen(gtest_dev_name))) {
					ibv_dev = dev_list[i];
					break;
			}
		}
		ASSERT_TRUE(ibv_dev != NULL);

		ibv_ctx = ibv_open_device(ibv_dev);
		ASSERT_TRUE(ibv_ctx != NULL);

		/*
		 * The device capabilities allow the user to understand the supported features
		 * (APM, SRQ) and capabilities of the opened device.
		 */
		device_attr.comp_mask = IBV_EXP_DEVICE_ATTR_RESERVED-1;
		rc = ibv_exp_query_device(ibv_ctx, &device_attr);
		ASSERT_TRUE(!rc);
	}

	virtual void TearDown() {

		/*
		 * Destroy objects in the reverse order you created them:
		 * Delete QP
		 * Delete CQ
		 * Deregister MR
		 * Deallocate PD
		 * Close device
		 */
		ibv_close_device(ibv_ctx);
		ibv_free_device_list(dev_list);
	}

protected:
	struct ibv_device **dev_list;
	struct ibv_device *ibv_dev;
	struct ibv_context*ibv_ctx;
	struct ibv_exp_device_attr device_attr;
};
