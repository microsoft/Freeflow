
#include "config.h"
#include <net/if_packet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/link.h>
#include <netlink/route/route.h>
#include <netlink/route/neighbour.h>
#ifndef HAVE_LIBNL1
#include <netlink/route/link/vlan.h>
#endif


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netdb.h>
#ifndef _LINUX_IF_H
#include <net/if.h>
#else
/*Workaround when there's a collision between the includes */
extern unsigned int if_nametoindex(__const char *__ifname) __THROW;
#endif

/* for PFX */
#include "ibverbs.h"

#include "neigh.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define print_hdr PFX "resolver: "
#define print_err(...) fprintf(stderr, print_hdr __VA_ARGS__)
#ifdef _DEBUG_
#define print_dbg_s(stream, ...) fprintf((stream), __VA_ARGS__)
#define print_address(stream, msg, addr) \
	{							      \
		char buff[100];					      \
		print_dbg_s((stream), (msg), nl_addr2str((addr), buff,\
			    sizeof(buff)));			      \
	}
#else
#define print_dbg_s(stream, args...)
#define print_address(stream, msg, ...)
#endif
#define print_dbg(...) print_dbg_s(stderr, print_hdr __VA_ARGS__)

#ifdef HAVE_LIBNL1
#define nl_geterror(x) nl_geterror()
#endif

/* Workaround - declaration missing */
extern int		rtnl_link_vlan_get_id(struct rtnl_link *);

static pthread_once_t device_neigh_alloc = PTHREAD_ONCE_INIT;
#ifdef HAVE_LIBNL1
static struct nl_handle *zero_socket;
#else
static struct nl_sock *zero_socket;
#endif

union sktaddr {
	struct sockaddr s;
	struct sockaddr_in s4;
	struct sockaddr_in6 s6;
};

struct skt {
	union sktaddr sktaddr;
	socklen_t len;
};


struct rtnl_neigh *mrtnl_neigh_get(struct nl_cache *cache, int ifindex,
				   struct nl_addr *dst)
{
	struct rtnl_neigh *neigh;

	neigh = (struct rtnl_neigh *)nl_cache_get_first(cache);
	while (neigh) {
		if (rtnl_neigh_get_ifindex(neigh) == ifindex &&
		    !nl_addr_cmp(rtnl_neigh_get_dst(neigh), dst)) {
			nl_object_get((struct nl_object *)neigh);
			return neigh;
		}

		neigh = (struct rtnl_neigh *)nl_cache_get_next((struct nl_object *)neigh);
	}

	return NULL;
}

