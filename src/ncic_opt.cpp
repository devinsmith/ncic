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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ncic_opt.h"

static void print_help_text();

struct sockaddr_storage local_addr;
in_port_t local_port;

int get_options(int argc, char *const argv[])
{

	while (--argc) {
	  char *p = *++argv;

	  if (!strcmp(p, "-v")) {
      printf("ncic version %s\n", VERSION);
      printf("Written by Devin Smith <devin@devinsmith.net>\n");
      printf("http://devinsmith.net/programs/ncic.php\n");
      exit(0);
	  } else if (!strcmp(p, "-h")) {
      print_help_text();
      exit(0);
	  } else {
	    print_help_text();
	    exit(1);
	  }
	}

	return (0);
}

static void print_help_text() {
	const char usage[] =
"Usage: ncic [options]\n"
"-h or --help           Display this help text\n"
"-v or --version        Display version information and exit\n";

	printf("%s", usage);
}
