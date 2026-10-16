/*
 * Unit tests for the protocol classifiers, run on the build host:
 *   make test
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>

#include "mcr.h"

extern const struct mcr_plugin mcr_mdns, mcr_ssdp;

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int run(const struct mcr_plugin *pl, uint8_t *buf, size_t len, int src_port, int multicast, int via_proxy, struct mcr_pkt *p)
{
	struct sockaddr_in *s = (struct sockaddr_in *)&p->src;

	memset(p, 0, sizeof(*p));
	p->data = buf;
	p->len = len;
	p->cap = 9216;
	p->family = AF_INET;
	s->sin_family = AF_INET;
	s->sin_port = htons(src_port);
	p->src_port = src_port;
	p->multicast = multicast;
	p->via_proxy = via_proxy;
	p->proxy_timeout = 5;
	return pl->classify(p);
}

/* query: id 0, flags 0, 1 question "_googlecast._tcp.local" PTR, class IN with QU bit */
static uint8_t mdns_query[] = {
	0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
	11, '_', 'g', 'o', 'o', 'g', 'l', 'e', 'c', 'a', 's', 't',
	4, '_', 't', 'c', 'p', 5, 'l', 'o', 'c', 'a', 'l', 0,
	0, 12, 0x80, 1,
};

/* response: flags 0x8400, 0 questions, 1 answer: PTR record, name compressed later */
static uint8_t mdns_resp[] = {
	0, 0, 0x84, 0, 0, 0, 0, 1, 0, 0, 0, 0,
	8, '_', 'a', 'i', 'r', 'p', 'l', 'a', 'y', 4, '_', 't', 'c', 'p', 5, 'l', 'o', 'c', 'a', 'l', 0,
	0, 12, 0, 1, 0, 0, 0x11, 0x94, 0, 2, 0xc0, 12,
};

static void test_mdns(void)
{
	uint8_t b[256];
	struct mcr_pkt p;

	memcpy(b, mdns_query, sizeof(mdns_query));
	CHECK(run(&mcr_mdns, b, sizeof(mdns_query), 5353, 1, 0, &p) == MCR_QUERY, "query");
	CHECK(!strcmp(p.desc, "_googlecast._tcp.local"), "query desc '%s'", p.desc);
	CHECK(b[sizeof(mdns_query) - 2] == 0x00, "QU bit cleared");
	CHECK(!p.need_proxy, "no proxy for port-5353 queries");

	memcpy(b, mdns_query, sizeof(mdns_query));
	CHECK(run(&mcr_mdns, b, sizeof(mdns_query), 40000, 1, 0, &p) == MCR_QUERY && p.need_proxy, "legacy query proxied");
	CHECK(b[sizeof(mdns_query) - 2] == 0x80, "legacy query untouched");

	memcpy(b, mdns_query, sizeof(mdns_query));
	CHECK(run(&mcr_mdns, b, sizeof(mdns_query), 5353, 0, 0, &p) == MCR_IGNORE, "unicast query to the router ignored");
	CHECK(run(&mcr_mdns, b, 20, 5353, 1, 0, &p) == MCR_IGNORE, "truncated query ignored");
	CHECK(run(&mcr_mdns, b, 5, 5353, 1, 0, &p) == MCR_IGNORE, "short packet ignored");

	memcpy(b, mdns_resp, sizeof(mdns_resp));
	CHECK(run(&mcr_mdns, b, sizeof(mdns_resp), 5353, 1, 0, &p) == MCR_ANSWER, "response");
	CHECK(!strcmp(p.desc, "_airplay._tcp.local"), "response desc '%s'", p.desc);
	CHECK(run(&mcr_mdns, b, sizeof(mdns_resp), 5353, 0, 0, &p) == MCR_ANSWER, "unicast response to the router relayed");
	CHECK(run(&mcr_mdns, b, sizeof(mdns_resp), 1234, 1, 0, &p) == MCR_IGNORE, "response not from 5353 ignored");
	CHECK(run(&mcr_mdns, b, sizeof(mdns_resp), 5353, 0, 1, &p) == MCR_ANSWER, "proxied legacy reply");

	/* compression loop must not hang */
	uint8_t loop[] = { 0, 0, 0x84, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xc0, 12, 0, 12, 0, 1, 0, 0, 0, 1, 0, 0 };
	run(&mcr_mdns, loop, sizeof(loop), 5353, 1, 0, &p);
	CHECK(1, "loop");
}

