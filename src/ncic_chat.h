/*
** Based on pork_chat.h
** ncic_chat.h
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_CHAT_H__
#define __NCIC_CHAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CHAT_STATUS_OP		0x01
#define CHAT_STATUS_HALFOP	0x02
#define CHAT_STATUS_VOICE	0x04

struct imwindow;
struct pork_acct;

struct chatroom {
	char *title;
	char *title_quoted;
	char *title_full;
	char *title_full_quoted;
	char *topic;
	void *data;
	char mode[128];
	u_int32_t num_users;
	dlist_t *user_list;
	struct imwindow *win;
};

struct chat_user {
	char *name;
	char *nname;
	char *host;
	u_int32_t status;
	u_int32_t ignore:1;
	void *data;
};

struct chatroom *chat_new(	struct pork_acct *acct,
							char *chat_title,
							char *chat_title_full,
							struct imwindow *win);

int chat_send_msg(struct pork_acct *acct, struct chatroom *chat, char *target, char *msg);
int chat_send_notice(struct pork_acct *acct, struct chatroom *chat, char *target, char *msg);
int chat_ignore(struct pork_acct *acct, char *chat_name, char *user);
int chat_unignore(struct pork_acct *acct, char *chat_name, char *user);
int chat_join(struct pork_acct *acct, char *args);
int chat_leave(struct pork_acct *acct, char *chat_name, int close_window);
int chat_leave_all(struct pork_acct *acct);
int chat_invite(struct pork_acct *acct, char *chat_name, char *user, char *msg);
struct chatroom *chat_find(struct pork_acct *acct, char *chat_name);
void chat_list(struct pork_acct *acct);
int chat_free(struct pork_acct *acct, struct chatroom *chat, int silent);
int chat_rejoin(struct pork_acct *acct, struct chatroom *chat);
int chat_rejoin_all(struct pork_acct *acct);
int chat_nick_change(struct pork_acct *acct, char *old, char *new_nick);

int chat_user_kicked(	struct pork_acct *acct,
						struct chatroom *chat,
						char *kicked,
						char *kicker,
						char *reason);

int chat_user_left(	struct pork_acct *acct,
					struct chatroom *chat,
					char *user,
					int silent);

int chat_kick(	struct pork_acct *acct,
				struct chatroom *chat,
				char *user,
				char *reason);

int chat_ban(	struct pork_acct *acct,
				struct chatroom *chat,
				char *user);

int chat_send_action(	struct pork_acct *acct,
						struct chatroom *chat,
						char *target,
						char *msg);

struct chat_user *chat_find_user(struct pork_acct *acct,
										struct chatroom *chat,
										char *user);

#ifdef __cplusplus
}
#endif
#endif /* __NCIC_CHAT_H__ */