static int set_link_port(union sktaddr *s, int port, int oif)
{
	switch (s->s.sa_family) {
	case AF_INET:
		s->s4.sin_port = port;
		break;
	case AF_INET6:
		s->s6.sin6_port = port;
		s->s6.sin6_scope_id = oif;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cmp_address(const struct sockaddr *s1,
		       const struct sockaddr *s2) {
	if (s1->sa_family != s2->sa_family)
		return s1->sa_family ^ s2->sa_family;

	switch (s1->sa_family) {
	case AF_INET:
		return ((struct sockaddr_in *)s1)->sin_addr.s_addr ^
		       ((struct sockaddr_in *)s2)->sin_addr.s_addr;
	case AF_INET6:
		return memcmp(
			((struct sockaddr_in6 *)s1)->sin6_addr.s6_addr,
			((struct sockaddr_in6 *)s2)->sin6_addr.s6_addr,
			sizeof(((struct sockaddr_in6 *)s1)->sin6_addr.s6_addr));
	default:
		return -ENOTSUP;
	}
}


static int get_ifindex(const struct sockaddr *s)
{
	struct ifaddrs *ifaddr, *ifa;
	int name2index = -ENODEV;

	if (-1 == getifaddrs(&ifaddr))
		return errno;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (NULL == ifa->ifa_addr)
			continue;

		if (!cmp_address(ifa->ifa_addr, s)) {
			name2index = if_nametoindex(ifa->ifa_name);
			break;
		}
	}

	freeifaddrs(ifaddr);

	return name2index;
}

static struct nl_addr *get_neigh_mac(struct get_neigh_handler *neigh_handler)
{
	struct rtnl_neigh *neigh;
	struct nl_addr *ll_addr = NULL;

	/* future optimization - if link local address - parse address and
	 * return mac */
#ifdef HAVE_LIBNL3_BUG
	neigh = mrtnl_neigh_get(neigh_handler->neigh_cache,
				neigh_handler->oif,
				neigh_handler->dst);
#else
	neigh = rtnl_neigh_get(neigh_handler->neigh_cache,
			       neigh_handler->oif,
			       neigh_handler->dst);
#endif

	if (NULL == neigh) {
		print_dbg("Neigh isn't at cache\n");
		return NULL;
	}

	ll_addr = rtnl_neigh_get_lladdr(neigh);
	if (NULL == ll_addr)
		print_dbg("Failed to get hw address from neighbour\n");
	else
		ll_addr = nl_addr_clone(ll_addr);

	rtnl_neigh_put(neigh);
	return ll_addr;
}

static void get_neigh_cb_event(struct nl_object *obj, void *arg)
{
	struct get_neigh_handler *neigh_handler =
		(struct get_neigh_handler *)arg;
	/* assumed serilized callback (no parallel execution of function) */
	if (nl_object_match_filter(
		obj,
		(struct nl_object *)neigh_handler->filter_neigh)) {
		struct rtnl_neigh *neigh = (struct rtnl_neigh *)obj;
		print_dbg("Found a match for neighbour\n");
		/* check that we didn't set it already */
		if (NULL == neigh_handler->found_ll_addr) {
			if (NULL == rtnl_neigh_get_lladdr(neigh)) {
				print_err("Neighbour doesn't have a hw addr\n");
				return;
			}
			neigh_handler->found_ll_addr =
				nl_addr_clone(rtnl_neigh_get_lladdr(neigh));
			if (NULL == neigh_handler->found_ll_addr)
				print_err("Couldn't copy neighbour hw addr\n");
		}
	}
}

static int get_neigh_cb(struct nl_msg *msg, void *arg)
{
	struct get_neigh_handler *neigh_handler =
		(struct get_neigh_handler *)arg;

	if (nl_msg_parse(msg, &get_neigh_cb_event, neigh_handler) < 0)
		print_err("Unknown event\n");

	return NL_OK;
}

static void set_neigh_filter(struct get_neigh_handler *neigh_handler,
			     struct rtnl_neigh *filter) {
	neigh_handler->filter_neigh = filter;
}

static struct rtnl_neigh *create_filter_neigh_for_dst(struct nl_addr *dst_addr,
						      int oif)
{
	struct rtnl_neigh *filter_neigh;

	filter_neigh = rtnl_neigh_alloc();
	if (NULL == filter_neigh) {
		print_err("Couldn't allocate filter neigh\n");
		return NULL;
	}
	rtnl_neigh_set_ifindex(filter_neigh, oif);
	rtnl_neigh_set_dst(filter_neigh, dst_addr);

	return filter_neigh;
}

#define PORT_DISCARD htons(9)
#define SEND_PAYLOAD "H"

static int create_socket(struct get_neigh_handler *neigh_handler,
			 struct skt *addr_dst, int *psock_fd)
{
	int err;
	struct skt addr_src;
	int sock_fd;

	memset(addr_dst, 0, sizeof(*addr_dst));
	memset(&addr_src, 0, sizeof(addr_src));
	addr_src.len = sizeof(addr_src.sktaddr);

	err = nl_addr_fill_sockaddr(neigh_handler->src,
				    &addr_src.sktaddr.s,
				    &addr_src.len);
	if (err) {
		print_err("couldn't convert src to sockaddr\n");
		return err;
	}

	addr_dst->len = sizeof(addr_dst->sktaddr);
	err = nl_addr_fill_sockaddr(neigh_handler->dst,
				    &addr_dst->sktaddr.s,
				    &addr_dst->len);
	if (err) {
		print_err("couldn't convert dst to sockaddr\n");
		return err;
	}

	err = set_link_port(&addr_dst->sktaddr, PORT_DISCARD,
			    neigh_handler->oif);
	if (err)
		return err;

	sock_fd = socket(addr_dst->sktaddr.s.sa_family,
			SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sock_fd == -1)
		return errno ? -errno : -1;
	err = bind(sock_fd, &addr_src.sktaddr.s, addr_src.len);
	if (err) {
		int bind_err = -errno;
		print_err("Couldn't bind socket\n");
		close(sock_fd);
		return bind_err ?: err;
	}

	*psock_fd = sock_fd;

	return 0;
}

#define NUM_OF_RETRIES 10
#define NUM_OF_TRIES ((NUM_OF_RETRIES) + 1)
#if NUM_OF_TRIES < 1
#error "neigh: invalid value of NUM_OF_RETRIES"
#endif
static int create_timer(struct get_neigh_handler *neigh_handler)
{
	int user_timeout = neigh_handler->timeout/NUM_OF_TRIES;
	struct timespec timeout = {
		.tv_sec = user_timeout / 1000,
		.tv_nsec = (user_timeout % 1000) * 1000000
	};
	struct itimerspec timer_time = {.it_value = timeout};
	int timer_fd;

	timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (-1 == timer_fd) {
		print_err("Couldn't create timer\n");
		return timer_fd;
	}

	if (neigh_handler->timeout) {
		if (NUM_OF_TRIES <= 1)
			bzero(&timer_time.it_interval,
			      sizeof(timer_time.it_interval));
		else
			timer_time.it_interval = timeout;
		if (timerfd_settime(timer_fd, 0, &timer_time, NULL)) {
			print_err("Couldn't set timer\n");
			return -1;
		}
	}

	return timer_fd;
}

#define UDP_SOCKET_MAX_SENDTO 100000ULL

static struct nl_addr *process_get_neigh_mac(
		struct get_neigh_handler *neigh_handler)
{
	int err;
	struct nl_addr *ll_addr = get_neigh_mac(neigh_handler);
	struct rtnl_neigh *neigh_filter;
	fd_set fdset;
	int uninitialized_var(sock_fd);
	int fd;
	int nfds;
	int timer_fd;
	int ret;
	struct skt addr_dst;
	char buff[sizeof(SEND_PAYLOAD)] = SEND_PAYLOAD;
	int retries = 0;
	uint64_t max_count = UDP_SOCKET_MAX_SENDTO;

	if (NULL != ll_addr)
		return ll_addr;

	err = nl_socket_add_membership(neigh_handler->sock,
				       RTNLGRP_NEIGH);
	if (err < 0) {
		print_err("%s\n", nl_geterror(err));
		return NULL;
	}

	neigh_filter = create_filter_neigh_for_dst(neigh_handler->dst,
						   neigh_handler->oif);
	if (NULL == neigh_filter)
		return NULL;

	set_neigh_filter(neigh_handler, neigh_filter);

#ifdef HAVE_LIBNL1
	nl_disable_sequence_check(neigh_handler->sock);
#else
	nl_socket_disable_seq_check(neigh_handler->sock);
#endif
	nl_socket_modify_cb(neigh_handler->sock, NL_CB_VALID, NL_CB_CUSTOM,
			    &get_neigh_cb, neigh_handler);

	fd = nl_socket_get_fd(neigh_handler->sock);

	err = create_socket(neigh_handler, &addr_dst, &sock_fd);

	if (err)
		return NULL;

	do {
		err = sendto(sock_fd, buff, sizeof(buff), 0,
				 &addr_dst.sktaddr.s,
				 addr_dst.len);
		if (err > 0)
			err = 0;
	} while (-1 == err && EADDRNOTAVAIL == errno && --max_count);

	if (err) {
		print_err("Failed to send packet %d", err);
		goto close_socket;
	}
	timer_fd = create_timer(neigh_handler);
	if (timer_fd < 0)
		goto close_socket;

	nfds = MAX(fd, timer_fd) + 1;


	while (1) {
		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);
		FD_SET(timer_fd, &fdset);

		/* wait for an incoming message on the netlink socket */
		ret = select(nfds, &fdset, NULL, NULL, NULL);

		if (ret) {
			if (FD_ISSET(fd, &fdset)) {
				nl_recvmsgs_default(neigh_handler->sock);
				if (neigh_handler->found_ll_addr)
					break;
			} else {
				nl_cache_refill(neigh_handler->sock,
						neigh_handler->neigh_cache);
				ll_addr = get_neigh_mac(neigh_handler);
				if (NULL != ll_addr) {
					break;
				} else if (FD_ISSET(timer_fd, &fdset) &&
					   retries < NUM_OF_RETRIES) {
					if (sendto(sock_fd, buff, sizeof(buff),
						   0, &addr_dst.sktaddr.s,
						   addr_dst.len) < 0)
						print_err("Failed to send "
							  "packet while waiting"
							  " for events\n");
				}
			}

			if (FD_ISSET(timer_fd, &fdset)) {
				uint64_t read_val;
				if (read(timer_fd, &read_val, sizeof(read_val))
				    < 0)
					print_dbg("Read timer_fd failed\n");
				if (++retries >=  NUM_OF_TRIES) {
					print_dbg("Timeout while trying to fetch "
						  "neighbours\n");
					break;
				}
			}
		}
	}
	close(timer_fd);
close_socket:
	close(sock_fd);
	return ll_addr ? ll_addr : neigh_handler->found_ll_addr;
}


