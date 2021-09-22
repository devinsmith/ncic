/*
** Based on pork_acct.c
** ncic_acct.c - account management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_buddy.h"
#include "ncic_proto.h"
#include "ncic_acct.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_msg.h"
#include "ncic_chat.h"

extern struct sockaddr_storage local_addr;
extern in_port_t local_port;

static int pork_acct_find_helper(void *l, void *r) {
	u_int32_t refnum = POINTER_TO_UINT(l);
	struct pork_acct *acct = r;

	return (refnum - acct->refnum);
}

/*
** Free everything that needs freeing.
*/

static void pork_acct_free(struct pork_acct *acct) {
	/* XXX - really? */
	if (acct->proto->write_config != NULL)
		acct->proto->write_config(acct);

	if (acct->proto->free != NULL)
		acct->proto->free(acct);

	if (acct->blist != NULL) {
		free(acct->blist);
	}

	buddy_destroy(acct);

	hash_destroy(&acct->autoreply);

	free_str_wipe(acct->passwd);
	free(acct->away_msg);
	free(acct->username);
	free(acct->profile);
	free(acct->server);
	free(acct->fport);
	free(acct);
}

dlist_t *pork_acct_find(u_int32_t refnum) {
	dlist_t *ret;

	ret = dlist_find(screen.acct_list,
			UINT_TO_POINTER(refnum), pork_acct_find_helper);

	return (ret);
}

struct pork_acct *pork_acct_find_name(const char *name, int protocol) {
	dlist_t *cur = screen.acct_list;

	while (cur != NULL) {
		struct pork_acct *acct = cur->data;

		if (acct->proto->protocol == protocol &&
			!strcasecmp(name, acct->username))
		{
			return (cur->data);
		}

		cur = cur->next;
	}

	return (NULL);
}

struct pork_acct *pork_acct_get_data(u_int32_t refnum) {
	dlist_t *node;

	node = pork_acct_find(refnum);
	if (node == NULL || node->data == NULL)
		return (NULL);

	return (node->data);
}

int pork_acct_add(struct pork_acct *acct) {
	screen.acct_list = dlist_add_head(screen.acct_list, acct);
	return (0);
}

int pork_acct_del_refnum(u_int32_t refnum, char *reason) {
	dlist_t *node;

	node = pork_acct_find(refnum);
	if (node == NULL)
		return (-1);

	pork_acct_del(node, reason);
	return (0);
}

void pork_acct_del(dlist_t *node, char *reason) {
	struct pork_acct *acct = node->data;
	dlist_t *cur;

	pork_signoff(acct, reason);
	chat_leave_all(acct);

	screen.acct_list = dlist_remove(screen.acct_list, node);

	cur = screen.window_list;
	if (cur != NULL) {
		do {
			struct imwindow *win = cur->data;

			if (win->owner == acct) {

				win->owner->ref_count--;

				if (win->owner == screen.null_acct)
					win->owner = NULL;
				else {
					win->owner = screen.null_acct;
					win->owner->ref_count++;
				}
			}

			cur = cur->next;
		} while (cur != screen.window_list);
	}

	/* This must always be the case. */
	if (acct->ref_count == 0)
		pork_acct_free(acct);
	else {
		debug("ref count for %s is not 0 [%u]",
			acct->username, acct->ref_count);
	}
}

int pork_acct_next_refnum(u_int32_t cur_refnum, u_int32_t *next) {
	dlist_t *cur;
	struct pork_acct *acct = NULL;

	cur = pork_acct_find(cur_refnum);
	if (cur == NULL) {
		debug("current refnum %u doesn't exist", cur_refnum);
		return (-1);
	}

	do {
		if (cur->next == NULL) {
			if (cur == screen.acct_list)
				return (-1);

			cur = screen.acct_list;
		} else
			cur = cur->next;

		acct = cur->data;
	} while (acct->proto->protocol < 0);

	*next = acct->refnum;
	return (0);
}

