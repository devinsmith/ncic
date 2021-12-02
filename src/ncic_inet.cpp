/*
** Heavily based on pork_inet.c
** ncic_inet.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_inet.h"
#include "ncic_screen_io.h"

/*
** Write to a socket, deal with interrupted and incomplete writes. Returns
** the number of characters written to the socket on success, -1 on failure.
*/

ssize_t sock_write(int sock, void *buf, size_t len) {
	ssize_t n, written = 0;

	while (len > 0) {
		n = write(sock, buf, len);
		if (n == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;

			debug("sock: %s", strerror(errno));
			return (-1);
		}

		written += n;
		len -= n;
		buf = (char *) buf + n;
	}

	return (written);
}

/*
** printf-like function that writes to sockets.
*/

#ifdef HAVE_VASPRINTF

int sockprintf(int fd, const char *fmt, ...) {
	va_list ap;
	char *buf;
	ssize_t ret;

	va_start(ap, fmt);
	ret = vasprintf(&buf, fmt, ap);
	va_end(ap);

	ret = sock_write(fd, buf, ret);
	free(buf);

	return (ret);
}

#else

int sockprintf(int fd, const char *fmt, ...) {
	va_list ap;
	char buf[4096];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return (sock_write(fd, buf, strlen(buf)));
}

#endif

/*
** Returns the length of the sockaddr struct.
*/

static size_t sin_len(const struct sockaddr_storage *ss) {
	if (ss->ss_family == AF_INET6)
		return (sizeof(struct sockaddr_in6));

	return (sizeof(struct sockaddr_in));
}

/*
** Sets the port for the approprite socket family.
*/

void sin_set_port(struct sockaddr_storage *ss, in_port_t port) {
	if (ss->ss_family == AF_INET6)
		SIN6(ss)->sin6_port = port;

	SIN4(ss)->sin_port = port;
}

/*
** Get the port associated with a tcp service name.
*/

int get_port(const char *name, in_port_t *port) {
	struct servent *servent;

	servent = getservbyname(name, "tcp");
	if (servent != nullptr)
		*port = ntohs(servent->s_port);
	else {
		char *end;
		long temp_port;

		temp_port = strtol(name, &end, 10);

		if (*end != '\0') {
			debug("invalid port: %s", name);
			return (-1);
		}

		if (!VALID_PORT(temp_port)) {
			debug("invalid port: %s", name);
			return (-1);
		}

		*port = temp_port;
	}

	return (0);
}

/*
** Return a network byte ordered ipv4 or ipv6 address.
*/

int get_addr(const char *hostname, struct sockaddr_storage *addr) {
	struct addrinfo *res = nullptr;
	size_t len;
	int ret;

	if ((ret = getaddrinfo(hostname, NULL, NULL, &res)) != 0) {
		if (res != nullptr)
			freeaddrinfo(res);

		debug("getaddrinfo: %s: %s", hostname, gai_strerror(ret));
		return (-1);
	}

	switch (res->ai_addr->sa_family) {
		case AF_INET:
			len = sizeof(struct sockaddr_in);
			break;

		case AF_INET6:
			len = sizeof(struct sockaddr_in6);
			break;

		default:
			debug("unknown family: %d", res->ai_addr->sa_family);
			goto out_fail;
	}

	if (len < res->ai_addrlen) {
		debug("%u < %u", len, res->ai_addrlen);
		goto out_fail;
	}

	memcpy(addr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	return (0);

out_fail:
	freeaddrinfo(res);
	return (-1);
}

void sock_setkeepalive(int sockfd)
{
	int keep_alive = 1;
	int keep_idle = 30;
	int keep_cnt = 5;
	int keep_intvl = 10;

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keep_alive,
	    sizeof(keep_alive))) {
		debug("ERROR: setsockopt(), SO_KEEPALIVE: %s", strerror(errno));
	}
#ifdef __linux__
	/* Linux specific socket options */
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&keep_idle,
	    sizeof(keep_idle))) {
		debug("ERROR: setsocketopt(), SO_KEEPIDLE: %s", strerror(errno));
	}

	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keep_cnt,
	    sizeof(keep_cnt))) {
		debug("ERROR: setsocketopt(), SO_KEEPCNT: %s", strerror(errno));
	}

	if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keep_intvl,
	    sizeof(keep_intvl))) {
		debug("ERROR: setsocketopt(), SO_KEEPINTVL: %s", strerror(errno));
	}
#endif /* __linux__ */
}

int sock_setflags(int sock, uint32_t flags) {
	int ret;

	ret = fcntl(sock, F_SETFL, flags);
	if (ret == -1)
		debug("fnctl: %s", strerror(errno));

	return (ret);
}

int nb_connect(	struct sockaddr_storage *ss,
				struct sockaddr_storage *laddr,
				in_port_t port,
				int *dsock)
{
	int sock;
	int ret = 0;

	sock = socket(ss->ss_family, SOCK_STREAM, 0);
	if (sock < 0) {
		debug("socket: %s", strerror(errno));
		return (-1);
	}

	if (laddr != nullptr) {
		if (bind(sock, (struct sockaddr *) laddr, sin_len(laddr)) != 0) {
			screen_err_msg("Failed to bind: %s", strerror(errno));
		}
	}

	/* Set nonblocking socket */
	if (sock_setflags(sock, O_NONBLOCK) == -1)
		goto err_out;

	sin_set_port(ss, htons(port));

	if (connect(sock, (struct sockaddr *) ss, sin_len(ss)) != 0) {
		if (errno != EINPROGRESS) {
			debug("connect: %s", strerror(errno));
			goto err_out;
		}

		ret = -EINPROGRESS;
	} else {
		/* Turn off non-blocking. */
		if (sock_setflags(sock, 0) == -1)
			goto err_out;
	}

	if (ret == 0)
		debug("immediate connect");

	*dsock = sock;
	return (ret);

err_out:
	close(sock);
	return (-1);
}

int sock_is_error(int sock) {
	int error;
	socklen_t errlen = sizeof(error);
	int ret;

	ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &errlen);
	if (ret == -1) {
		if (errno == ENOTSOCK)
			return (0);

		debug("getsockopt: %d: %s", sock, strerror(errno));
		return (errno);
	}

	return (error);
}