static int get_mcast_mac_ipv4(struct nl_addr *dst, struct nl_addr **ll_addr)
{
	char mac_addr[6] = {0x01, 0x00, 0x5E};
	uint32_t addr = ntohl(*(uint32_t *)nl_addr_get_binary_addr(dst));
	mac_addr[5] = addr & 0xFF;
	addr >>= 8;
	mac_addr[4] = addr & 0xFF;
	addr >>= 8;
	mac_addr[3] = addr & 0x7F;

	*ll_addr = nl_addr_build(AF_LLC, mac_addr, sizeof(mac_addr));

	return *ll_addr == NULL ? -EINVAL : 0;
}

static int get_mcast_mac_ipv6(struct nl_addr *dst, struct nl_addr **ll_addr)
{
	char mac_addr[6] = {0x33, 0x33};
	memcpy(mac_addr + 2, (char *)nl_addr_get_binary_addr(dst) + 12, 4);

	*ll_addr = nl_addr_build(AF_LLC, mac_addr, sizeof(mac_addr));

	return *ll_addr == NULL ? -EINVAL : 0;
}

static int get_link_local_mac_ipv6(struct nl_addr *dst,
				   struct nl_addr **ll_addr)
{
	char mac_addr[6];
	int dev_id_zero =
		(uint16_t)(
		 (*((uint8_t *)(nl_addr_get_binary_addr(dst) + 11)) << 8) +
		 *(uint8_t *)(nl_addr_get_binary_addr(dst) + 12)) == 0xfffe;