void pork_acct_print_list(void) {
	dlist_t *cur = screen.acct_list;

	screen_cmd_output("REFNUM\tUSERNAME\t\tPROTOCOL\tSERVER\t\t\t\t\t\tSTATUS");
	for (cur = screen.acct_list ; cur != NULL ; cur = cur->next) {
		char buf[128];
		struct pork_acct *acct = cur->data;
		u_int32_t max_reconnect_tries = 0;
		char server_buf[512];

		if (acct->proto->protocol < 0)
			continue;

		if (!acct->connected)
			max_reconnect_tries = opt_get_int(OPT_RECONNECT_TRIES);

		if (acct->reconnecting) {
			snprintf(buf, sizeof(buf), "reconnecting (attempt %u/%u)",
				acct->reconnect_tries, max_reconnect_tries);
		} else if (acct->disconnected) {
			snprintf(buf, sizeof(buf),
				"disconnected: reconnect attempt %u/%u in %ld seconds",
				acct->reconnect_tries + 1, max_reconnect_tries,
				(long int) max(0, acct->reconnect_next_try - time(NULL)));
		} else
			xstrncpy(buf, "connected", sizeof(buf));

		if (acct->server != NULL) {
			if (acct->fport != NULL) {
				snprintf(server_buf, sizeof(server_buf), "%s:%s",
					acct->server, acct->fport);
			} else
				xstrncpy(server_buf, acct->server, sizeof(server_buf));
		} else {
			server_buf[0] = '\0';
		}

		screen_cmd_output("%u\t\t%s\t\t\t%s\t\t\t%s\t\t\t%s",
			acct->refnum, acct->username, acct->proto->name, server_buf, buf);
	}
}

void pork_acct_del_all(char *reason) {
	dlist_t *cur = screen.acct_list;

	while (cur != NULL) {
		dlist_t *next = cur->next;

		pork_acct_del(cur, reason);
		cur = next;
	}
}

int pork_acct_connect(const char *user, char *args, int protocol) {
	struct pork_acct *acct;

	if (user == NULL)
		return (-1);

	acct = pork_acct_init(user, protocol);
	if (acct == NULL) {
		screen_err_msg("%s is already connected", user);
		return (-1);
	}

	pork_acct_add(acct);

	if (!acct->can_connect || acct->proto->connect == NULL) {
		screen_err_msg("You must specify a screen name before connecting");
		pork_acct_del_refnum(acct->refnum, NULL);
		return (-1);
	}

	screen_bind_all_unbound(acct);

	if (acct->proto->connect(acct, args) == -1) {
		screen_err_msg("Unable to login as %s", acct->username);
		pork_acct_del_refnum(acct->refnum, NULL);
		return (-1);
	}

	return (0);
}

void pork_acct_update(void) {
	dlist_t *cur = screen.acct_list;

	for (cur = screen.acct_list ; cur != NULL ; cur = cur->next) {
		struct pork_acct *acct = cur->data;
		time_t time_now = time(NULL);

		if (acct->proto->update != NULL) {
			if (acct->proto->update(acct) == -1)
				continue;
		}

		if (acct->proto->set_idle_time != NULL && acct->report_idle &&
			!acct->marked_idle && opt_get_bool(OPT_REPORT_IDLE))
		{
			time_t time_diff = time_now - acct->last_input;
			int idle_after = opt_get_int(OPT_IDLE_AFTER);

			if (idle_after > 0 && time_diff >= 60 * idle_after) {
				screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_IDLE,
					"Setting %s idle after %d minutes of inactivity",
					acct->username, (int) time_diff / 60);
				acct->proto->set_idle_time(acct, time_diff);
			}
		}
	}
}

static inline u_int32_t pork_acct_get_new_refnum(void) {
	u_int32_t i;

	for (i = 0 ; i < 0xffffffff ; i++) {
		if (pork_acct_find(i) == NULL)
			return (i);
	}

	return (0);
}

/*
** Initialize an account for the specified user.
*/

struct pork_acct *pork_acct_init(const char *user, int protocol) {
	struct pork_acct *acct;

	acct = xcalloc(1, sizeof(*acct));
	acct->username = xstrdup(user);
	acct->state = STATE_DISCONNECTED;

	acct->proto = proto_get(protocol);

	if (protocol < 0)
		return (acct);

	if (buddy_init(acct) == -1)
		goto out_fail;

