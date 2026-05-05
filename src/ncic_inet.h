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

int get_port(const char *name, in_port_t *port);
int get_addr(const char *hostname, struct sockaddr_storage *addr);
int sock_setflags(int sock, u_int32_t flags);
void sock_setkeepalive(int sock);
int sock_is_error(int sock);

#endif /* __NCIC_INET_H__ */
