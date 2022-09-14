/*
** Based on pork_msg.c
** ncic_msg.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_acct.h"
#include "ncic_proto.h"
#include "ncic_misc.h"
#include "ncic_set.h"
#include "ncic_format.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_screen_io.h"
#include "ncic_screen.h"
#include "ncic_msg.h"

int ncic_recv_highlight_msg(struct pork_acct *acct, char *msg)
{
  char buf[4096];
  int ret;

  ret = fill_format_str(OPT_FORMAT_HIGHLIGHT, buf, sizeof(buf), acct, msg);
  if (ret < 1)
    return (-1);

  screen_print_str(cur_window(), buf, (size_t) ret, MSG_TYPE_PRIVMSG_RECV);
  imwindow_recv_msg(cur_window());
  return (0);
}

int ncic_recv_sys_alert(struct pork_acct *acct, const char *msg)
{
	int type;
	char buf[4096];
	int ret;

	type = OPT_FORMAT_SYSTEM_ALERT;

	ret = fill_format_str(type, buf, sizeof(buf), acct, msg);
	if (ret < 1)
		return (-1);

	screen_print_str(cur_window(), buf, (size_t) ret, MSG_TYPE_PRIVMSG_RECV);
	imwindow_recv_msg(cur_window());
	return (0);
}

static void autoresp_destroy_cb(void *param __notused, void *data) {
	struct autoresp *autoresp = (struct autoresp *) data;

	free(autoresp->name);
	free(autoresp);
}

static int autoresp_compare_cb(void *l, void *r) {
	char *name = (char *) l;
	struct autoresp *autoresp = (struct autoresp *) r;

	return (strcasecmp(name, autoresp->name));
}

int pork_set_away(struct pork_acct *acct, char *msg) {
	if (msg == nullptr)
		return (pork_set_back(acct));

	if (acct->away_msg != nullptr) {
		free(acct->away_msg);
		hash_destroy(&acct->autoreply);
	}

	acct->away_msg = xstrdup(msg);
	hash_init(&acct->autoreply, 3, autoresp_compare_cb, autoresp_destroy_cb);

	if (acct->proto->set_away != nullptr) {
		if (acct->proto->set_away(acct, msg) == -1) {
			screen_err_msg("An error occurred while setting %s away",
				acct->username);

			return (-1);
		}
	}

	screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_AWAY,
		"%s is now away", acct->username);
	return (0);
}

int pork_set_back(struct pork_acct *acct) {
	if (acct->away_msg == nullptr) {
		screen_err_msg("%s is not away", acct->username);
		return (-1);
	}

	free(acct->away_msg);
	acct->away_msg = nullptr;

	if (acct->proto->set_back != nullptr) {
		if (acct->proto->set_back(acct) == -1) {
			screen_err_msg("An error occurred while setting %s unaway",
				acct->username);

			return (-1);
		}
	}

	hash_destroy(&acct->autoreply);
	memset(&acct->autoreply, 0, sizeof(acct->autoreply));

	screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_BACK,
		"%s is no longer away", acct->username);
	return (0);
}

int pork_msg_send(struct pork_acct *acct, char *dest, char *msg) {
	int ret = 0;


	return (ret);
}


int pork_set_idle_time(struct pork_acct *acct, u_int32_t seconds) {
	char timebuf[32];

	if (acct->proto->set_idle_time == nullptr)
		return (-1);

	acct->proto->set_idle_time(acct, seconds);
	time_to_str_full(seconds, timebuf, sizeof(timebuf));

	screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_CMD_OUTPUT,
		"%s's idle time set to %s", acct->username, timebuf);

	return (0);
}


int pork_change_nick(struct pork_acct *acct, char *nick) {
	if (acct->proto->change_nick != nullptr)
		return (acct->proto->change_nick(acct, nick));

	return (-1);
}

int pork_signoff(struct pork_acct *acct, const char *msg) {
  if (acct == nullptr) {
    return 0;
  }

	if (acct->proto->signoff != nullptr) {
		ncic_recv_sys_alert(acct, ">> You have been disconnected");
		return (acct->proto->signoff(acct, msg));
	}

	return (0);
}
