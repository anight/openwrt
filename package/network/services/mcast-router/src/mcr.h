/*
 * mcast-router: relays link-local multicast discovery (mDNS, SSDP, ...)
 * between networks by policy. Plugin interface.
 *
 * Each protocol is one source file (proto-mdns.c, proto-ssdp.c) defining a
 * struct mcr_plugin, built into the daemon and listed in builtin[] in
 * mcast-router.c. It describes the protocol (port, groups, TTL) and
 * classifies every received packet:
 *
 *   MCR_QUERY   sent by a seeker; relayed to the networks of the providers
 *               the seeker may discover
 *   MCR_ANSWER  sent by a provider (answer or announcement); relayed to the
 *               networks of the seekers that may discover it
 *   MCR_IGNORE  not relayed
 *
 * classify() may rewrite the packet in place (it is relayed as rewritten),
 * may ask for the query to be sent through a reply proxy (answers come back
 * unicast to the sender's address and port, so the router must pass them
 * on) and fills desc with a short description for the records (service
 * type, search target).
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#ifndef MCR_H
#define MCR_H

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

#define MCR_DESC_LEN 96

enum mcr_kind {
	MCR_IGNORE = 0,
	MCR_QUERY = 1,
	MCR_ANSWER = 2,
};

struct mcr_pkt {
	uint8_t *data;          /* packet payload, may be rewritten in place */
	size_t len;             /* payload length (may be changed, up to cap) */
	size_t cap;             /* buffer size */
	int family;             /* AF_INET or AF_INET6 */
	struct sockaddr_storage src;
	uint16_t src_port;      /* host byte order */
	int multicast;          /* sent to the protocol's group (not unicast to the router) */
	int via_proxy;          /* arrived on a reply-proxy socket: a reply to a relayed query */

	/* set by classify() */
	int need_proxy;         /* relay this query through a reply proxy */
	int proxy_timeout;      /* seconds to wait for replies (default 5) */
	char desc[MCR_DESC_LEN];
};

struct mcr_plugin {
	const char *name;       /* "mdns" */
	uint16_t port;          /* 5353 */
	const char *group4;     /* "224.0.0.251", or NULL for IPv6 only */
	const char *group6;     /* "ff02::fb", or NULL for IPv4 only */
	int ttl;                /* IP TTL / hop limit of relayed packets */
	enum mcr_kind (*classify)(struct mcr_pkt *p);
};

#endif
