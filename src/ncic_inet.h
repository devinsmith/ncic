/*
** Heavily based on pork_inet.h
** ncic_inet.h
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_INET_H__
#define __NCIC_INET_H__

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef INET6_ADDRSTRLEN
#	define MAX_IPLEN 	INET6_ADDRSTRLEN
#elif defined INET_ADDRSTRLEN
#	define MAX_IPLEN 	INET_ADDRSTRLEN
#else
#	define MAX_IPLEN	46
#endif

#define SIN4(x) ((struct sockaddr_in *) (x))
#define SIN6(x) ((struct sockaddr_in6 *) (x))
#define VALID_PORT(x) ((((x) & 0xffff) == (x)) && ((x) != 0))

int nb_connect(struct sockaddr_storage *ss,
				in_port_t port,
				int *dsock);

ssize_t sock_write(int sock, void *buf, size_t len);
int sockprintf(int fd, const char *fmt, ...);
int get_port(const char *name, in_port_t *port);
int get_addr(const char *hostname, struct sockaddr_storage *addr);
void sin_set_port(struct sockaddr_storage *ss, in_port_t port);
int sock_setflags(int sock, u_int32_t flags);
void sock_setkeepalive(int sock);
int sock_is_error(int sock);


#ifdef __cplusplus
}
#endif

#endif /* __NCIC_INET_H__ */
