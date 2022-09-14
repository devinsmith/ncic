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

#include "ncic_opt.h"

char *g_log_file = nullptr;

static void print_help_text()
{
  const char usage[] =
          "Usage: ncic [options]\n"
          "-h or --help           Display this help text\n"
          "-v or --version        Display version information and exit\n";

  printf("%s", usage);
}

static void args_required(char * arg, const char *argname)
{
  fprintf(stderr, "ncic: option '%s' expects a parameter (%s)\n", arg,
    argname);

  exit(1);
}

int get_options(int argc, char *const argv[])
{

	while (--argc) {
	  char *p = *++argv;

	  if (!strcmp(p, "-v")) {
      printf("ncic version %s\n", VERSION);
      printf("Written by Devin Smith <devin@devinsmith.net>\n");
      printf("http://devinsmith.net/programs/ncic.html\n");
      exit(0);
	  } else if (!strcmp(p, "-h")) {
      print_help_text();
      exit(0);
    } else if (!strcmp(p, "--log")) {
      if (!argv[1]) {
        args_required(p, "log");
      }
      g_log_file = *++argv, --argc;
	  } else {
	    print_help_text();
	    exit(1);
	  }
	}

	return (0);
}