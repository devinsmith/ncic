/*
** Based on pork_proto.c
** ncic_proto.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <cstdlib>
#include <cstring>

#include "ncic_util.h"
#include "ncic_proto.h"

static struct pork_proto *proto_table[PROTO_MAX + 1];

extern "C" int irc_proto_init(struct pork_proto *proto);

struct pork_proto *proto_get(int protocol) {
	if (protocol > PROTO_MAX || protocol < -1)
		return (nullptr);

	return (proto_table[protocol + 1]);
}

static int proto_new(	int protocol,
				const char *name,
				int (*init_func)(struct pork_proto *))
{
	struct pork_proto *new_proto;
	int ret;

	if (protocol < -1 || protocol > PROTO_MAX ||
		proto_table[protocol + 1] != nullptr || init_func == nullptr)
	{
		return (-1);
	}

	new_proto = (struct pork_proto *)xcalloc(1, sizeof(*new_proto));
	xstrncpy(new_proto->name, name, sizeof(new_proto->name));

	ret = init_func(new_proto);
	if (ret != 0) {
		free(new_proto);
		return (ret);
	}

	proto_table[protocol + 1] = new_proto;
	return (0);
}

void proto_destroy(void) {
	size_t i;

	for (i = 0 ; i <= PROTO_MAX ; i++)
		free(proto_table[i]);
}

int proto_init(void) {
  proto_new(PROTO_IRC, "IRC", irc_proto_init);

	return (0);
}
