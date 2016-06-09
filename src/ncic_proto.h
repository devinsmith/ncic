/*
** Based on pork_proto.h
** ncic_proto.h
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_PROTO_H__
#define __NCIC_PROTO_H__

struct pork_acct;
struct imwindow;
struct file_transfer;
struct chatroom;
struct chat_user;
struct buddy;

enum {
	PROTO_NULL = -1,
//	PROTO_AIM,
	PROTO_IRC,
	PROTO_MAX
};

struct pork_proto {
	int protocol;
	char name[32];

	struct command *cmd;
	size_t num_cmds;

	int (*init)(struct pork_acct *);
	int (*free)(struct pork_acct *);
	int (*read_config)(struct pork_acct *);
	int (*write_config)(struct pork_acct *);
	int (*update)(struct pork_acct *);
	int (*normalize)(char *dest, const char *str, size_t len);
	int (*user_compare)(const char *u1, const char *u2);
	char *(*filter_text)(char *);
	char *(*filter_text_out)(char *);

	int (*connect)(struct pork_acct *, char *);
	int (*connect_abort)(struct pork_acct *acct);
	int (*reconnect)(struct pork_acct *, char *);
	int (*disconnected)(struct pork_acct *);
	int (*signoff)(struct pork_acct *acct, char *reason);

	int (*buddy_block)(struct pork_acct *, char *);
	int (*buddy_permit)(struct pork_acct *, char *);
	int (*buddy_unblock)(struct pork_acct *, char *);
	int (*buddy_add)(struct pork_acct *, struct buddy *);
	int (*buddy_update)(struct pork_acct *, struct buddy *, void *);

	struct chatroom *(*chat_find)(struct pork_acct *, char *);
	int (*is_chat)(struct pork_acct *acct, char *str);
	int (*chat_free)(struct pork_acct *, void *chat_data);
	int (*chat_ignore)(struct pork_acct *, struct chatroom *, char *user);
	int (*chat_unignore)(struct pork_acct *, struct chatroom *, char *user);
	int (*chat_join)(struct pork_acct *, char *name, char *args);
	int (*chat_rejoin)(struct pork_acct *, struct chatroom *);
	int (*chat_leave)(struct pork_acct *, struct chatroom *);
	int (*chat_invite)(struct pork_acct *, struct chatroom *, char *, char *);
	int (*chat_kick)(struct pork_acct *, struct chatroom *, char *user, char *);
	int (*chat_ban)(struct pork_acct *, struct chatroom *, char *user);
	int (*chat_name)(const char *name, char *buf, size_t len, char *, size_t);
	int (*chat_send)(struct pork_acct *, struct chatroom *chat, char *, char *);
	int (*chat_action)(struct pork_acct *, struct chatroom *, char *, char *msg);
	int (*chat_send_notice)(struct pork_acct *, struct chatroom *, char *, char *);
	int (*chat_user_free)(struct pork_acct *acct, struct chat_user *);
	int (*chat_users)(struct pork_acct *, struct chatroom *chat);
	int (*chat_who)(struct pork_acct *, struct chatroom *chat);
	int (*chat_set_topic)(struct pork_acct *, struct chatroom *chat, char *topic);

	int (*set_report_idle)(struct pork_acct *, int);
	int (*set_idle_time)(struct pork_acct *, u_int32_t);
	int (*set_privacy_mode)(struct pork_acct *, int);

	int (*warn)(struct pork_acct *, char *);
	int (*warn_anon)(struct pork_acct *, char *);

	int (*send_action)(struct pork_acct *, char *target, char *msg);
	int (*send_msg)(struct pork_acct *, char *target, char *msg);
	int (*send_notice)(struct pork_acct *, char *target, char *msg);
	int (*send_msg_auto)(struct pork_acct *, char *target, char *msg);
	int (*set_back)(struct pork_acct *);
	int (*set_away)(struct pork_acct *, char *);
	int (*get_away_msg)(struct pork_acct *, char *);
	int (*set_profile)(struct pork_acct *, char *);
	int (*get_profile)(struct pork_acct *, char *);

	int (*whowas)(struct pork_acct *, char *);
	int (*quote)(struct pork_acct *acct, char *str);
	int (*mode)(struct pork_acct *, char *);
	int (*who)(struct pork_acct *, char *str);
	int (*ping)(struct pork_acct *, char *str);
	int (*ctcp)(struct pork_acct *, char *dest, char *str);

	int (*keepalive)(struct pork_acct *);
	int (*change_nick)(struct pork_acct *acct, char *nick);
};

int proto_init(void);
int proto_get_num(const char *name);
int proto_new(int proto, const char *name, int (*init)(struct pork_proto *));
void proto_destroy(void);
struct pork_proto *proto_get(int protocol);
struct pork_proto *proto_get_name(const char *name);

#endif /* __NCIC_PROTO_H__ */
