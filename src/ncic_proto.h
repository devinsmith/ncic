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

#ifdef __cplusplus
extern "C" {
#endif

struct pork_acct;
struct imwindow;
struct chatroom;
struct chat_user;
struct buddy;

enum {
	PROTO_NULL = -1,
	PROTO_IRC,
	PROTO_MAX
};

struct pork_proto {
	char name[32];

	int (*init)(struct pork_acct *);
	int (*free)(struct pork_acct *);
	int (*update)(struct pork_acct *);
	int (*user_compare)(const char *u1, const char *u2);
	char *(*filter_text)(char *);

	int (*connect)(struct pork_acct *, char *);
	int (*connect_abort)(struct pork_acct *acct);
	int (*reconnect)(struct pork_acct *, char *);
	int (*disconnected)(struct pork_acct *);
	int (*signoff)(struct pork_acct *acct, const char *reason);

	struct chatroom *(*chat_find)(struct pork_acct *, char *);
	int (*chat_free)(struct pork_acct *, void *chat_data);
	int (*chat_unignore)(struct pork_acct *, struct chatroom *, char *user);
	int (*chat_rejoin)(struct pork_acct *, struct chatroom *);
	int (*chat_leave)(struct pork_acct *, struct chatroom *);
	int (*chat_name)(const char *name, char *buf, size_t len, char *, size_t);
	int (*chat_send)(struct pork_acct *, struct chatroom *chat, const char *, char *);
	int (*chat_action)(struct pork_acct *, struct chatroom *, char *, char *msg);
	int (*chat_send_notice)(struct pork_acct *, struct chatroom *, char *, char *);
	int (*chat_user_free)(struct pork_acct *acct, struct chat_user *);

	int (*set_idle_time)(struct pork_acct *, u_int32_t);

	int (*send_action)(struct pork_acct *, char *target, char *msg);
	int (*send_msg)(struct pork_acct *, char *target, char *msg);
	int (*send_notice)(struct pork_acct *, char *target, char *msg);
	int (*send_msg_auto)(struct pork_acct *, char *target, char *msg);
	int (*set_back)(struct pork_acct *);
	int (*set_away)(struct pork_acct *, char *);

	int (*mode)(struct pork_acct *, char *);
	int (*ctcp)(struct pork_acct *, char *dest, char *str);

	int (*change_nick)(struct pork_acct *acct, char *nick);
};

int proto_init(void);
void proto_destroy(void);
struct pork_proto *proto_get(int protocol);
struct pork_proto *proto_get_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_PROTO_H__ */
