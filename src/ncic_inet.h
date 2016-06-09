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

#ifdef INET6_ADDRSTRLEN
#	define MAX_IPLEN 	INET6_ADDRSTRLEN
#elif defined INET_ADDRSTRLEN
#	define MAX_IPLEN 	INET_ADDRSTRLEN
#else
#	define MAX_IPLEN	46
#endif

#define MAX_HOSTLEN		256

#ifdef WIN32 /* hack for Windows */
typedef uint16_t in_port_t;
#endif

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
struct sockaddr_storage {
	struct	sockaddr __ss_sockaddr;
	char	__ss_pad[128 - sizeof(struct sockaddr)];
};
#	define ss_family __ss_sockaddr.sa_family
#endif

#ifndef HAVE_STRUCT_IN6_ADDR
struct in6_addr {
	u_int8_t s6_addr[16];
};
#endif

#ifndef HAVE_STRUCT_SOCKADDR_IN6
struct sockaddr_in6 {
	unsigned short sin6_family;
	u_int16_t sin6_port;
	u_int32_t sin6_flowinfo;
	struct in6_addr sin6_addr;
};
#endif

#ifdef HAVE___SS_FAMILY
	#define ss_family __ss_family
#endif

#define SIN4(x) ((struct sockaddr_in *) (x))
#define SIN6(x) ((struct sockaddr_in6 *) (x))
#define VALID_PORT(x) ((((x) & 0xffff) == (x)) && ((x) != 0))

int nb_connect(	struct sockaddr_storage *ss,
				struct sockaddr_storage *local,
				in_port_t port,
				int *dsock);

ssize_t sock_write(int sock, void *buf, size_t len);
int sockprintf(int fd, const char *fmt, ...);
int get_port(const char *name, in_port_t *port);
int get_addr(const char *hostname, struct sockaddr_storage *addr);
void sin_set_port(struct sockaddr_storage *ss, in_port_t port);
int sock_setflags(int sock, u_int32_t flags);
int sock_is_error(int sock);
int sock_listen(struct sockaddr_storage *ss, in_port_t port);

int get_hostname(struct sockaddr_storage *addr,
						char *hostbuf,
						size_t len);

#endif /* __NCIC_INET_H__ */
