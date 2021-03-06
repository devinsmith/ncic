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

#define DEFAULT_IRC_PROFILE "i <3 pork"
#define DEFAULT_IRC_PORT	"6666"
#define DEFAULT_SECURE_PORT	"6667"

#include "ncic_queue.h"

#define IRC_CHAN_OP			0x01
#define IRC_CHAN_VOICE		0x02
#define IRC_CHAN_HALFOP		0x04

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

struct chatroom;

enum {
	MODE_PLUS = '+',
	MODE_MINUS = '-'
};

typedef struct {
	int sock;
	SSL *sslHandle;
	SSL_CTX *sslContext;

	pork_queue_t *inq;
	pork_queue_t *outq;

	char *servers[24];
	char *chanmodes;
	char *chantypes;
	char *prefix_types;
	char *prefix_codes;

	u_int32_t wallchops:1;
	u_int32_t excepts:1;
	u_int32_t capab:1;
	u_int32_t knock:1;
	u_int32_t invex:1;
	u_int32_t callerid:1;
	u_int32_t etrace:1;
	u_int32_t safelist:1;

	u_int32_t nick_len;
	u_int32_t kick_len;
	u_int32_t topic_len;
	u_int32_t num_servers;

	hash_t callbacks;

	time_t last_update;
	size_t input_offset;
	char input_buf[IRC_IN_BUFLEN];
	void *data;
} irc_session_t;

struct irc_chan_data {
	char mode_str[128];
	hash_t mode_args;
	/* This is such a stupid hack. */
	u_int32_t join_complete:1;
	u_int32_t joined:1;
};

struct irc_cmd_q {
	char *cmd;
	size_t len;
};

struct irc_chan_arg {
	int arg;
	char *val;
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

int irc_flush_outq(irc_session_t *session);
int irc_connect(struct pork_acct *a, const char *server, int *sock);

int irc_send_raw(irc_session_t *session, char *str);
int irc_send_pong(irc_session_t *session, char *dest);
int irc_send_login(irc_session_t *session);
int irc_send_privmsg(irc_session_t *session, char *dest, char *msg);
int irc_send_mode(irc_session_t *session, char *mode_str);
int irc_send_ctcp(irc_session_t *session, char *dest, char *msg);
int irc_send_ctcp_reply(irc_session_t *session, char *dest, char *msg);
int irc_send_names(irc_session_t *session, char *chan);
int irc_send_who(irc_session_t *session, char *dest);
int irc_send_whois(irc_session_t *session, char *dest);
int irc_send_whowas(irc_session_t *session, char *dest);
int irc_send_kick(irc_session_t *session, char *chan, char *nick, char *reason);
int irc_send_ping(irc_session_t *session, char *str);
int irc_send_quit(irc_session_t *session, char *reason);
int irc_send_topic(irc_session_t *session, char *chan, char *topic);
int irc_send_notice(irc_session_t *session, char *dest, char *msg);
int irc_kick(irc_session_t *session, char *chan, char *user, char *msg);
int irc_set_away(irc_session_t *session, char *msg);
int irc_send_action(irc_session_t *session, char *dest, char *msg);
int irc_chan_free(struct pork_acct *acct, void *data);
int irc_send_invite(irc_session_t *session, char *channel, char *user);

char *irc_get_chanmode_arg(struct irc_chan_data *chat, char mode);
int irc_chanmode_has_arg(irc_session_t *session, char mode);
int naken_input_dispatch(irc_session_t *session);
char *irc_text_filter(char *str);

#endif /* __NCIC_IRC_H__ */
