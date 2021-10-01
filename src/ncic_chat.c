/*
** Based on pork_chat.c
** ncic_chat.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <unistd.h>
#include <stdlib.h>
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

	if (acct->proto->chat_user_free != NULL)
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

	chat = xcalloc(1, sizeof(*chat));
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
					char *target,
					char *msg)
{
	if (acct->proto->chat_send == NULL || msg == NULL)
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
	if (acct->proto->chat_send == NULL || msg == NULL)
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
	if (acct->proto->chat_action == NULL || chat == NULL)
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

int chat_recv_action(	struct pork_acct *acct,
						struct chatroom *chat,
						char *dest,
						char *user,
						char *userhost,
						char *msg)
{
	if (!chat_user_is_ignored(acct, chat, user)) {
		char buf[4096];
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_RECV_ACTION, buf,
				sizeof(buf), acct, chat, dest, user, userhost, msg);
		if (ret < 1)
			return (-1);
		screen_print_str(chat->win, buf, (size_t) ret,
			MSG_TYPE_CHAT_ACTION_RECV);
		imwindow_recv_msg(chat->win);
	}

	return (0);
}

int chat_recv_msg(	struct pork_acct *acct,
					struct chatroom *chat,
					char *dest,
					char *user,
					char *userhost,
					char *msg)
{
	if (!chat_user_is_ignored(acct, chat, user)) {
		char buf[4096];
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_RECV, buf, sizeof(buf), acct,
				chat, dest, user, userhost, msg);
		if (ret < 1)
			return (-1);
		screen_print_str(chat->win, buf, (size_t) ret,
			MSG_TYPE_CHAT_MSG_RECV);
		imwindow_recv_msg(chat->win);
	}

	return (0);
}

int chat_recv_notice(	struct pork_acct *acct,
						struct chatroom *chat,
						char *dest,
						char *user,
						char *userhost,
						char *msg)
{
	if (!chat_user_is_ignored(acct, chat, user)) {
		char buf[4096];
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_RECV_NOTICE, buf,
				sizeof(buf), acct, chat, dest, user, userhost, msg);
		if (ret < 1)
			return (-1);
		screen_print_str(chat->win, buf, (size_t) ret,
			MSG_TYPE_CHAT_NOTICE_RECV);
		imwindow_recv_msg(chat->win);
	}

	return (0);
}

int chat_ignore(struct pork_acct *acct, char *chat_name, char *user) {
	struct chat_user *chat_user;
	struct chatroom *chat;
	char buf[4096];
	int ret;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		return (-1);

	chat_user = chat_find_user(acct, chat, user);
	if (chat_user == NULL) {
		screen_err_msg("%s is not a member of %s", user, chat->title_quoted);
		return (-1);
	}

	chat_user->ignore = 1;

	if (acct->proto->chat_ignore != NULL) {
		if (acct->proto->chat_ignore(acct, chat, user) == -1)
			return (-1);
	}

	ret = fill_format_str(OPT_FORMAT_CHAT_IGNORE, buf, sizeof(buf),
			acct, chat, chat->title, acct->username, user, NULL);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (0);
}

int chat_unignore(struct pork_acct *acct, char *chat_name, char *user) {
	struct chat_user *chat_user;
	struct chatroom *chat;
	char buf[4096];
	int ret;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		return (-1);

	chat_user = chat_find_user(acct, chat, user);
	if (chat_user == NULL) {
		screen_err_msg("%s is not a member of %s", user, chat->title_quoted);
		return (-1);
	}

	chat_user->ignore = 0;

	if (acct->proto->chat_unignore != NULL) {
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

int chat_user_is_ignored(	struct pork_acct *acct,
							struct chatroom *chat,
							char *user)
{
	struct chat_user *chat_user;

	chat_user = chat_find_user(acct, chat, user);
	if (chat_user == NULL)
		return (0);

	return (chat_user->ignore);
}

int chat_join(struct pork_acct *acct, char *args) {
	struct imwindow *imwindow = NULL;
	char buf[512];
	char arg_buf[512];
	int ret = 0;

	screen_err_msg("Got here in chat_join");
	if (acct->proto->chat_join == NULL || acct->proto->chat_name == NULL)
		return (-1);

	if (args == NULL) {
		imwindow = cur_window();

		if (imwindow->type == WIN_TYPE_CHAT)
			args = imwindow->target;
		else
			return (-1);
	}

	if (acct->proto->chat_name(args, buf, sizeof(buf),
		arg_buf, sizeof(arg_buf)) == -1)
	{
		screen_err_msg("Invalid chat name: %s", args);
		return (-1);
	}

	imwindow = imwindow_find_chat_target(acct, buf);
	if (imwindow == NULL) {
		imwindow = screen_new_chat_window(acct, buf);
		if (imwindow == NULL) {
			screen_err_msg("Unable to create a new window for %s", buf);
			return (-1);
		}
	}

	ret = acct->proto->chat_join(acct, buf, arg_buf);

	screen_goto_window(imwindow->refnum);
	return (ret);
}

int chat_leave(struct pork_acct *acct, char *chat_name, int close_window) {
	struct chatroom *chat;
	struct imwindow *win;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		return (-1);

	if (acct->proto->chat_leave != NULL) {
		if (acct->proto->chat_leave(acct, chat) == -1) {
			screen_err_msg("Error leaving chat room %s for %s",
				chat->title_quoted, acct->username);
			return (-1);
		}
	}

	win = chat->win;
	win->data = NULL;
	chat->win = NULL;

	if (close_window)
		screen_close_window(win);

	return (chat_free(acct, chat, 0));
}

int chat_leave_all(struct pork_acct *acct) {
	dlist_t *cur;

	cur = acct->chat_list;
	while (cur != NULL) {
		struct chatroom *chat = cur->data;
		dlist_t *next = cur->next;

		chat_leave(acct, chat->title, 0);
		cur = next;
	}

	return (0);
}

int chat_user_kicked(	struct pork_acct *acct,
						struct chatroom *chat,
						char *kicked,
						char *kicker,
						char *reason)
{
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_CHAT_KICK, buf, sizeof(buf), acct,
			chat, chat->title, kicker, kicked, reason);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	chat_user_left(acct, chat, kicked, 1);
	return (0);
}

int chat_forced_leave(	struct pork_acct *acct,
						char *chat_name,
						char *target,
						char *reason)
{
	struct chatroom *chat;

	chat = chat_find(acct, chat_name);
	if (chat == NULL) {
		debug("forced leave of unjoined chat: %s", chat_name);
		return (-1);
	}

	chat_user_kicked(acct, chat, acct->username, target, reason);

	if (opt_get_bool(OPT_AUTO_REJOIN))
		return (chat_rejoin(acct, chat));

	return (chat_free(acct, chat, 1));
}

int chat_invite(struct pork_acct *acct, char *chat_name, char *user, char *msg)
{
	struct chatroom *chat;
	char buf[4096];
	int ret;

	if (acct->proto->chat_invite == NULL)
		return (-1);

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		return (-1);

	if (acct->proto->chat_invite(acct, chat, user, msg) == -1)
		return (-1);

	ret = fill_format_str(OPT_FORMAT_CHAT_INVITE, buf, sizeof(buf), acct,
			chat, chat->title, acct->username, user, msg);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (0);
}

struct chatroom *chat_find(struct pork_acct *acct, char *chat_name) {
	if (acct->proto->chat_find == NULL)
		return (NULL);

	return (acct->proto->chat_find(acct, chat_name));
}

void chat_list(struct pork_acct *acct) {
	dlist_t *cur;

	cur = acct->chat_list;
	if (cur == NULL) {
		screen_cmd_output("%s is not a member of any chat rooms",
			acct->username);
		return;
	}

	screen_cmd_output("%s is joined to the following chat rooms",
		acct->username);

	do {
		struct chatroom *chat = cur->data;

		screen_cmd_output("  %s in window refnum %u",
			chat->title_quoted, chat->win->refnum);

		cur = cur->next;
	} while (cur != NULL);
}

int chat_free(struct pork_acct *acct, struct chatroom *chat, int silent) {
	dlist_t *cur;

	cur = dlist_find(acct->chat_list, chat, NULL);
	if (cur == NULL) {
		debug("tried to free unknown chat: %s", chat->title_quoted);
		return (-1);
	}

	acct->chat_list = dlist_remove(acct->chat_list, cur);

	if (chat->win != NULL) {
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

		chat->win->data = NULL;
		chat->win = NULL;
	}

	if (acct->proto->chat_free != NULL)
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

int chat_got_invite(struct pork_acct *acct,
					char *chat_name,
					char *user,
					char *userhost,
					char *message)
{
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_CHAT_INVITE, buf, sizeof(buf),
		acct, NULL, chat_name, user, acct->username, message);
	if (ret < 1)
		return (-1);
	screen_print_str(cur_window(), buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (0);
}

int chat_created(struct pork_acct *acct, struct chatroom *chat) {
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_CHAT_CREATE, buf, sizeof(buf), acct, chat,
			chat->title, acct->username, NULL, NULL);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (0);
}

struct chat_user *chat_user_joined(	struct pork_acct *acct,
									struct chatroom *chat,
									char *user,
									char *host,
									int silent)
{
	char buf[4096];
	struct chat_user *chat_user;

	if (chat_find_user(acct, chat, user) != NULL) {
		debug("chat user %s already joined to %s", user, chat->title);
		return (NULL);
	}

	chat->num_users++;
	acct->proto->normalize(buf, user, sizeof(buf));

	chat_user = xcalloc(1, sizeof(*chat_user));
	chat_user->name = xstrdup(user);
	chat_user->nname = xstrdup(buf);

	if (host != NULL)
		chat_user->host = xstrdup(host);

	chat->user_list = dlist_add_head(chat->user_list, chat_user);

	if (!silent) {
		int ret;

		ret = fill_format_str(OPT_FORMAT_CHAT_JOIN, buf, sizeof(buf), acct,
			chat, chat->title, user, NULL, NULL);
		if (ret > 0) {
			screen_print_str(chat->win, buf, (size_t) ret,
				MSG_TYPE_CHAT_STATUS);
		}
	}

	return (chat_user);
}

static dlist_t *chat_find_user_node(struct pork_acct *acct,
									struct chatroom *chat,
									char *user)
{
	dlist_t *cur;

	cur = chat->user_list;
	while (cur != NULL) {
		struct chat_user *chat_user = cur->data;

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

	if (cur == NULL)
		return (NULL);

	return (cur->data);
}

int chat_user_left(	struct pork_acct *acct,
					struct chatroom *chat,
					char *user,
					int silent)
{
	dlist_t *node;
	struct chat_user *chat_user = NULL;
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

	if (node != NULL) {
		chat->num_users--;

		chat_user = node->data;
		chat->user_list = dlist_remove(chat->user_list, node);
		chat_destroy_user_list_cb(acct, chat_user);
	} else {
		debug("unknown user %s left %s", user, chat->title_quoted);
	}

	return (ret);
}

int chat_user_quit(	struct pork_acct *acct,
					struct chatroom *chat,
					struct chat_user *user,
					char *msg)
{

	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_CHAT_QUIT, buf, sizeof(buf), acct,
			chat, chat->title, user->name, NULL, msg);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (chat_user_left(acct, chat, user->name, 1));
}

int chat_set_topic(struct pork_acct *acct, struct chatroom *chat, char *topic) {
	int ret = 0;

	if (acct->proto->chat_set_topic == NULL)
		return (-1);

	ret = acct->proto->chat_set_topic(acct, chat, topic);

	return (ret);
}

int chat_got_topic(	struct pork_acct *acct,
					struct chatroom *chat,
					char *set_by,
					char *topic)
{
	free(chat->topic);
	chat->topic = xstrdup(topic);

	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_CHAT_TOPIC, buf, sizeof(buf),
			acct, chat, chat->title, set_by, NULL, topic);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (0);
}

int chat_got_mode(	struct pork_acct *acct,
					struct chatroom *chat,
					char *user,
					char *mode)
{
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_CHAT_MODE, buf, sizeof(buf), acct,
		chat, chat->title, user, NULL, mode);
	if (ret < 1)
		return (-1);
	screen_print_str(chat->win, buf, (size_t) ret,
		MSG_TYPE_CHAT_STATUS);

	return (0);
}

int chat_kick(	struct pork_acct *acct,
				struct chatroom *chat,
				char *user,
				char *reason)
{
	if (acct->proto->chat_kick == NULL)
		return (-1);

	return (acct->proto->chat_kick(acct, chat, user, reason));
}

int chat_ban(	struct pork_acct *acct,
				struct chatroom *chat,
				char *user)
{
	if (acct->proto->chat_ban == NULL)
		return (-1);

	return (acct->proto->chat_ban(acct, chat, user));
}

int chat_rejoin(struct pork_acct *acct, struct chatroom *chat) {
	if (acct->proto->chat_rejoin == NULL)
		return (-1);

	return (acct->proto->chat_rejoin(acct, chat));
}

int chat_rejoin_all(struct pork_acct *acct) {
	dlist_t *cur;

	if (acct->proto->chat_rejoin == NULL)
		return (-1);

	for (cur = acct->chat_list ; cur != NULL ; cur = cur->next)
		chat_rejoin(acct, cur->data);

	return (0);
}

int chat_nick_change(struct pork_acct *acct, char *old, char *new_nick) {
	dlist_t *cur;

	cur = acct->chat_list;
	while (cur != NULL) {
		struct chat_user *user;
		struct chatroom *chat = cur->data;

		user = chat_find_user(acct, chat, old);
		if (user != NULL) {
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

			acct->proto->normalize(buf, new_nick, sizeof(buf));
			user->nname = xstrdup(buf);
		}

		cur = cur->next;
	}

	return (0);
}
