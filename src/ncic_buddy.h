/*
** Based on pork_buddy.h
** ncic_buddy.h - Buddy list / permit / deny management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_BUDDY_H__
#define __NCIC_BUDDY_H__

#include "ncic_list.h"

enum {
	STATUS_OFFLINE,
};

struct pork_acct;

struct buddy_pref {
	struct bgroup *group;
	dlist_t *block_list;
	u_int32_t num_buddies;
	char **user_list;
	int user_list_size;
};

struct bgroup {
	char *name;
	u_int32_t refnum;
	u_int32_t num_members;
	u_int32_t high_refnum;
	int n_buddies;
	struct buddy **buddy_list;
};

struct buddy {
	char *name;
	char *nname;
	char *userhost;
	u_int32_t refnum;
	u_int32_t idle_time;
	u_int32_t status:2;
	u_int32_t notify:1;
	u_int32_t ignore:1;
	unsigned int id;
	struct slist_cell *blist_line;
};

char *buddy_name(struct pork_acct *acct, char *buddy);

struct buddy *buddy_find(struct pork_acct *acct, const char *screen_name, int number);

#endif /* __NCIC_BUDDY_H__ */
