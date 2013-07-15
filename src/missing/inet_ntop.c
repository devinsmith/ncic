#include <config.h>

#ifndef HAVE_INET_NTOP

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#ifndef INET_ADDRSTRLEN
#	define INET_ADDRSTRLEN 16
#endif

#ifdef WIN32
const char *inet_ntop(int af, const void *src, char *dst, size_t len)
{
	if (af == AF_INET) {
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, src, sizeof(struct in_addr));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in),
		    dst, len, NULL, 0, NI_NUMERICHOST);
		return dst;
	} else if (af == AF_INET6) {
		struct sockaddr_in6 in6;
		memset(&in6, 0, sizeof(in6));
		in6.sin6_family = AF_INET6;
		memcpy(&in6.sin6_addr, src, sizeof(struct in_addr6));
		getnameinfo((struct sockaddr *)&in6,
		    sizeof(struct sockaddr_in6), dst, len, NULL, 0,
		    NI_NUMERICHOST);
	}
	return (NULL);
}
#else
/*
** This is an IPv4 only inet_ntop(3) replacement function.
*/
const char *inet_ntop(int af, const void *src, char *dst, size_t len) {
	const char *tmp = src;

	if (af != AF_INET) {
		errno = EAFNOSUPPORT;
		return (NULL);
	}

	if (len < INET_ADDRSTRLEN) {
		errno = ENOSPC;
		return (NULL);
	}

	snprintf(dst, len, "%u.%u.%u.%u", tmp[0], tmp[1], tmp[2], tmp[3]);

	return (dst);
}
#endif /* WIN32 */

#endif
