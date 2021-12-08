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

struct irc_session_t;

/* Account states */
enum {
  STATE_DISCONNECTED,
  STATE_CONNECTING,
  STATE_READY
};

struct pork_proto;

class pork_acct {
public:
  pork_acct();
  ~pork_acct();

  // Set an account as connected.
  void set_connected();

public:
	char *username;
	char *passwd;
	char *userhost;
	char *away_msg;

	time_t last_input;

	int id;
	int state;

	u_int16_t idle_time;

	bool report_idle;
	bool marked_idle;
	bool can_connect;
	/* presently connected */
	bool connected;
	/* ever successfully connected */
	bool successful_connect;
	/* presently disconnected */
	bool disconnected;
	/* presently in the process of reconnecting */
	bool reconnecting;

	u_int32_t reconnect_tries{};
	time_t reconnect_next_try{};

	u_int32_t ref_count{};
	u_int32_t refnum{};

	dlist_t *chat_list{};
	hash_t autoreply{};

	char *fport{};
	char *server{};

	struct pork_proto *proto{};
	irc_session_t *data{};
};

int pork_acct_del_refnum(u_int32_t refnum, char *reason);
void pork_acct_del(struct pork_acct *acct, const char *reason);
void pork_acct_del_all(const char *reason);
struct pork_acct *pork_acct_find(u_int32_t refnum);
struct pork_acct *pork_acct_get_data(u_int32_t refnum);
void pork_acct_update(void);
int pork_acct_disconnected(struct pork_acct *acct);
void pork_acct_reconnect_all(void);
int pork_acct_connect(const char *user, char *args, int protocol);
int pork_acct_next_refnum(u_int32_t cur_refnum, u_int32_t *next);
struct pork_acct *pork_acct_init(const char *user, int protocol);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_ACCT_H__ */
