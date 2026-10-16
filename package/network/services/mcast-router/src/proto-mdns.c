/*
 * mcast-router protocol: mDNS / DNS-SD (RFC 6762, 6763), 224.0.0.251 and
 * ff02::fb, port 5353.
 *
 * Queries (QR=0) are relayed towards providers with the "unicast response"
 * bit of every question cleared, so the answers come back as multicast and
 * are relayed by the same policy. Legacy one-shot queries (source port other
 * than 5353, e.g. "dig -p 5353 @224.0.0.251") expect a unicast reply to
 * their port: they go through a reply proxy. Responses (QR=1) are relayed
 * towards seekers; they must come from port 5353 (RFC 6762 section 6),
 * except replies to a proxied legacy query.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <stdio.h>
#include <string.h>

#include "mcr.h"

#define MDNS_PORT 5353
#define HDR_LEN 12

static unsigned get16(const uint8_t *d)
{
	return d[0] << 8 | d[1];
}

/* advance *off past a (possibly compressed) name; 0 on success */
static int skip_name(const uint8_t *d, size_t len, size_t *off)
{
	size_t o = *off;

	while (o < len) {
		uint8_t l = d[o];

		if (l == 0) {
			*off = o + 1;
			return 0;
		}
		if ((l & 0xc0) == 0xc0) {
			if (o + 2 > len)
				return -1;
			*off = o + 2;
			return 0;
		}
		if (l & 0xc0)
			return -1;      /* reserved label types */
		o += 1 + l;
	}
	return -1;
}

/* the name at off, decompressed and dotted, for the records */
static void read_name(const uint8_t *d, size_t len, size_t off, char *out, size_t outlen)
{
	size_t n = 0;
	int hops = 0;

	out[0] = 0;
	while (off < len && hops < 16) {
		uint8_t l = d[off];

		if (l == 0)
			break;
		if ((l & 0xc0) == 0xc0) {
			if (off + 2 > len)
				break;
			off = (get16(d + off) & 0x3fff);
			hops++;
			continue;
		}
		if ((l & 0xc0) || off + 1 + l > len)
			break;
		for (size_t i = 0; i < l && n + 2 < outlen; i++) {
			char c = d[off + 1 + i];
			out[n++] = (c >= 0x20 && c < 0x7f && c != '\t') ? c : '?';
		}
		if (n + 2 < outlen)
			out[n++] = '.';
		off += 1 + l;
	}
	if (n && out[n - 1] == '.')
		n--;
	out[n] = 0;
}

static enum mcr_kind mdns_classify(struct mcr_pkt *p)
{
	uint8_t *d = p->data;
	size_t len = p->len, off = HDR_LEN;
	unsigned flags, qd, an;
	int legacy;

	if (len < HDR_LEN)
		return MCR_IGNORE;
	flags = get16(d + 2);
	if ((flags >> 11) & 0xf)        /* opcode: standard query only */
		return MCR_IGNORE;
	qd = get16(d + 4);
	an = get16(d + 6);

	if (p->via_proxy) {
		/* unicast reply to a proxied legacy query */
		if (!(flags & 0x8000))
			return MCR_IGNORE;
		read_name(d, len, off, p->desc, sizeof(p->desc));
		return MCR_ANSWER;
	}

	if (!(flags & 0x8000)) {
		/* query: only those sent to the group, not unicast queries to the router */
		if (!p->multicast || qd == 0)
			return MCR_IGNORE;
		legacy = p->src_port != MDNS_PORT;
		for (unsigned i = 0; i < qd; i++) {
			if (i == 0)
				read_name(d, len, off, p->desc, sizeof(p->desc));
			if (skip_name(d, len, &off) || off + 4 > len)
				return MCR_IGNORE;
			if (!legacy)
				d[off + 2] &= 0x7f;     /* QU bit: ask for multicast answers */
			off += 4;
		}
		if (legacy) {
			p->need_proxy = 1;
			p->proxy_timeout = 3;
		}
		return MCR_QUERY;
	}

	/* response or announcement */
	if (p->src_port != MDNS_PORT || an == 0)
		return MCR_IGNORE;
	for (unsigned i = 0; i < qd; i++) {
		if (skip_name(d, len, &off) || off + 4 > len)
			return MCR_IGNORE;
		off += 4;
	}
	read_name(d, len, off, p->desc, sizeof(p->desc));
	return MCR_ANSWER;
}

const struct mcr_plugin mcr_mdns = {
	.name = "mdns",
	.port = MDNS_PORT,
	.group4 = "224.0.0.251",
	.group6 = "ff02::fb",
	.ttl = 255,                     /* RFC 6762 section 11 */
	.classify = mdns_classify,
};
