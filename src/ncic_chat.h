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

int chat_recv_msg(	struct pork_acct *acct,
					struct chatroom *chat,
					char *dest,
					char *user,
					char *userhost,
					char *msg);

int chat_recv_notice(	struct pork_acct *acct,
						struct chatroom *chat,
						char *dest,
						char *user,
						char *userhost,
						char *msg);

int chat_send_msg(struct pork_acct *acct, struct chatroom *chat, char *target, char *msg);
int chat_send_notice(struct pork_acct *acct, struct chatroom *chat, char *target, char *msg);
int chat_ignore(struct pork_acct *acct, char *chat_name, char *user);
int chat_unignore(struct pork_acct *acct, char *chat_name, char *user);
int chat_users(struct pork_acct *acct, char *chat_name);
int chat_who(struct pork_acct *acct, char *chat_name);
int chat_join(struct pork_acct *acct, char *args);
int chat_leave(struct pork_acct *acct, char *chat_name, int close_window);
int chat_leave_all(struct pork_acct *acct);
int chat_invite(struct pork_acct *acct, char *chat_name, char *user, char *msg);
int chat_created(struct pork_acct *acct, struct chatroom *chat);
struct chatroom *chat_find(struct pork_acct *acct, char *chat_name);
void chat_list(struct pork_acct *acct);
int chat_free(struct pork_acct *acct, struct chatroom *chat, int silent);
int chat_set_topic(struct pork_acct *acct, struct chatroom *chat, char *topic);
int chat_rejoin(struct pork_acct *acct, struct chatroom *chat);
int chat_rejoin_all(struct pork_acct *acct);
int chat_nick_change(struct pork_acct *acct, char *old, char *new_nick);

int chat_user_is_ignored(	struct pork_acct *acct,
							struct chatroom *chat,
							char *user);

int chat_forced_leave(	struct pork_acct *acct,
						char *chat_name,
						char *person,
						char *reason);

int chat_user_kicked(	struct pork_acct *acct,
						struct chatroom *chat,
						char *kicked,
						char *kicker,
						char *reason);

int chat_got_invite(struct pork_acct *acct,
					char *chat_name,
					char *user,
					char *userhost,
					char *message);

struct chat_user *chat_user_joined(	struct pork_acct *acct,
									struct chatroom *chat,
									char *user,
									char *host,
									int silent);

int chat_user_left(	struct pork_acct *acct,
					struct chatroom *chat,
					char *user,
					int silent);

int chat_got_topic(	struct pork_acct *acct,
					struct chatroom *chat,
					char *set_by,
					char *topic);

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

int chat_recv_action(	struct pork_acct *acct,
						struct chatroom *chat,
						char *dest,
						char *user,
						char *userhost,
						char *msg);

int chat_user_quit(	struct pork_acct *acct,
					struct chatroom *chat,
					struct chat_user *user,
					char *msg);


int chat_got_mode(	struct pork_acct *acct,
					struct chatroom *chat,
					char *user,
					char *mode);

inline struct chat_user *chat_find_user(struct pork_acct *acct,
										struct chatroom *chat,
										char *user);
#endif /* __NCIC_CHAT_H__ */