	if (acct->proto->init != NULL && acct->proto->init(acct) == -1)
		goto out_fail2;

	if (acct->proto->read_config != NULL &&
		acct->proto->read_config(acct) == -1)
	{
		goto out_fail2;
	}

	acct->can_connect = 1;
	acct->refnum = pork_acct_get_new_refnum();

	memcpy(&acct->laddr, &local_addr, sizeof(acct->laddr));
	acct->lport = local_port;

	time(&acct->last_input);
	return (acct);

out_fail:
	buddy_destroy(acct);

out_fail2:
	free(acct->username);
	free(acct);
	return (NULL);
}

void pork_acct_connected(struct pork_acct *acct) {
	acct->successful_connect = 1;
	acct->connected = 1;

	if (acct->reconnecting)
		chat_rejoin_all(acct);

	acct->disconnected = 0;
	acct->reconnecting = 0;
	acct->reconnect_tries = 0;
}

static int pork_acct_reconnect(struct pork_acct *acct) {
	if (acct->proto->reconnect == NULL || acct->connected ||
		!acct->disconnected || acct->reconnecting)
	{
		debug("%p %u %u %u", acct->proto->reconnect, acct->connected,
			!acct->disconnected, acct->reconnecting);
		return (-1);
	}

	acct->reconnect_tries++;
	acct->reconnecting = 1;

	screen_err_msg("Automatically reconnecting account %s (attempt %u)",
		acct->username, acct->reconnect_tries);

	return (acct->proto->reconnect(acct, NULL));
}

static int pork_acct_connect_fail(struct pork_acct *acct) {
	u_int32_t max_reconnect_tries;
	u_int32_t connect_interval;
	u_int32_t connect_interval_max;

	max_reconnect_tries = opt_get_int(OPT_RECONNECT_TRIES);
	connect_interval = opt_get_int(OPT_RECONNECT_INTERVAL);
	connect_interval_max = opt_get_int(OPT_RECONNECT_MAX_INTERVAL);

	acct->connected = 0;
	acct->reconnecting = 0;
	acct->disconnected = 1;

	if (acct->reconnect_tries >= max_reconnect_tries) {
		screen_err_msg("Failed to reconnect %s after %u tries. Giving up.",
			acct->username, max_reconnect_tries);

		pork_acct_del_refnum(acct->refnum, NULL);
		return (-1);
	}

	acct->reconnect_next_try = time(NULL) +
		min(acct->reconnect_tries * connect_interval, connect_interval_max);
	return (0);
}

int pork_acct_disconnected(struct pork_acct *acct) {
	if (!acct->successful_connect || !opt_get_bool(OPT_AUTO_RECONNECT)) {
		pork_acct_del_refnum(acct->refnum, NULL);
		return (0);
	}

	if (!acct->connected)
		return (pork_acct_connect_fail(acct));

	screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_SIGNOFF,
		"%s has been disconnected", acct->username);

	acct->connected = 0;
	acct->reconnecting = 0;
	acct->reconnect_tries = 0;
	acct->disconnected = 1;
	acct->reconnect_next_try = time(NULL);

	if (acct->proto->disconnected != NULL)
		acct->proto->disconnected(acct);

	return (pork_acct_reconnect(acct));
}

void pork_acct_reconnect_all(void) {
	dlist_t *cur;
	time_t now = time(NULL);
	int timeout = opt_get_int(OPT_CONNECT_TIMEOUT);

	cur = screen.acct_list;
	while (cur != NULL) {
		struct pork_acct *acct = cur->data;
		dlist_t *next = cur->next;

		if (acct->disconnected && !acct->reconnecting &&
			acct->reconnect_next_try < now)
		{
			pork_acct_reconnect(acct);
		} else if (	acct->reconnecting &&
					acct->reconnect_next_try + timeout < now)
		{
			screen_err_msg(
				"Attempt %u to reconnect %s timed out after %u seconds",
				acct->reconnect_tries, acct->username, timeout);

			acct->proto->connect_abort(acct);
			pork_acct_connect_fail(acct);
		}

		cur = next;
	}
}

int pork_acct_save(struct pork_acct *acct) {
	return (0);
}
