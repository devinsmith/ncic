/*
** Based on pork_chat.c
** ncic_chat.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <cstdlib>
#include <ncurses.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_acct.h"
#include "ncic_set.h"
#include "ncic_proto.h"
#include "ncic_format.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_chat.h"

static void chat_destroy_user_list_cb(void *param, void *data) {
	struct pork_acct *acct = (struct pork_acct *) param;
	struct chat_user *chat_user = (struct chat_user *) data;

	if (acct->proto->chat_user_free != nullptr)
		acct->proto->chat_user_free(acct, chat_user);

	free(chat_user->host);
	free(chat_user->nname);
	free(chat_user->name);
	free(chat_user);
}

struct chatroom *chat_new(	struct pork_acct *acct,
                            char *chat_title,
							char *chat_title_full,
							struct imwindow *win)
{
	struct chatroom *chat;

	chat = (struct chatroom *)xcalloc(1, sizeof(*chat));
	chat->title = xstrdup(chat_title);
	chat->title_quoted = acct->proto->filter_text(chat_title);
	chat->title_full = xstrdup(chat_title_full);
	chat->title_full_quoted = acct->proto->filter_text(chat_title_full);
	chat->win = win;
	win->data = chat;

	acct->chat_list = dlist_add_head(acct->chat_list, chat);

	return (chat);
}

int chat_send_msg(	struct pork_acct *acct,
					struct chatroom *chat,
					const char *target,
					char *msg)
{
	if (acct->proto->chat_send == nullptr || msg == nullptr)
		return (-1);

	if (acct->proto->chat_send(acct, chat, target, msg) != -1) {
		char buf[4096];
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_SEND, buf, sizeof(buf),
				acct, chat, target, msg);
		if (ret < 1)
			return (-1);
//		screen_print_str(chat->win, buf, (size_t) ret,
//			MSG_TYPE_CHAT_MSG_SEND);
//		imwindow_send_msg(chat->win);
	}

	return (0);
}

int chat_send_notice(	struct pork_acct *acct,
						struct chatroom *chat,
						char *target,
						char *msg)
{
	if (acct->proto->chat_send == nullptr || msg == nullptr)
		return (-1);

	if (acct->proto->chat_send_notice(acct, chat, target, msg) != -1) {
		char buf[4096];
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_SEND_NOTICE, buf, sizeof(buf),
			acct, chat, target, msg);
		if (ret < 1)
			return (-1);
		screen_print_str(chat->win, buf, (size_t) ret,
			MSG_TYPE_CHAT_NOTICE_SEND);
		imwindow_send_msg(chat->win);
	}

	return (0);
}

int chat_send_action(	struct pork_acct *acct,
						struct chatroom *chat,
						char *target,
						char *msg)
{
	if (acct->proto->chat_action == nullptr || chat == nullptr)
		return (-1);

	if (acct->proto->chat_action(acct, chat, target, msg) != -1) {
		char buf[4096];
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_SEND_ACTION, buf, sizeof(buf),
				acct, chat, target, msg);
		if (ret < 1)
			return (-1);
		screen_print_str(chat->win, buf, (size_t) ret,
			MSG_TYPE_CHAT_ACTION_SEND);
		imwindow_send_msg(chat->win);
	} else
		return (-1);

	return (0);
}

int chat_unignore(struct pork_acct *acct, char *chat_name, char *user) {
	struct chat_user *chat_user;
	struct chatroom *chat;
	char buf[4096];
	int ret;

	chat = chat_find(acct, chat_name);
	if (chat == nullptr)
		return (-1);

	chat_user = chat_find_user(acct, chat, user);
	if (chat_user == nullptr) {
		screen_err_msg("%s is not a member of %s", user, chat->title_quoted);
		return (-1);
	}

	chat_user->ignore = 0;

	if (acct->proto->chat_unignore != nullptr) {
		if (acct->proto->chat_unignore(acct, chat, user) == -1)
			return (-1);
	}

	ret = fill_format_str(OPT_FORMAT_CHAT_UNIGNORE, buf, sizeof(buf), acct,
			chat, chat->title, acct->username, user, NULL);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (0);
}

int chat_leave(struct pork_acct *acct, char *chat_name, int close_window) {
	struct chatroom *chat;
	struct imwindow *win;

	chat = chat_find(acct, chat_name);
	if (chat == nullptr)
		return (-1);

	if (acct->proto->chat_leave != nullptr) {
		if (acct->proto->chat_leave(acct, chat) == -1) {
			screen_err_msg("Error leaving chat room %s for %s",
				chat->title_quoted, acct->username);
			return (-1);
		}
	}

	win = chat->win;
	win->data = nullptr;
	chat->win = nullptr;

	if (close_window)
		screen_close_window(win);

	return (chat_free(acct, chat, 0));
}

int chat_leave_all(struct pork_acct *acct) {
	dlist_t *cur;

	cur = acct->chat_list;
	while (cur != nullptr) {
		struct chatroom *chat = (struct chatroom *)cur->data;
		dlist_t *next = cur->next;

		chat_leave(acct, chat->title, 0);
		cur = next;
	}

	return (0);
}

struct chatroom *chat_find(struct pork_acct *acct, char *chat_name) {
	if (acct->proto->chat_find == nullptr)
		return (nullptr);

	return (acct->proto->chat_find(acct, chat_name));
}

void chat_list(struct pork_acct *acct) {
	dlist_t *cur;

	cur = acct->chat_list;
	if (cur == nullptr) {
		screen_cmd_output("%s is not a member of any chat rooms",
			acct->username);
		return;
	}

	screen_cmd_output("%s is joined to the following chat rooms",
		acct->username);

	do {
		struct chatroom *chat = (struct chatroom *)cur->data;

		screen_cmd_output("  %s in window refnum %u",
			chat->title_quoted, chat->win->refnum);

		cur = cur->next;
	} while (cur != nullptr);
}

int chat_free(struct pork_acct *acct, struct chatroom *chat, int silent) {
	dlist_t *cur;

	cur = dlist_find(acct->chat_list, chat, nullptr);
	if (cur == nullptr) {
		debug("tried to free unknown chat: %s", chat->title_quoted);
		return (-1);
	}

	acct->chat_list = dlist_remove(acct->chat_list, cur);

	if (chat->win != nullptr) {
		if (!silent) {
			char buf[4096];
			int ret;

			ret = fill_format_str(OPT_FORMAT_CHAT_LEAVE, buf, sizeof(buf), acct,
					chat, chat->title, acct->username, NULL, NULL);
			if (ret < 1)
				return (-1);
			screen_print_str(chat->win, buf, (size_t) ret,
				MSG_TYPE_CHAT_STATUS);
		}

		chat->win->data = nullptr;
		chat->win = nullptr;
	}

	if (acct->proto->chat_free != nullptr)
		acct->proto->chat_free(acct, chat->data);

	dlist_destroy(chat->user_list, acct, chat_destroy_user_list_cb);

	free(chat->title);
	free(chat->title_quoted);
	free(chat->title_full);
	free(chat->title_full_quoted);
	free(chat->topic);
	free(chat);

	return (0);
}


static dlist_t *chat_find_user_node(struct pork_acct *acct,
									struct chatroom *chat,
									char *user)
{
	dlist_t *cur;

	cur = chat->user_list;
	while (cur != nullptr) {
		struct chat_user *chat_user = (struct chat_user *)cur->data;

		if (!acct->proto->user_compare(user, chat_user->nname))
			break;

		cur = cur->next;
	}

	return (cur);
}

struct chat_user *chat_find_user(struct pork_acct *acct,
										struct chatroom *chat,
										char *user)
{
	dlist_t *cur = chat_find_user_node(acct, chat, user);

	if (cur == nullptr)
		return (nullptr);

	return (struct chat_user *)(cur->data);
}

int chat_user_left(	struct pork_acct *acct,
					struct chatroom *chat,
					char *user,
					int silent)
{
	dlist_t *node;
	struct chat_user *chat_user = nullptr;
	int ret = 0;

	node = chat_find_user_node(acct, chat, user);

	if (!silent) {
		char buf[4096];
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_LEAVE, buf, sizeof(buf), acct,
				chat, chat->title, user, NULL, NULL);
		if (ret < 1)
			return (-1);
		screen_print_str(chat->win, buf, (size_t) ret,
			MSG_TYPE_CHAT_STATUS);
	}

	if (node != nullptr) {
		chat->num_users--;

		chat_user = (struct chat_user *)node->data;
		chat->user_list = dlist_remove(chat->user_list, node);
		chat_destroy_user_list_cb(acct, chat_user);
	} else {
		debug("unknown user %s left %s", user, chat->title_quoted);
	}

	return (ret);
}

int chat_rejoin(struct pork_acct *acct, struct chatroom *chat) {
	if (acct->proto->chat_rejoin == nullptr)
		return (-1);

	return (acct->proto->chat_rejoin(acct, chat));
}

int chat_rejoin_all(struct pork_acct *acct) {
	dlist_t *cur;

	if (acct->proto->chat_rejoin == nullptr)
		return (-1);

	for (cur = acct->chat_list ; cur != nullptr ; cur = cur->next)
		chat_rejoin(acct, (struct chatroom *)cur->data);

	return (0);
}

int chat_nick_change(struct pork_acct *acct, char *old, char *new_nick) {
	dlist_t *cur;

	cur = acct->chat_list;
	while (cur != nullptr) {
		struct chat_user *user;
		struct chatroom *chat = (struct chatroom *)cur->data;

		user = chat_find_user(acct, chat, old);
		if (user != nullptr) {
			char buf[4096];
			int ret;

			ret = fill_format_str(OPT_FORMAT_CHAT_NICK, buf, sizeof(buf),
					acct, chat, chat->title, old, new_nick, NULL);
			if (ret > 0) {
				screen_print_str(chat->win, buf, (size_t) ret,
					MSG_TYPE_CHAT_STATUS);
			}

			free(user->name);
			free(user->nname);

			user->name = xstrdup(new_nick);

      xstrncpy(buf, new_nick, sizeof(buf));
			user->nname = xstrdup(buf);
		}

		cur = cur->next;
	}

	return (0);
}
