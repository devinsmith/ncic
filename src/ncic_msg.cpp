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

int pork_msg_autoreply(struct pork_acct *acct, char *dest, char *msg) {
	struct imwindow *win;
	char buf[4096];
	int ret;

	if (acct->proto->send_msg_auto == nullptr)
		return (-1);

	if (acct->proto->send_msg_auto(acct, dest, msg) == -1)
		return (-1);

	screen_get_query_window(acct, dest, &win);
	ret = fill_format_str(OPT_FORMAT_IM_SEND_AUTO, buf, sizeof(buf), acct, dest, msg);
	if (ret < 1)
		return (-1);
	screen_print_str(win, buf, (size_t) ret, MSG_TYPE_PRIVMSG_SEND);
	imwindow_send_msg(win);
	return (0);
}

int pork_msg_send_auto(struct pork_acct *acct, char *sender) {
	dlist_t *node;
	u_int32_t hash_val;
	int ret = 0;

	if (acct->away_msg != nullptr && acct->proto->send_msg_auto == nullptr)
		return (-1);

	hash_val = string_hash(sender, acct->autoreply.order);
	node = hash_find(&acct->autoreply, sender, hash_val);
	if (node == nullptr) {
		struct autoresp *autoresp;

		autoresp = (struct autoresp *)xcalloc(1, sizeof(*autoresp));
		autoresp->name = xstrdup(sender);
		autoresp->last = time(nullptr);

		hash_add(&acct->autoreply, autoresp, hash_val);
		ret = pork_msg_autoreply(acct, sender, acct->away_msg);
	} else {
		time_t time_now = time(nullptr);
		struct autoresp *autoresp = (struct autoresp *)node->data;

		/*
		** Only send someone an auto-reply every 10 minutes.
		** XXX: maybe this should be a configurable value.
		*/
		if (autoresp->last + 600 <= time_now) {
			autoresp->last = time_now;
			ret = pork_msg_autoreply(acct, sender, acct->away_msg);
		}
	}

	return (ret);
}

int pork_recv_msg(	struct pork_acct *acct,
					char *dest,
					char *sender,
					char *userhost,
					char *msg,
					int autoresp)
{
	struct imwindow *win;
	int type;
	char buf[4096];
	int ret;

	screen_get_query_window(acct, sender, &win);
	win->typing = 0;

	if (autoresp)
		type = OPT_FORMAT_IM_RECV_AUTO;
	else {
		if (win == screen.status_win)
			type = OPT_FORMAT_IM_RECV_STATUS;
		else
			type = OPT_FORMAT_IM_RECV;
	}

	ret = fill_format_str(type, buf, sizeof(buf), acct, dest,
			sender, userhost, msg);
	if (ret < 1)
		return (-1);
	screen_print_str(win, buf, (size_t) ret, MSG_TYPE_PRIVMSG_RECV);
	imwindow_recv_msg(win);

	if (acct->away_msg != nullptr && !autoresp &&
		opt_get_bool(OPT_AUTOSEND_AWAY))
	{
		pork_msg_send_auto(acct, sender);
	}

	return (0);
}

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

	if (acct->proto->send_msg != nullptr) {
		ret = acct->proto->send_msg(acct, dest, msg);
		if (ret == -1) {
			screen_err_msg("Error: the last message to %s could not be sent",
				dest);
		} else {
			struct imwindow *win;
			char buf[4096];
			int type;
			int ret;

			if (acct->away_msg != nullptr) {
				if (opt_get_bool(OPT_SEND_REMOVES_AWAY))
					pork_set_back(acct);
			}

			if (screen_get_query_window(acct, dest, &win) != 0)
				screen_goto_window(win->refnum);

			if (win == screen.status_win)
				type = OPT_FORMAT_IM_SEND_STATUS;
			else
				type = OPT_FORMAT_IM_SEND;

			ret = fill_format_str(type, buf, sizeof(buf), acct, dest, msg);
			if (ret < 1)
				return (-1);
			screen_print_str(win, buf, (size_t) ret, MSG_TYPE_PRIVMSG_SEND);
			imwindow_send_msg(win);
		}
	}

	return (ret);
}

int pork_set_profile(struct pork_acct *acct, char *profile) {
	int ret = 0;

	free(acct->profile);
	if (profile == nullptr)
		acct->profile = nullptr;
	else
		acct->profile = xstrdup(profile);

	if (acct->proto->set_profile != nullptr)
		ret = acct->proto->set_profile(acct, profile);

	if (ret == 0) {
		screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_CMD_OUTPUT,
			"Profile for %s was %s", acct->username,
			(profile == nullptr ? "cleared" : "set"));
	}

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

