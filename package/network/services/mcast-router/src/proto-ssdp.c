/*
 * mcast-router protocol: SSDP (UPnP device architecture), 239.255.255.250
 * and ff02::c, port 1900.
 *
 *   M-SEARCH * HTTP/1.1   query from a control point; relayed towards
 *                         devices through a reply proxy, because devices
 *                         answer unicast to the searcher's address and port
 *   HTTP/1.1 200 OK       such an answer, arriving on the proxy; passed back
 *                         to the searcher
 *   NOTIFY * HTTP/1.1     announcement (ssdp:alive, ssdp:byebye) from a
 *                         device; relayed towards control points
 *
 * The answers and announcements carry a LOCATION URL with the device's own
 * address; fetching it is ordinary routed traffic, decided by the firewall.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mcr.h"

static int starts(const uint8_t *d, size_t len, const char *s)
{
	size_t n = strlen(s);

	return len >= n && !strncasecmp((const char *)d, s, n);
}

/* value of header 'name' (without the colon), copied into out */
static int header(const uint8_t *d, size_t len, const char *name, char *out, size_t outlen)
{
	size_t nl = strlen(name), i = 0;

	out[0] = 0;
	while (i < len) {
		size_t e = i;

		while (e < len && d[e] != '\n')
			e++;
		/* line d[i..e) */
		if (e - i > nl && !strncasecmp((const char *)d + i, name, nl) && d[i + nl] == ':') {
			size_t v = i + nl + 1, n = 0;

			while (v < e && (d[v] == ' ' || d[v] == '\t'))
				v++;
			while (v < e && d[v] != '\r' && n + 1 < outlen) {
				char c = d[v++];
				out[n++] = (c >= 0x20 && c < 0x7f && c != '\t') ? c : '?';
			}
			out[n] = 0;
			return 1;
		}
		i = e + 1;
	}
	return 0;
}

static enum mcr_kind ssdp_classify(struct mcr_pkt *p)
{
	const uint8_t *d = p->data;
	size_t len = p->len;
	char v[MCR_DESC_LEN - 40], nts[32];

	if (p->via_proxy) {
		if (!starts(d, len, "HTTP/1.1 200"))
			return MCR_IGNORE;
		header(d, len, "ST", p->desc, sizeof(p->desc));
		return MCR_ANSWER;
	}
	if (!p->multicast)
		return MCR_IGNORE;

	if (starts(d, len, "M-SEARCH * HTTP/1.1")) {
		int mx = 0;

		if (header(d, len, "MX", v, sizeof(v)))
			mx = atoi(v);
		/* devices answer within MX seconds (1-5 by the spec) */
		p->need_proxy = 1;
		p->proxy_timeout = mx < 1 ? 5 : mx > 8 ? 10 : mx + 2;
		header(d, len, "ST", p->desc, sizeof(p->desc));
		return MCR_QUERY;
	}
	if (starts(d, len, "NOTIFY * HTTP/1.1")) {
		header(d, len, "NT", v, sizeof(v));
		if (header(d, len, "NTS", nts, sizeof(nts)))
			snprintf(p->desc, sizeof(p->desc), "%s (%s)", v, nts);
		else
			snprintf(p->desc, sizeof(p->desc), "%s", v);
		return MCR_ANSWER;
	}
	return MCR_IGNORE;
}

const struct mcr_plugin mcr_ssdp = {
	.name = "ssdp",
	.port = 1900,
	.group4 = "239.255.255.250",
	.group6 = "ff02::c",
	.ttl = 2,                       /* UPnP device architecture: default TTL */
	.classify = ssdp_classify,
};
