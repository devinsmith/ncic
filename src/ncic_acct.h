/*
** Heavily based on pork_acct.h
** ncic_acct.h - account management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_ACCT_H__
#define __NCIC_ACCT_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ncic_inet.h"
#include "ncic_list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pork_proto;

/* Account states */
enum {
	STATE_DISCONNECTED,
	STATE_CONNECTING,
	STATE_READY
};

struct pork_acct {
	char *username;
	char *passwd;
	char *profile;
	char *userhost;
	char *away_msg;
	char umode[64];

	time_t last_input;

	struct timeval last_flush;

	int id;
	int state;

	u_int16_t idle_time;

	u_int16_t report_idle:1;
	u_int16_t marked_idle:1;
	u_int16_t can_connect:1;
	/* presently connected */
	u_int16_t connected:1;
	/* ever successfully connected */
	u_int16_t successful_connect:1;
	/* presently disconnected */
	u_int16_t disconnected:1;
	/* presently in the process of reconnecting */
	u_int16_t reconnecting:1;

	u_int32_t reconnect_tries;
	time_t reconnect_next_try;

	u_int32_t ref_count;
	u_int32_t refnum;

	dlist_t *chat_list;
	hash_t autoreply;

	char *fport;
	char *server;

	in_port_t lport;
	struct sockaddr_storage laddr;

	struct pork_proto *proto;
	void *data;
};

int pork_acct_del_refnum(u_int32_t refnum, char *reason);
void pork_acct_del(struct pork_acct *acct, const char *reason);
void pork_acct_del_all(const char *reason);
struct pork_acct *pork_acct_find(u_int32_t refnum);
struct pork_acct *pork_acct_get_data(u_int32_t refnum);
void pork_acct_update(void);
int pork_acct_disconnected(struct pork_acct *acct);
void pork_acct_reconnect_all(void);
void pork_acct_connected(struct pork_acct *acct);
int pork_acct_connect(const char *user, char *args, int protocol);
int pork_acct_next_refnum(u_int32_t cur_refnum, u_int32_t *next);
struct pork_acct *pork_acct_init(const char *user, int protocol);
int pork_acct_save(struct pork_acct *acct);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_ACCT_H__ */