	memcpy(mac_addr + 3, (char *)nl_addr_get_binary_addr(dst) + 13, 3);
	memcpy(mac_addr, (char *)nl_addr_get_binary_addr(dst) + 8, 3);
	if (dev_id_zero)
		mac_addr[0] ^= 2;

	*ll_addr = nl_addr_build(AF_LLC, mac_addr, sizeof(mac_addr));
	return *ll_addr == NULL ? -EINVAL : 0;
}

const struct encoded_l3_addr {
	short family;
	uint8_t prefix_bits;
	const char data[16];
	int (*getter)(struct nl_addr *dst, struct nl_addr **ll_addr);
} encoded_prefixes[] = {
	{.family = AF_INET,
	 .prefix_bits = 4,
	 .data = {0xe0},
	 .getter = &get_mcast_mac_ipv4},
	{.family = AF_INET6,
	 .prefix_bits = 8,
	 .data = {0xff},
	 .getter = &get_mcast_mac_ipv6},
	{.family = AF_INET6,
	 .prefix_bits = 64,
	 .data = {0xfe, 0x80},
	 .getter = get_link_local_mac_ipv6},
};

int nl_addr_cmp_prefix_msb(void *addr1, int len1, void *addr2, int len2)
{
	int len = MIN(len1, len2);
	int bytes = len / 8;
	int d = memcmp(addr1, addr2, bytes);

	if (d == 0) {
		int mask = ((1UL << (len % 8)) - 1UL) << (8 - len);

		d = (((char *)addr1)[bytes] & mask) -
		    (((char *)addr2)[bytes] & mask);
	}

	return d;
}
static int handle_encoded_mac(struct nl_addr *dst, struct nl_addr **ll_addr)
{
	uint32_t family = nl_addr_get_family(dst);
	struct nl_addr *prefix = NULL;
	int i;
	int ret = 1;

	for (i = 0;
	     i < sizeof(encoded_prefixes)/sizeof(encoded_prefixes[0]) &&
	     ret; prefix = NULL, i++) {
		if (encoded_prefixes[i].family != family)
			continue;

		prefix = nl_addr_build(
				family,
				(void *)encoded_prefixes[i].data,
				MIN(encoded_prefixes[i].prefix_bits/8 +
				    !!(encoded_prefixes[i].prefix_bits % 8),
				    sizeof(encoded_prefixes[i].data)));

		if (NULL == prefix)
			return -ENOMEM;
		nl_addr_set_prefixlen(prefix,
				      encoded_prefixes[i].prefix_bits);

		if (!nl_addr_cmp_prefix_msb(nl_addr_get_binary_addr(dst),
					    nl_addr_get_prefixlen(dst),
					    nl_addr_get_binary_addr(prefix),
					    nl_addr_get_prefixlen(prefix)))
			ret = encoded_prefixes[i].getter(dst, ll_addr);
#ifdef HAVE_LIBNL1
		nl_addr_destroy(prefix);
#else
		nl_addr_put(prefix);
#endif
	}

	return ret;
}

