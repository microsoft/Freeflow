#ifndef _GET_NEIGH_
#define _GET_NEIGH_

#include <stddef.h>
#include <stdint.h>
#include "config.h"
#ifdef HAVE_LIBNL1
#include <netlink/object.h>
#else
#include <netlink/object-api.h>
#endif

enum get_neigh_status {
	GET_NEIGH_STATUS_OK = 0,
	GET_NEIGH_STATUS_IN_PROCESS = 1 << 0,
	GET_NEIGH_STATUS_ERR = 1 << 1,
};

struct get_neigh_handler {
#ifdef HAVE_LIBNL1
	struct nl_handle *sock;
#else
	struct nl_sock *sock;
#endif
	struct nl_cache *link_cache;
	struct nl_cache	*neigh_cache;
	struct nl_cache *route_cache;
	int32_t oif;
	int vid;
	struct rtnl_neigh *filter_neigh;
	struct nl_addr *found_ll_addr;
	struct nl_addr *dst;
	struct nl_addr *src;
	enum get_neigh_status neigh_status;
	uint64_t timeout;
};

int process_get_neigh(struct get_neigh_handler *neigh_handler);
void neigh_free_resources(struct get_neigh_handler *neigh_handler);
void neigh_set_vlan_id(struct get_neigh_handler *neigh_handler, uint16_t vid);
uint16_t neigh_get_vlan_id_from_dev(struct get_neigh_handler *neigh_handler);
int neigh_init_resources(struct get_neigh_handler *neigh_handler, int timeout);

int neigh_set_src(struct get_neigh_handler *neigh_handler,
		  int family, void *buf, size_t size);
void neigh_set_oif(struct get_neigh_handler *neigh_handler, int oif);
int neigh_set_dst(struct get_neigh_handler *neigh_handler,
		  int family, void *buf, size_t size);
int neigh_get_oif_from_src(struct get_neigh_handler *neigh_handler);
int neigh_get_ll(struct get_neigh_handler *neigh_handler, void *addr_buf,
		 int addr_size);

#endif