static void test_ssdp(void)
{
	char b[1024];
	struct mcr_pkt p;
	const char *msearch = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: urn:dial-multiscreen-org:service:dial:1\r\n\r\n";
	const char *notify = "NOTIFY * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nNT: upnp:rootdevice\r\nNTS: ssdp:alive\r\nLOCATION: http://10.20.5.2:8008/ssdp/device-desc.xml\r\n\r\n";
	const char *ok = "HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=1800\r\nST: urn:dial-multiscreen-org:service:dial:1\r\nLOCATION: http://10.20.5.2:8008/\r\n\r\n";

	strcpy(b, msearch);
	CHECK(run(&mcr_ssdp, (uint8_t *)b, strlen(b), 50000, 1, 0, &p) == MCR_QUERY, "m-search");
	CHECK(p.need_proxy && p.proxy_timeout == 4, "m-search proxy %d %d", p.need_proxy, p.proxy_timeout);
	CHECK(!strcmp(p.desc, "urn:dial-multiscreen-org:service:dial:1"), "m-search desc '%s'", p.desc);
	CHECK(run(&mcr_ssdp, (uint8_t *)b, strlen(b), 50000, 0, 0, &p) == MCR_IGNORE, "unicast m-search ignored");

	strcpy(b, notify);
	CHECK(run(&mcr_ssdp, (uint8_t *)b, strlen(b), 1900, 1, 0, &p) == MCR_ANSWER, "notify");
	CHECK(!strcmp(p.desc, "upnp:rootdevice (ssdp:alive)"), "notify desc '%s'", p.desc);

	strcpy(b, ok);
	CHECK(run(&mcr_ssdp, (uint8_t *)b, strlen(b), 1900, 0, 1, &p) == MCR_ANSWER, "200 via proxy");
	CHECK(run(&mcr_ssdp, (uint8_t *)b, strlen(b), 1900, 0, 0, &p) == MCR_IGNORE, "200 not via proxy ignored");
	CHECK(run(&mcr_ssdp, (uint8_t *)"GET / HTTP/1.1\r\n\r\n", 18, 1900, 1, 0, &p) == MCR_IGNORE, "other ignored");
}

/* random and mutated packets: must never read or write out of bounds (run under ASan) */
static void test_fuzz(void)
{
	static uint8_t b[600];
	struct mcr_pkt p;
	unsigned seed = 1;

	for (int i = 0; i < 200000; i++) {
		size_t len = (seed = seed * 1103515245 + 12345) % sizeof(b);
		const uint8_t *base = (i & 1) ? mdns_query : mdns_resp;
		size_t blen = (i & 1) ? sizeof(mdns_query) : sizeof(mdns_resp);

		for (size_t k = 0; k < len; k++)
			b[k] = (seed = seed * 1103515245 + 12345) >> 16;
		if (i % 3 == 0 && len >= blen) {        /* mutate a valid packet */
			memcpy(b, base, blen);
			b[(seed >> 8) % blen] ^= 1 << ((seed >> 4) & 7);
		}
		run(&mcr_mdns, b, len, (i & 2) ? 5353 : 1234, i & 4, i & 8, &p);
		run(&mcr_ssdp, b, len, 1900, i & 4, i & 8, &p);
	}
}

int main(void)
{
	test_mdns();
	test_ssdp();
	test_fuzz();
	printf(failures ? "%d failure(s)\n" : "all tests passed\n", failures);
	return failures != 0;
}