static void get_route_cb_parser(struct nl_object *obj, void *arg)
{
	struct get_neigh_handler *neigh_handler =
		(struct get_neigh_handler *)arg;

	struct rtnl_route *route = (struct rtnl_route *)obj;
	struct nl_addr *gateway;
	struct nl_addr *src = rtnl_route_get_pref_src(route);
	int oif;
	int type = rtnl_route_get_type(route);
	struct rtnl_link *link;

#ifdef HAVE_LIBNL1
	gateway = rtnl_route_get_gateway(route);
	oif = rtnl_route_get_oif(route);
#else
	struct rtnl_nexthop *nh = rtnl_route_nexthop_n(route, 0);
	if (!nh)
		print_err("Out of memory\n");
	gateway = rtnl_route_nh_get_gateway(nh);
	oif = rtnl_route_nh_get_ifindex(nh);
#endif

	if (gateway) {
#ifdef HAVE_LIBNL1
		nl_addr_destroy(neigh_handler->dst);
#else
		nl_addr_put(neigh_handler->dst);
#endif
		neigh_handler->dst = nl_addr_clone(gateway);
		print_dbg("Found gateway\n");
	}

	if (RTN_BLACKHOLE == type ||
	    RTN_UNREACHABLE == type ||
	    RTN_PROHIBIT == type ||
	    RTN_THROW == type) {
		print_err("Destination unrechable (type %d)\n", type);
		goto err;
	}

	if (!neigh_handler->src && src)
		neigh_handler->src = nl_addr_clone(src);

	if (neigh_handler->oif < 0 && oif > 0)
		neigh_handler->oif = oif;

	/* Link Local */
	if (RTN_LOCAL == type) {
		struct nl_addr *lladdr;

		link = rtnl_link_get(neigh_handler->link_cache,
				     neigh_handler->oif);

		if (NULL == link)
			goto err;

		lladdr = rtnl_link_get_addr(link);

		if (NULL == lladdr)
			goto err_link;

		neigh_handler->found_ll_addr = nl_addr_clone(lladdr);
		rtnl_link_put(link);
	} else {
		if (!handle_encoded_mac(
				neigh_handler->dst,
				&neigh_handler->found_ll_addr))
			print_address(stderr, "calculated address %s\n",
				      neigh_handler->found_ll_addr);
	}

	print_address(stderr, "Current dst %s\n", neigh_handler->dst);
	return;

err_link:
	rtnl_link_put(link);
err:
	if (neigh_handler->src) {
#ifdef HAVE_LIBNL1
		nl_addr_put(neigh_handler->src);
#else
		nl_addr_put(neigh_handler->src);
#endif
		neigh_handler->src = NULL;
	}
}

