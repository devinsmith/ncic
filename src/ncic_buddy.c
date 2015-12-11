/*
** Based on pork_buddy.c
** ncic_buddy.c - Buddy list / permit / deny management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <unistd.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "ncic.h"
//#include "ncic_missing.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_acct.h"
#include "ncic_proto.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_buddy.h"
#include "ncic_misc.h"
#include "ncic_set.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"

static int buddy_came_online(struct pork_acct *acct, struct buddy *buddy);
static int buddy_went_offline(struct pork_acct *acct, struct buddy *buddy);
static void buddy_offline(struct pork_acct *acct, struct buddy *buddy);

static void buddy_cleanup(void *data) {
	struct buddy *buddy = data;

	free(buddy->nname);
	free(buddy->name);
	free(buddy->userhost);
	free(buddy);
}

static void group_cleanup(void *param __notused, void *data) {
	struct bgroup *group = data;

	/*
	** The memory allocated for the buddies in the buddy_list of
	** each group is freed when the buddy hash is destroyed, so
	** we don't have to do it here.
	*/
	free(group->buddy_list);
	free(group->name);
	free(group);
}

static void generic_cleanup(void *param __notused, void *data) {
	free(data);
}

int
buddy_remove(struct pork_acct *acct, const char *screen_name, int id) {
	struct buddy *buddy;
	char nname[NUSER_LEN];

	/* Sometimes nakenchat issues a buddy remove for a user that isn't on
	 * the chat. If this happens, screen_name will be NULL, so just ignore
	 * it. */
	if (screen_name == NULL)
		return (0);

	acct->proto->normalize(nname, screen_name, sizeof(nname));
	buddy = buddy_find(acct, nname, id);
	if (buddy == NULL)
		return (-1);

	buddy_went_offline(acct, buddy);

	if (buddy->status != STATUS_OFFLINE)
		buddy_offline(acct, buddy);

	acct->buddy_pref->num_buddies--;

	buddy_cleanup(buddy);
	return (0);
}

/*
** Add the buddy whose screen name is "screen_name" to the specified
** group.
*/

struct buddy *buddy_add(struct pork_acct *acct,
			const char *screen_name,
			int id)
{
	struct buddy *buddy;
	char nname[NUSER_LEN];
	struct buddy_pref *pref = acct->buddy_pref;
	struct bgroup *group = pref->group;

	acct->proto->normalize(nname, screen_name, sizeof(nname));

	buddy = xcalloc(1, sizeof(*buddy));
	buddy->name = xstrdup(screen_name);
	buddy->nname = xstrdup(nname);
	buddy->refnum = group->high_refnum++;
	buddy->id = id;
	pref->num_buddies++;

	if ((id + 1) > group->n_buddies) {
		struct buddy **expand;
		expand = xrealloc(group->buddy_list, group->n_buddies + 10);
		group->buddy_list = expand;
		group->n_buddies += 10;
	}
	group->buddy_list[id] = buddy;
	group->num_members++;

	buddy_came_online(acct, buddy);

	return (buddy);
}

struct bgroup *group_find(struct pork_acct *acct, char *group_name) {
	struct buddy_pref *pref = acct->buddy_pref;

	if (pref->group == NULL)
		return (NULL);

	return (pref->group);
}

struct bgroup *group_add(struct pork_acct *acct, char *group_name) {
	struct bgroup *group;
	struct buddy_pref *pref = acct->buddy_pref;

	group = group_find(acct, group_name);
	if (group != NULL)
		return (group);

	group = xcalloc(1, sizeof(*group));
	group->name = xstrdup(group_name);
	group->refnum = 0; /* only 1 group ever */
	pref->group = group;
	group->n_buddies = 20;
	group->buddy_list = xcalloc(group->n_buddies, sizeof(struct buddy *));
	memset(group->buddy_list, 0, group->n_buddies * sizeof(struct buddy *));

	return (group);
}

