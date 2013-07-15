/*
** Based on pork_opt.c
** ncic_opt.c - Command-line argument handler.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifndef HAVE_GETOPT_LONG
#	include <getopt_missing.h>
#else
#	include <getopt.h>
#endif

#include "ncic.h"
//#include <pork_missing.h>
#include "ncic_inet.h"
#include "ncic_opt.h"

#define OPTSTRING "vH:p:h"

static u_int32_t flags;

static void print_help_text(void);

struct sockaddr_storage local_addr;
in_port_t local_port;

static const struct option longopts[] = {
	{ "host",		required_argument,		0, 'H'	},
	{ "port",		required_argument,		0, 'p'	},
	{ "version",		no_argument,			0, 'v'	},
	{ "help",		no_argument,			0, 'h'	},
	{ NULL, 0, NULL, 0 },
};

int get_options(int argc, char *const argv[]) {
	int opt;

	while ((opt = getopt_long(argc, argv, OPTSTRING, longopts, NULL)) != EOF) {
		switch (opt) {
			case 'H':
				if (get_addr(optarg, &local_addr) != 0) {
					fprintf(stderr, "Error: Invalid local address: %s\n",
						optarg);
					return (-1);
				}

				flags |= USE_ADDR;
				break;

			case 'p':
				if (get_port(optarg, &local_port) != 0) {
					fprintf(stderr, "Error: Invalid local port: %s\n", optarg);
					return (-1);
				}

				local_port = htons(local_port);
				flags |= USE_PORT;
				break;

			case 'h':
				print_help_text();
				exit(0);
				break;

			case 'v':
				printf("ncic version %s\n", VERSION);
				printf("Written by Devin Smith <devin@devinsmith.net>\n");
				printf("http://devinsmith.net/programs/ncic.php\n");
				exit(0);
				break;
			default:
				return (-1);
		}
	}

	return (0);
}

int opt_is_set(u_int32_t opt) {
	return ((flags & opt) != 0);
}

static void print_help_text(void) {
	const char usage[] =
"Usage: ncic [options]\n"
"-H or --host <addr>    Use the local address specified for outgoing connections\n"
"-p or --port <port>    Use the local port specified for the main connection\n"
"-h or --help           Display this help text\n"
"-v or --version        Display version information and exit\n";

	printf("%s", usage);
}