static int get_route_cb(struct nl_msg *msg, void *arg)
{
	struct get_neigh_handler *neigh_handler =
		(struct get_neigh_handler *)arg;
	int err;

	err = nl_msg_parse(msg, &get_route_cb_parser, neigh_handler);
	if (err < 0) {
		print_err("Unable to parse object: %s", nl_geterror(err));
		return err;
	}

	if (!neigh_handler->dst || !neigh_handler->src ||
	    neigh_handler->oif <= 0) {
		print_err("Missing params\n");
		return -1;
	}

	if (NULL != neigh_handler->found_ll_addr)
		goto found;

	neigh_handler->found_ll_addr =
		process_get_neigh_mac(neigh_handler);
	if (neigh_handler->found_ll_addr)
		print_address(stderr, "Neigh is %s\n",
			      neigh_handler->found_ll_addr);

found:
	return neigh_handler->found_ll_addr ? 0 : -1;
}

int neigh_get_oif_from_src(struct get_neigh_handler *neigh_handler)
{
	int oif = -ENODEV;
	struct addrinfo *src_info;

#ifdef HAVE_LIBNL1
	src_info = nl_addr_info(neigh_handler->src);
	if (NULL == src_info) {
#else
	int err;
	err = nl_addr_info(neigh_handler->src, &src_info);
	if (err) {
#endif
		print_err("Unable to get address info %s\n",
			  nl_geterror(err));
		return oif;
	}

	oif = get_ifindex(src_info->ai_addr);
	if (oif <= 0)
		goto free;

	print_dbg("IF index is %d\n", oif);

free:
	freeaddrinfo(src_info);
	return oif;
}

static void destroy_zero_based_socket(void)
{
	if (NULL != zero_socket) {
		print_dbg("destroying zero based socket\n");
#ifdef HAVE_LIBNL1
		nl_handle_destroy(zero_socket);
#else
		nl_socket_free(zero_socket);
#endif
	}
}

static void alloc_zero_based_socket(void)
{
#ifdef HAVE_LIBNL1
	zero_socket = nl_handle_alloc();
#else
	zero_socket = nl_socket_alloc();
#endif
	print_dbg("creating zero based socket\n");
	atexit(&destroy_zero_based_socket);
}

int neigh_init_resources(struct get_neigh_handler *neigh_handler, int timeout)
{
	int err;

	pthread_once(&device_neigh_alloc, &alloc_zero_based_socket);
#ifdef HAVE_LIBNL1
	neigh_handler->sock = nl_handle_alloc();
#else
	neigh_handler->sock = nl_socket_alloc();
#endif
	if (NULL == neigh_handler->sock) {
		print_err("Unable to allocate netlink socket\n");
		return -ENOMEM;
	}

	err = nl_connect(neigh_handler->sock, NETLINK_ROUTE);
	if (err < 0) {
		print_err("Unable to connect netlink socket: %s\n",
			  nl_geterror(err));
		goto free_socket;
	}

#ifdef HAVE_LIBNL1
	neigh_handler->link_cache =
		rtnl_link_alloc_cache(neigh_handler->sock);
	if (NULL == neigh_handler->link_cache) {
		err = -ENOMEM;
#else

	err = rtnl_link_alloc_cache(neigh_handler->sock, AF_UNSPEC,
				    &neigh_handler->link_cache);
	if (err) {
#endif
		print_err("Unable to allocate link cache: %s\n",
			  nl_geterror(err));
		err = -ENOMEM;
		goto close_connection;
	}

	nl_cache_mngt_provide(neigh_handler->link_cache);

#ifdef HAVE_LIBNL1
	neigh_handler->route_cache =
		rtnl_route_alloc_cache(neigh_handler->sock);
	if (NULL == neigh_handler->route_cache) {
		err = -ENOMEM;
#else
	err = rtnl_route_alloc_cache(neigh_handler->sock, AF_UNSPEC, 0,
				     &neigh_handler->route_cache);
	if (err) {
#endif
		print_err("Unable to allocate route cache: %s\n",
			  nl_geterror(err));
		goto free_link_cache;
	}

	nl_cache_mngt_provide(neigh_handler->route_cache);

#ifdef HAVE_LIBNL1
	neigh_handler->neigh_cache =
		rtnl_neigh_alloc_cache(neigh_handler->sock);
	if (NULL == neigh_handler->neigh_cache) {
		err = -ENOMEM;
#else
	err = rtnl_neigh_alloc_cache(neigh_handler->sock,
				     &neigh_handler->neigh_cache);
	if (err) {
#endif
		print_err("Couldn't allocate neigh cache %s\n",
			  nl_geterror(err));
		goto free_route_cache;
	}

	nl_cache_mngt_provide(neigh_handler->neigh_cache);

	/* init structure */
	neigh_handler->timeout = timeout;
	neigh_handler->oif = -1;
	neigh_handler->filter_neigh = NULL;
	neigh_handler->found_ll_addr = NULL;
	neigh_handler->dst = NULL;
	neigh_handler->src = NULL;
	neigh_handler->vid = -1;
	neigh_handler->neigh_status = GET_NEIGH_STATUS_OK;

	return 0;

free_route_cache:
	nl_cache_mngt_unprovide(neigh_handler->route_cache);
	nl_cache_free(neigh_handler->route_cache);
	neigh_handler->route_cache = NULL;
free_link_cache:
	nl_cache_mngt_unprovide(neigh_handler->link_cache);
	nl_cache_free(neigh_handler->link_cache);
	neigh_handler->link_cache = NULL;
close_connection:
	nl_close(neigh_handler->sock);
free_socket:
#ifdef HAVE_LIBNL1
	nl_handle_destroy(neigh_handler->sock);
		nl_handle_destroy(zero_socket);
#else
	nl_socket_free(neigh_handler->sock);
#endif
	neigh_handler->sock = NULL;
	return err;
}

uint16_t neigh_get_vlan_id_from_dev(struct get_neigh_handler *neigh_handler)
{
	struct rtnl_link *link;
	int vid = 0xffff;

	link = rtnl_link_get(neigh_handler->link_cache, neigh_handler->oif);
	if (NULL == link) {
		print_err("Can't find link in cache\n");
		return -EINVAL;
	}

#ifndef HAVE_LIBNL1
	if (rtnl_link_is_vlan(link))
#endif
		vid = rtnl_link_vlan_get_id(link);
	rtnl_link_put(link);
	return vid >= 0 && vid <= 0xfff ? vid : 0xffff;
}

void neigh_set_vlan_id(struct get_neigh_handler *neigh_handler, uint16_t vid)
{
	if (vid <= 0xfff)
		neigh_handler->vid = vid;
}

int neigh_set_dst(struct get_neigh_handler *neigh_handler,
		  int family, void *buf, size_t size)
{
	neigh_handler->dst = nl_addr_build(family, buf, size);
	return NULL == neigh_handler->dst;
}

int neigh_set_src(struct get_neigh_handler *neigh_handler,
		  int family, void *buf, size_t size)
{
	neigh_handler->src = nl_addr_build(family, buf, size);
	return NULL == neigh_handler->src;
}

void neigh_set_oif(struct get_neigh_handler *neigh_handler, int oif)
{
	neigh_handler->oif = oif;
}

int neigh_get_ll(struct get_neigh_handler *neigh_handler, void *addr_buff,
		 int addr_size) {
	int neigh_len;

	if (NULL == neigh_handler->found_ll_addr)
		return -EINVAL;

	 neigh_len = nl_addr_get_len(neigh_handler->found_ll_addr);

	if (neigh_len > addr_size)
		return -EINVAL;

	memcpy(addr_buff, nl_addr_get_binary_addr(neigh_handler->found_ll_addr),
	       neigh_len);

	return neigh_len;
}

void neigh_free_resources(struct get_neigh_handler *neigh_handler)
{
	/* Should be released first because it's holding a reference to dst */
	if (NULL != neigh_handler->filter_neigh) {
		rtnl_neigh_put(neigh_handler->filter_neigh);
		neigh_handler->filter_neigh = NULL;
	}

	if (NULL != neigh_handler->src) {
#ifdef HAVE_LIBNL1
		nl_addr_put(neigh_handler->src);
#else
		nl_addr_put(neigh_handler->src);
#endif
		neigh_handler->src = NULL;
	}

	if (NULL != neigh_handler->dst) {
#ifdef HAVE_LIBNL1
		nl_addr_destroy(neigh_handler->dst);
#else
		nl_addr_put(neigh_handler->dst);
#endif
		neigh_handler->dst = NULL;
	}

	if (NULL != neigh_handler->found_ll_addr) {
#ifdef HAVE_LIBNL1
		nl_addr_destroy(neigh_handler->found_ll_addr);
#else
		nl_addr_put(neigh_handler->found_ll_addr);
#endif
		neigh_handler->found_ll_addr = NULL;
	}

	if (NULL != neigh_handler->neigh_cache) {
		nl_cache_mngt_unprovide(neigh_handler->neigh_cache);
		nl_cache_free(neigh_handler->neigh_cache);
		neigh_handler->neigh_cache = NULL;
	}

	if (NULL != neigh_handler->route_cache) {
		nl_cache_mngt_unprovide(neigh_handler->route_cache);
		nl_cache_free(neigh_handler->route_cache);
		neigh_handler->route_cache = NULL;
	}

	if (NULL != neigh_handler->link_cache) {
		nl_cache_mngt_unprovide(neigh_handler->link_cache);
		nl_cache_free(neigh_handler->link_cache);
		neigh_handler->link_cache = NULL;
	}

	if (NULL != neigh_handler->sock) {
		nl_close(neigh_handler->sock);
#ifdef HAVE_LIBNL1
		nl_handle_destroy(neigh_handler->sock);
#else
		nl_socket_free(neigh_handler->sock);
#endif
		neigh_handler->sock = NULL;
	}
}

int process_get_neigh(struct get_neigh_handler *neigh_handler)
{
	struct nl_msg *m;
	struct rtmsg rmsg = {
		.rtm_family = nl_addr_get_family(neigh_handler->dst),
		.rtm_dst_len = nl_addr_get_prefixlen(neigh_handler->dst),
	};
	int err;
	int last_status;

	last_status = __sync_fetch_and_or(
			&neigh_handler->neigh_status,
			GET_NEIGH_STATUS_IN_PROCESS);

	if (last_status != GET_NEIGH_STATUS_OK)
		return -EINPROGRESS;

	m = nlmsg_alloc_simple(RTM_GETROUTE, 0);

	if (NULL == m)
		return -ENOMEM;

	nlmsg_append(m, &rmsg, sizeof(rmsg), NLMSG_ALIGNTO);

	nla_put_addr(m, RTA_DST, neigh_handler->dst);

	if (neigh_handler->oif > 0)
		nla_put_u32(m, RTA_OIF, neigh_handler->oif);

	err = nl_send_auto_complete(neigh_handler->sock, m);
	nlmsg_free(m);
	if (err < 0) {
		print_err("%s", nl_geterror(err));
		return err;
	}

	nl_socket_modify_cb(neigh_handler->sock, NL_CB_VALID,
			    NL_CB_CUSTOM, &get_route_cb, neigh_handler);

	err = nl_recvmsgs_default(neigh_handler->sock);
	if (err < 0) {
		print_err("%s", nl_geterror(err));
		__sync_fetch_and_or(
			&neigh_handler->neigh_status,
			GET_NEIGH_STATUS_ERR);
	}

	__sync_fetch_and_and(
		&neigh_handler->neigh_status,
		~GET_NEIGH_STATUS_IN_PROCESS);

	return err;
}
