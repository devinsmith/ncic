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

enum {
	STATUS_OFFLINE,
	STATUS_ACTIVE,
	STATUS_IDLE,
	STATUS_AWAY
};

struct pork_acct;

struct buddy_pref {
	struct bgroup *group;
	dlist_t *block_list;
	u_int16_t privacy_mode;
	u_int32_t num_buddies;
	//hash_t buddy_hash;
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
	struct slist_cell *blist_line;
};

struct buddy {
	char *name;
	char *nname;
	char *userhost;
	struct bgroup *group;
	u_int32_t refnum;
	u_int32_t signon_time;
	u_int32_t idle_time;
	u_int32_t status:2;
	u_int32_t notify:1;
	u_int32_t ignore:1;
	unsigned int id;
	struct slist_cell *blist_line;
};

int buddy_update(	struct pork_acct *acct,
					struct buddy *buddy,
					void *data);

char *buddy_name(struct pork_acct *acct, char *buddy);

struct bgroup *group_find(struct pork_acct *acct, char *group_name);
struct buddy *buddy_find(struct pork_acct *acct, const char *screen_name, int number);

struct buddy *buddy_add(struct pork_acct *acct,
			const char *screen_name,
			int id);
int buddy_remove(struct pork_acct *acct, const char *screen_name, int id);

int buddy_init(struct pork_acct *acct);
void buddy_destroy(struct pork_acct *acct);

struct bgroup *group_add(struct pork_acct *acct, char *group_name);

#endif /* __NCIC_BUDDY_H__ */
