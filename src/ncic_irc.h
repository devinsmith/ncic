/*
** Based on pork_irc.h
** ncic_irc.h
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_IRC_H__
#define __NCIC_IRC_H__

#define IRC_OUT_BUFLEN		2048
#define IRC_IN_BUFLEN		8192

#define DEFAULT_SECURE_PORT	6667

#include "ncic_acct.h"
#include "ncic_queue.h"

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

struct chatroom;
struct pork_acct;

enum {
	MODE_PLUS = '+',
	MODE_MINUS = '-'
};

struct irc_session_t {
	int sock;

  bool use_ssl;
	SSL *sslHandle;
	SSL_CTX *sslContext;

	pork_queue_t *inq;
	pork_queue_t *outq;

	char *server;

	time_t last_update;
	size_t input_offset;
	char input_buf[IRC_IN_BUFLEN];
	void *data;
};

struct irc_cmd_q {
	char *cmd;
	size_t len;
};

struct irc_input {
	char *tokens[20];
	char *args;
	char *cmd;
	char *orig;
	int numeric;
	u_int32_t num_tokens;
};

struct callback_handler {
	char *str;
	int (*handler)(struct pork_acct *acct, struct irc_input *in);
};

int irc_proto_init(struct pork_proto *proto);

int irc_flush_outq(struct irc_session_t *session);
int irc_connect(struct pork_acct *a, const char *server, int *sock);

int irc_send_pong(struct irc_session_t *session, char *dest);
int irc_send_login(struct irc_session_t *session);
int irc_send_quit(struct irc_session_t *session, const char *reason);
int irc_set_away(struct irc_session_t *session, char *msg);
int irc_send_action(struct irc_session_t *session, char *dest, char *msg);

int naken_input_dispatch(struct irc_session_t *session);

#endif /* __NCIC_IRC_H__ */
