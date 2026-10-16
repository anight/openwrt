/*
 * mcr-probe: test tool for mcast-router (plays a device in a network).
 *
 *   mcr-probe listen -i <ifname> -g <group> -p <port> [-t secs] [-R reply]
 *       join <group> on <ifname>, print every packet received on <port>;
 *       with -R, answer each one unicast to its sender with <reply>
 *   mcr-probe send -i <ifname> -d <dest> -p <port> (-s text | -x hex)
 *                  [-l localport] [-t secs]
 *       send one packet to <dest> (group or unicast) out of <ifname>, then
 *       print the replies arriving for <secs> (default 3)
 *
 * Text payloads understand \r and \n. IPv6 when the address has a colon.
 * Output: one line per packet: "<from> <len> <first line or hex>".
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static size_t unescape(const char *s, unsigned char *out, size_t cap)
{
	size_t n = 0;

	for (; *s && n < cap; s++) {
		if (s[0] == '\\' && s[1] == 'r') { out[n++] = '\r'; s++; }
		else if (s[0] == '\\' && s[1] == 'n') { out[n++] = '\n'; s++; }
		else out[n++] = *s;
	}
	return n;
}

static size_t unhex(const char *s, unsigned char *out, size_t cap)
{
	size_t n = 0;
	unsigned v;

	while (*s && n < cap) {
		if (*s == ' ' || *s == ':') { s++; continue; }
		if (sscanf(s, "%2x", &v) != 1)
			break;
		out[n++] = v;
		s += 2;
	}
	return n;
}

static void show(const struct sockaddr_storage *ss, const unsigned char *b, ssize_t n)
{
	char a[INET6_ADDRSTRLEN];
	int port;
	int text = n > 0;

	if (ss->ss_family == AF_INET) {
		inet_ntop(AF_INET, &((struct sockaddr_in *)ss)->sin_addr, a, sizeof(a));
		port = ntohs(((struct sockaddr_in *)ss)->sin_port);
	} else {
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *)ss)->sin6_addr, a, sizeof(a));
		port = ntohs(((struct sockaddr_in6 *)ss)->sin6_port);
	}
	for (ssize_t i = 0; i < n && i < 40; i++)
		if ((b[i] < 0x20 && b[i] != '\r' && b[i] != '\n') || b[i] > 0x7e)
			text = 0;
	printf("%s:%d %zd ", a, port, n);
	if (text) {
		for (ssize_t i = 0; i < n && b[i] != '\r' && b[i] != '\n'; i++)
			putchar(b[i]);
	} else {
		for (ssize_t i = 0; i < n && i < 48; i++)
			printf("%02x", b[i]);
	}
	putchar('\n');
	fflush(stdout);
}

static int make_socket(int family, int port, int ifindex, const char *group)
{
	int fd = socket(family, SOCK_DGRAM, 0), one = 1;

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (family == AF_INET) {
		struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons(port) };
		struct ip_mreqn m = { .imr_ifindex = ifindex };
		unsigned char ttl = 255;

		setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
		m.imr_ifindex = ifindex;
		setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &m, sizeof(m));
		if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) { perror("bind"); exit(1); }
		if (group && inet_pton(AF_INET, group, &m.imr_multiaddr) == 1 &&
		    setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m)) < 0) { perror("join"); exit(1); }
	} else {
		struct sockaddr_in6 sa = { .sin6_family = AF_INET6, .sin6_port = htons(port) };
		struct ipv6_mreq m = { .ipv6mr_interface = ifindex };
		int hops = 255;

		setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
		setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops));
		setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex));
		if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) { perror("bind"); exit(1); }
		if (group && inet_pton(AF_INET6, group, &m.ipv6mr_multiaddr) == 1 &&
		    setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &m, sizeof(m)) < 0) { perror("join"); exit(1); }
	}
	return fd;
}

static void receive(int fd, int secs, const unsigned char *reply, size_t rlen)
{
	time_t end = time(NULL) + secs;
	unsigned char b[9216];

	while (time(NULL) < end) {
		struct pollfd p = { .fd = fd, .events = POLLIN };
		struct sockaddr_storage ss;
		socklen_t sl = sizeof(ss);
		ssize_t n;

		if (poll(&p, 1, 200) <= 0)
			continue;
		n = recvfrom(fd, b, sizeof(b), 0, (struct sockaddr *)&ss, &sl);
		if (n < 0)
			continue;
		show(&ss, b, n);
		if (reply)
			sendto(fd, reply, rlen, 0, (struct sockaddr *)&ss, sl);
	}
}

int main(int argc, char **argv)
{
	const char *ifname = NULL, *group = NULL, *dest = NULL, *text = NULL, *hex = NULL, *rtext = NULL;
	int port = 0, lport = 0, secs = 3, opt, ifindex, family;
	unsigned char buf[9216], rbuf[2048];
	size_t len = 0, rlen = 0;
	int listen_mode;

	if (argc < 2 || (strcmp(argv[1], "listen") && strcmp(argv[1], "send"))) {
		fprintf(stderr, "usage: mcr-probe listen|send ... (see the source header)\n");
		return 2;
	}
	listen_mode = !strcmp(argv[1], "listen");
	optind = 2;
	while ((opt = getopt(argc, argv, "i:g:d:p:l:s:x:t:R:")) != -1) {
		switch (opt) {
		case 'i': ifname = optarg; break;
		case 'g': group = optarg; break;
		case 'd': dest = optarg; break;
		case 'p': port = atoi(optarg); break;
		case 'l': lport = atoi(optarg); break;
		case 's': text = optarg; break;
		case 'x': hex = optarg; break;
		case 't': secs = atoi(optarg); break;
		case 'R': rtext = optarg; break;
		default: return 2;
		}
	}
	if (!ifname || !port || !(ifindex = if_nametoindex(ifname))) {
		fprintf(stderr, "need -i <existing ifname> and -p <port>\n");
		return 2;
	}
	if (rtext)
		rlen = unescape(rtext, rbuf, sizeof(rbuf));

	if (listen_mode) {
		if (!group) { fprintf(stderr, "need -g <group>\n"); return 2; }
		family = strchr(group, ':') ? AF_INET6 : AF_INET;
		receive(make_socket(family, port, ifindex, group), secs, rtext ? rbuf : NULL, rlen);
		return 0;
	}

	if (!dest || (!text && !hex)) { fprintf(stderr, "need -d <dest> and -s or -x\n"); return 2; }
	len = text ? unescape(text, buf, sizeof(buf)) : unhex(hex, buf, sizeof(buf));
	family = strchr(dest, ':') ? AF_INET6 : AF_INET;
	int fd = make_socket(family, lport, ifindex, NULL);
	if (family == AF_INET) {
		struct sockaddr_in d = { .sin_family = AF_INET, .sin_port = htons(port) };
		inet_pton(AF_INET, dest, &d.sin_addr);
		if (sendto(fd, buf, len, 0, (struct sockaddr *)&d, sizeof(d)) < 0) { perror("send"); return 1; }
	} else {
		struct sockaddr_in6 d = { .sin6_family = AF_INET6, .sin6_port = htons(port), .sin6_scope_id = ifindex };
		inet_pton(AF_INET6, dest, &d.sin6_addr);
		if (sendto(fd, buf, len, 0, (struct sockaddr *)&d, sizeof(d)) < 0) { perror("send"); return 1; }
	}
	receive(fd, secs, NULL, 0);
	return 0;
}