int pork_send_warn(struct pork_acct *acct, char *user) {
	int ret = 0;

	if (acct->proto->warn == nullptr)
		return (-1);

	ret = acct->proto->warn(acct, user);
	if (ret == 0) {
		struct imwindow *win;

		win = imwindow_find(acct, user);
		if (win == nullptr)
			win = cur_window();

		screen_win_msg(win, 1, 1, 0,
			MSG_TYPE_CMD_OUTPUT, "%s has warned %s", acct->username, user);
	}

	return (ret);
}

int pork_send_warn_anon(struct pork_acct *acct, char *user) {
	int ret = 0;

	if (acct->proto->warn_anon == nullptr)
		return (-1);

	ret = acct->proto->warn_anon(acct, user);
	if (ret == 0) {
		struct imwindow *win;

		win = imwindow_find(acct, user);
		if (win == nullptr)
			win = cur_window();

		screen_win_msg(win, 0, 0, 1,
			MSG_TYPE_CMD_OUTPUT, "%s has warned %s anonymously",
			acct->username, user);
	}

	return (ret);
}

int pork_change_nick(struct pork_acct *acct, char *nick) {
	if (acct->proto->change_nick != nullptr)
		return (acct->proto->change_nick(acct, nick));

	return (-1);
}

int pork_recv_action(	struct pork_acct *acct,
						char *dest,
						char *sender,
						char *userhost,
						char *msg)
{
	struct imwindow *win;
	char buf[4096];
	int type;
	int ret;

	screen_get_query_window(acct, sender, &win);

	if (win == screen.status_win)
		type = OPT_FORMAT_ACTION_RECV_STATUS;
	else
		type = OPT_FORMAT_ACTION_RECV;

	ret = fill_format_str(type, buf, sizeof(buf), acct,
			dest, sender, userhost, msg);
	if (ret < 1)
		return (-1);
	screen_print_str(win, buf, (size_t) ret, MSG_TYPE_PRIVMSG_RECV);
	imwindow_recv_msg(win);

	return (0);
}

int pork_action_send(struct pork_acct *acct, char *dest, char *msg) {
	if (acct->proto->send_action == nullptr || dest == nullptr)
		return (-1);

	if (acct->proto->send_action(acct, dest, msg) != -1) {
		char buf[4096];
		struct imwindow *win;
		int type;
		int ret;

		screen_get_query_window(acct, dest, &win);

		if (win == screen.status_win)
			type = OPT_FORMAT_ACTION_SEND_STATUS;
		else
			type = OPT_FORMAT_ACTION_SEND;

		ret = fill_format_str(type, buf, sizeof(buf), acct, dest, msg);
		if (ret < 1)
			return (-1);
		screen_print_str(win, buf, (size_t) ret, MSG_TYPE_PRIVMSG_SEND);
		imwindow_send_msg(win);
	}

	return (0);
}

int pork_notice_send(struct pork_acct *acct, char *dest, char *msg) {
	struct imwindow *win;

	if (acct->proto->send_notice == nullptr)
		return (-1);

	win = imwindow_find(acct, dest);
	if (win == nullptr)
		win = cur_window();

	char buf[4096];
	int type;
	int ret;

	if (win == screen.status_win)
		type = OPT_FORMAT_NOTICE_SEND_STATUS;
	else
		type = OPT_FORMAT_NOTICE_SEND;

	ret = fill_format_str(type, buf, sizeof(buf), acct, dest, msg);
	if (ret < 1)
		return (-1);
	screen_print_str(win, buf, (size_t) ret, MSG_TYPE_NOTICE_SEND);
	imwindow_send_msg(win);

	return (0);
}

int pork_recv_notice(	struct pork_acct *acct,
						char *dest,
						char *sender,
						char *userhost,
						char *msg)
{
	struct imwindow *win;
	int type;

	win = imwindow_find(acct, sender);
	if (win == nullptr) {
		win = screen.status_win;
		type = OPT_FORMAT_NOTICE_RECV_STATUS;
	} else
		type = OPT_FORMAT_NOTICE_RECV;

	char buf[4096];
	int ret;

	ret = fill_format_str(type, buf, sizeof(buf), acct,
			dest, sender, userhost, msg);
	if (ret < 1)
		return (-1);
	screen_print_str(win, buf, (size_t) ret, MSG_TYPE_NOTICE_RECV);
	imwindow_recv_msg(win);

	return (0);
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