struct buddy *
buddy_find(struct pork_acct *acct, const char *screen_name, int number)
{
	struct buddy_pref *pref = acct->buddy_pref;
	struct bgroup *group;
	int i;

	if (pref == NULL)
		return (NULL);

	group = pref->group;
	if (group == NULL)
		return (NULL);

	if (number == -1) {
		for (i = 0; i < group->n_buddies; i++) {
			if (group->buddy_list[i] == NULL)
				continue;

			if (strncasecmp(screen_name, group->buddy_list[i]->name, strlen(screen_name)) == 0) {
				return (group->buddy_list[i]);
			}
		}
	}

	return (group->buddy_list[number]);
}

char *buddy_name(struct pork_acct *acct, char *name) {
	struct buddy *buddy;

	buddy = buddy_find(acct, name, -1);
	if (buddy != NULL)
		return (buddy->name);

	return (name);
}

int buddy_init(struct pork_acct *acct) {
	struct buddy_pref *pref;

	if (acct->buddy_pref != NULL)
		return (-1);

	pref = xcalloc(1, sizeof(*pref));

	pref->user_list_size = 20;
	pref->user_list = xcalloc(pref->user_list_size, sizeof(char *));
	memset(pref->user_list, 0, pref->user_list_size * sizeof(char *));

	acct->buddy_pref = pref;

	return (0);
}

void buddy_destroy(struct pork_acct *acct) {
	struct buddy_pref *pref = acct->buddy_pref;

	if (pref != NULL) {
		if (pref->group != NULL)
			group_cleanup(NULL, pref->group);
		dlist_destroy(pref->block_list, NULL, generic_cleanup);

		pref->block_list = NULL;
		acct->buddy_pref = NULL;
		free(pref->user_list);

		free(pref);
	}
}

int
buddy_update(struct pork_acct *acct,
		struct buddy *buddy,
		void *data)
{
	int ret;

	if (acct->proto->buddy_update == NULL)
		return (-1);

	ret = acct->proto->buddy_update(acct, buddy, data);

	return (ret);
}

static void
buddy_online(struct pork_acct *acct,
		struct buddy *buddy,
		void *data)
{
	buddy_update(acct, buddy, data);
}

static void
buddy_offline(struct pork_acct *acct, struct buddy *buddy)
{
	buddy->status = STATUS_OFFLINE;
	buddy->signon_time = 0;
	buddy->idle_time = 0;
}

static int
buddy_went_offline(struct pork_acct *acct, struct buddy *buddy) {
	char *name;
	int notify = 0;

	if (buddy != NULL) {
		if (buddy->notify)
			notify = 1;

		name = buddy->name;
		buddy_offline(acct, buddy);

		if (opt_get_bool(OPT_SHOW_BUDDY_SIGNOFF)) {
			struct imwindow *win;

			win = imwindow_find(acct, name);
			if (win != NULL) {
				win->typing = 0;
				screen_win_msg(win, 1, 1, 0,
					MSG_TYPE_SIGNOFF, "%s has signed off", name);
			} else if (notify) {
				screen_win_msg(screen.status_win, 1, 1, 0,
					MSG_TYPE_SIGNOFF, "%s has signed off", name);
			}
		}
	}

	return (0);
}

static int
buddy_came_online(struct pork_acct *acct, struct buddy *buddy)
{
	char *name;
	int notify = 0;

	if (buddy != NULL) {
		if (buddy->notify)
			notify = 1;

		name = buddy->name;
		buddy_online(acct, buddy, NULL);

		if (opt_get_bool(OPT_SHOW_BUDDY_SIGNOFF)) {
			struct imwindow *win;

			win = imwindow_find(acct, name);
			if (win != NULL) {
				screen_win_msg(win, 1, 1, 0,
					MSG_TYPE_SIGNON, "%s has signed on", name);
			} else if (notify) {
				screen_win_msg(screen.status_win, 1, 1, 0,
					MSG_TYPE_SIGNON, "%s has signed on", name);
			}
		}
	}

	return (0);
}
