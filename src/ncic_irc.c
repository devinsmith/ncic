/*
** Based on pork_irc.c
** ncic_irc.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <unistd.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#define EINPROGRESS WSAEINPROGRESS
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <errno.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_queue.h"
#include "ncic_buddy.h"
#include "ncic_inet.h"
#include "ncic_acct.h"
#include "ncic_proto.h"
#include "ncic_io.h"
#include "ncic_imwindow.h"
#include "ncic_screen_io.h"
#include "ncic_chat.h"
#include <ncic_missing.h>

#include "ncic_irc.h"
#include "ncic_irc_dcc.h"
#include "ncic_naken.h"

#define HIGHLIGHT_BOLD			0x01
#define HIGHLIGHT_UNDERLINE		0x02
#define HIGHLIGHT_INVERSE		0x04

static int naken_update_buddy(struct pork_acct *acct,
		struct buddy *buddy,
		void *data)
{
	buddy->status = STATUS_ACTIVE;

	return (0);
}

static void irc_event(int sock, u_int32_t cond, void *data) {
	if (cond & IO_COND_READ) {
		if (irc_input_dispatch(data) == -1) {
			irc_session_t *session = data;
			struct pork_acct *acct = session->data;

			pork_sock_err(acct, sock);
			pork_io_del(data);
			pork_acct_disconnected(acct);

			return;
		}
	}

	irc_flush_outq(data);
}

static void irc_connected(int sock, u_int32_t cond, void *data) {
	int ret;
	irc_session_t *session = data;

	pork_io_del(data);

	ret = sock_is_error(sock);
	if (ret != 0) {
		struct pork_acct *acct = session->data;
		char *errstr = strerror(ret);

		screen_err_msg("network error: %s: %s", acct->username, errstr);

		close(sock);
		pork_acct_disconnected(acct);
	} else {
		session->sock = sock;

		/* Register the error strings for libcrypto & libssl */
		SSL_load_error_strings();
		/* Register the available ciphers and digests */
		SSL_library_init();
		OpenSSL_add_all_algorithms();

		// New context saying we are a client, and using SSL 2 or 3
		session->sslContext = SSL_CTX_new(SSLv3_client_method());
		if (session->sslContext == NULL) // Dumps to stderr, yuck
			ERR_print_errors_fp (stderr);

		SSL_CTX_set_verify(session->sslContext, SSL_VERIFY_NONE, NULL);
		SSL_CTX_set_verify_depth(session->sslContext, 0);
		SSL_CTX_set_mode(session->sslContext, SSL_MODE_AUTO_RETRY);
		SSL_CTX_set_session_cache_mode(session->sslContext, SSL_SESS_CACHE_CLIENT);

		// Create an SSL struct for the connection
		session->sslHandle = SSL_new(session->sslContext);
		if (session->sslHandle == NULL)
			ERR_print_errors_fp(stderr);

		// Connect the SSL struct to our connection
		if (!SSL_set_fd(session->sslHandle, session->sock))
			ERR_print_errors_fp(stderr);

		while ((ret = SSL_connect(session->sslHandle)) == -1) {
			fd_set fds;
			int ssl_err;
			struct pork_acct *acct = session->data;

			FD_ZERO(&fds);
			FD_SET(session->sock, &fds);

			switch (ssl_err = SSL_get_error(session->sslHandle, ret)) {
			case SSL_ERROR_WANT_READ:
				select(session->sock + 1, &fds, NULL, NULL, NULL);
				break;
			case SSL_ERROR_WANT_WRITE:
				select(session->sock + 1, NULL, &fds, NULL, NULL);
				break;
			default:
				screen_err_msg("network error: %s: could not connect %d",
				    acct->username, ssl_err);
				return;
				break;
			}
		}

		sock_setflags(sock, 0);
		pork_io_add(sock, IO_COND_READ, data, data, irc_event);
		irc_send_login(session);
	}
}

static int irc_init(struct pork_acct *acct) {
	irc_session_t *session = xcalloc(1, sizeof(*session));
	char *ircname;

	ircname = getenv("IRCNAME");
	if (ircname != NULL)
		acct->profile = xstrdup(ircname);
	else {
		if (acct->profile == NULL)
			acct->profile = xstrdup(DEFAULT_IRC_PROFILE);
	}

	session->outq = queue_new(0);
	session->inq = queue_new(0);
	session->sock = -1;
	session->sslHandle = NULL;
	session->sslContext = NULL;

	session->data = acct;
	acct->data = session;
	return (0);
}

static int irc_free(struct pork_acct *acct) {
	irc_session_t *session = acct->data;
	u_int32_t i;

	if (session->sslHandle) {
		SSL_shutdown(session->sslHandle);
		SSL_free(session->sslHandle);
	}

	if (session->sslContext)
		SSL_CTX_free(session->sslContext);

	for (i = 0 ; i < session->num_servers ; i++)
		free_str_wipe(session->servers[i]);

	free(session->chanmodes);
	free(session->chantypes);
	free(session->prefix_types);
	free(session->prefix_codes);

	queue_destroy(session->inq, free);
	queue_destroy(session->outq, free);

	pork_io_del(session);
	free(session);
	return (0);
}

static inline int irc_is_chan_type(irc_session_t *session, char c) {
	return (strchr(session->chantypes, c) != NULL);
}

static inline int irc_is_chan_prefix(irc_session_t *session, char c) {
	return (session->prefix_codes != NULL && strchr(session->prefix_codes, c) != NULL);
}

static int irc_update(struct pork_acct *acct) {
	irc_session_t *session = acct->data;
	time_t time_now;

	if (session == NULL)
		return (-1);

	time(&time_now);
	if (session->last_update + 300 <= time_now && acct->connected) {
		irc_send_pong(session, acct->server);
		session->last_update = time_now;
	}

	return (0);
}

static int irc_read_config(struct pork_acct *acct) {
	return (0);
}

static int irc_write_config(struct pork_acct *acct) {
	return (0);
}

static u_int32_t irc_add_servers(struct pork_acct *acct, char *str) {
	char *server;
	irc_session_t *session = acct->data;

	while ((server = strsep(&str, " ")) != NULL &&
			session->num_servers < array_elem(session->servers))
	{
		session->servers[session->num_servers++] = xstrdup(server);
	}

	return (session->num_servers);
}

static int irc_do_connect(struct pork_acct *acct, char *args) {
	irc_session_t *session = acct->data;
	int sock;
	int ret;

	if (args == NULL) {
		screen_err_msg("Error: IRC: Syntax is /connect <nick> <server>[:<port>[:<passwd>]] ... <serverN>[:<port>[:<passwd>]]");
		return (-1);
	}

	if (irc_add_servers(acct, args) < 1) {
		screen_err_msg("Error: %s: No server specified", acct->username);
		return (-1);
	}

	screen_err_msg("Server is %s", session->servers[0]);
	ret = irc_connect(acct, session->servers[0], &sock);
	if (ret == 0)
		irc_connected(sock, 0, session);
	else if (ret == -EINPROGRESS)
		pork_io_add(sock, IO_COND_WRITE, session, session, irc_connected);
	else
		return (-1);

	return (0);
}

static int irc_connect_abort(struct pork_acct *acct) {
	irc_session_t *session = acct->data;

	close(session->sock);
	pork_io_del(session);
	return (0);
}

static int irc_reconnect(struct pork_acct *acct, char *args __notused) {
	int sock;
	int ret;
	irc_session_t *session = acct->data;
	u_int32_t server_num;

	server_num = (acct->reconnect_tries - 1) % session->num_servers;

	ret = irc_connect(acct, session->servers[server_num], &sock);
	if (ret == 0) {
		irc_connected(sock, 0, session);
	} else if (ret == -EINPROGRESS)
		pork_io_add(sock, IO_COND_WRITE, session, session, irc_connected);
	else
		return (-1);

	return (0);
}

static int irc_join(struct pork_acct *acct, char *chan, char *args) {
	screen_err_msg("Got here: irc_join\n");
	return (irc_send_join(acct->data, chan, args));
}

static int irc_privmsg(struct pork_acct *acct, char *dest, char *msg) {
	char *p;

	/* XXX - fix this */
	p = strchr(dest, ',');
	if (p != NULL)
		*p = '\0';

	return (irc_send_privmsg(acct->data, dest, msg));
}

static int irc_mode(struct pork_acct *acct, char *str) {
	return (irc_send_mode(acct->data, str));
}

static int irc_ctcp(struct pork_acct *acct, char *dest, char *msg) {
	return (irc_send_ctcp(acct->data, dest, msg));
}

static int irc_chan_send(struct pork_acct *acct,
			struct chatroom *chat,
			char *target,
			char *msg)
{
	return (naken_send(acct->data, msg));
}

static int chat_find_compare_cb(void *l, void *r) {
	char *str = l;
	struct chatroom *chat = r;

	return (strcasecmp(str, chat->title));
}

static struct chatroom *irc_find_chat(struct pork_acct *acct, char *chat) {
	dlist_t *node;
	irc_session_t *session = acct->data;

	while (irc_is_chan_prefix(session, *chat))
		chat++;

	if (session->chantypes != NULL && !irc_is_chan_type(session, *chat))
		return (NULL);

	node = dlist_find(acct->chat_list, chat, chat_find_compare_cb);
	if (node == NULL)
		return (NULL);

	return (node->data);
}

static int
naken_whois(struct pork_acct *acct, char *dest) {
	char *p;

	while ((p = strsep(&dest, ",")) != NULL)
		irc_send_whois(acct->data, p);

	return (0);
}

static int irc_whowas(struct pork_acct *acct, char *dest) {
	char *p;

	while ((p = strsep(&dest, ",")) != NULL)
		irc_send_whowas(acct->data, p);

	return (0);
}

static int irc_chan_get_name(	const char *str,
								char *buf,
								size_t len,
								char *arg_buf,
								size_t arg_len)
{
	char *p;

	if (xstrncpy(buf, str, len) == -1) {
		debug("xstrncpy failed: %s", str);
		return (-1);
	}

	p = strchr(buf, ',');
	if (p != NULL)
		*p = '\0';

	p = strchr(buf, ' ');
	if (p != NULL)
		*p++ = '\0';

	if (p != NULL) {
		if (xstrncpy(arg_buf, p, arg_len) == -1)
			return (-1);
	} else
		arg_buf[0] = '\0';

	return (0);
}

static int irc_change_nick(struct pork_acct *acct, char *nick) {
	irc_session_t *irc = acct->data;

	if (irc->nick_len) {
		if (strlen(nick) > irc->nick_len) {
			screen_err_msg("Error: Nick is too long. Maximum length is %d",
				irc->nick_len);

			return (-1);
		}
	}

	if (!acct->connected) {
		free(acct->username);
		acct->username = xstrdup(nick);
	}

	return (irc_send_nick(acct->data, nick));
}

static int irc_chan_users(struct pork_acct *acct, struct chatroom *chat) {
	return (irc_send_names(acct->data, chat->title));
}

static int irc_chan_kick(	struct pork_acct *acct,
							struct chatroom *chat,
							char *user,
							char *reason)
{
	if (reason == NULL)
		reason = "No reason given";

	return (irc_send_kick(acct->data, chat->title, user, reason));
}

static int irc_chan_ban(	struct pork_acct *acct,
							struct chatroom *chat,
							char *user)
{
	struct chat_user *chat_user;
	char buf[1024];
	int ret = -1;

	chat_user = chat_find_user(acct, chat, user);
	if (chat_user != NULL && chat_user->host != NULL) {
		ret = snprintf(buf, sizeof(buf), "%s +b *!%s",
				chat->title, chat_user->host);
	} else
		ret = snprintf(buf, sizeof(buf), "%s +b %s", chat->title, user);

	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send_mode(acct->data, buf));
}

static int irc_chan_notice(	struct pork_acct *acct,
							struct chatroom *chat,
							char *target,
							char *msg)
{
	return (irc_send_notice(acct->data, target, msg));
}

static int irc_chan_who(struct pork_acct *acct, struct chatroom *chat) {
	return (irc_send_who(acct->data, chat->title));
}

static int irc_notice(struct pork_acct *acct, char *dest, char *msg) {
	return (irc_send_notice(acct->data, dest, msg));
}

static int irc_who(struct pork_acct *acct, char *str) {
	return (irc_send_who(acct->data, str));
}

static int irc_ping(struct pork_acct *acct, char *str) {
	return (irc_send_ping(acct->data, str));
}

static int irc_quit(struct pork_acct *acct, char *reason) {
	if (acct->connected)
		return (irc_send_quit(acct->data, reason));

	return (-1);
}

static int irc_is_chat(struct pork_acct *acct, char *str) {
	irc_session_t *irc = acct->data;

	if (irc->chantypes != NULL)
		return (irc_is_chan_type(irc, *str));
	else if (*str == '#' || *str == '&')
		return (1);

	return (0);
}

static int irc_quote(struct pork_acct *acct, char *str) {
	char *p = str;

	if (str == NULL)
		return (-1);

	while (*p == ' ')
		p++;

	if (!strncasecmp(p, "NICK ", 5)) {
		screen_err_msg("Use the /nick command.");
		return (-1);
	}

	return (irc_send_raw(acct->data, str));
}

static int irc_action(struct pork_acct *acct, char *dest, char *msg) {
	return (irc_send_action(acct->data, dest, msg));
}

static int irc_chan_action(	struct pork_acct *acct,
							struct chatroom *chat,
							char *target,
							char *msg)
{
	return (irc_send_action(acct->data, target, msg));
}

static int irc_away(struct pork_acct *acct, char *msg) {
	return (irc_set_away(acct->data, msg));
}

static int irc_back(struct pork_acct *acct) {
	return (naken_set_back(acct->data, NULL));
}

static int irc_topic(	struct pork_acct *acct,
						struct chatroom *chat,
						char *topic)
{
	return (irc_send_topic(acct->data, chat->title, topic));
}

static int irc_invite(	struct pork_acct *acct,
						struct chatroom *chat,
						char *user,
						char *msg __notused)
{
	return (irc_send_invite(acct->data, chat->title, user));
}

char *irc_text_filter(char *str) {
	static const char *mirc_fg_col = "wwbgrymyYGcCBMDW";
	static const char *mirc_bg_col = "ddbgrymyygccbmww";
	static const char *ansi_esc_col = "drgybmcwDRGYBMCW";
	size_t len;
	char *ret;
	size_t i;
	int fgcol = 7;
	int bgcol = -1;
	u_int32_t highlighting = 0;

	if (str == NULL)
		return (xstrdup(""));

	len = strlen(str) + 1024;
	ret = xmalloc(len);

	len--;
	for (i = 0 ; i < len && *str != '\0' ;) {
		switch (*str) {
			case '%':
				if (i + 2 >= len)
					goto out;
				memcpy(&ret[i], "%%", 2);
				i += 2;
				str++;
				break;

			/* ^B - bold */
			case 0x02:
				if (!(highlighting & HIGHLIGHT_BOLD)) {
					if (i + 2 >= len)
						goto out;

					memcpy(&ret[i], "%1", 2);
					i += 2;
					highlighting |= HIGHLIGHT_BOLD;
				} else {
					if (i + 3 >= len)
						goto out;
					memcpy(&ret[i], "%-1", 3);
					i += 3;
					highlighting &= ~HIGHLIGHT_BOLD;
				}
				str++;
				break;

			/* ^O - clear everything */
			case 0x0f:
				if (i + 2 >= len)
					goto out;
				memcpy(&ret[i], "%x", 2);
				i += 2;
				highlighting = 0;
				str++;
				break;

			/* ^V - inverse */
			case 0x16:
				if (!(highlighting & HIGHLIGHT_INVERSE)) {
					if (i + 2 >= len)
						goto out;

					memcpy(&ret[i], "%2", 2);
					i += 2;
					highlighting |= HIGHLIGHT_INVERSE;
				} else {
					if (i + 3 >= len)
						goto out;

					memcpy(&ret[i], "%-2", 3);
					i += 3;
					highlighting &= ~HIGHLIGHT_INVERSE;
				}
				str++;
				break;

			/* ^_ - underline */
			case 0x1f:
				if (!(highlighting & HIGHLIGHT_UNDERLINE)) {
					if (i + 2 >= len)
						goto out;
					memcpy(&ret[i], "%3", 2);
					i += 2;

					highlighting |= HIGHLIGHT_UNDERLINE;
				} else {
					if (i + 3 >= len)
						goto out;
					memcpy(&ret[i], "%-3", 3);
					i += 3;

					highlighting &= ~HIGHLIGHT_UNDERLINE;
				}
				str++;
				break;

			/* ^C - mirc color code */
			case 0x03: {
				int fgcol = -1;
				int bgcol = -1;
				char colbuf[4];

				if (!isdigit(str[1])) {
					if (i + 2 >= len)
						goto out;
					memcpy(&ret[i], "%x", 2);
					str++;
					i += 2;
					break;
				}
				str++;

				memcpy(colbuf, str, 2);
				colbuf[2] = '\0';

				if (isdigit(colbuf[1]))
					str += 2;
				else
					str++;

				fgcol = strtol(colbuf, NULL, 10) % 16;

				if (*str == ',') {
					memcpy(colbuf, &str[1], 2);
					colbuf[2] = '\0';

					if (isdigit(colbuf[0])) {
						if (isdigit(str[2]))
							str += 3;
						else
							str += 2;

						bgcol = strtol(colbuf, NULL, 10) % 16;
					}
				}

				if (i + 2 >= len)
					goto out;

				ret[i++] = '%';
				ret[i++] = mirc_fg_col[fgcol];

				if (bgcol >= 0) {
					if (i + 2 >= len)
						goto out;

					ret[i++] = ',';
					ret[i++] = mirc_bg_col[bgcol];
				}

				break;
			}

			/* ^[ - ANSI escape sequence */
			case 0x1b: {
				char *end;
				char *p;
				int bold = 0;
				char buf[64];
				int slen;

				buf[0] = '\0';
				if (str[1] != '[')
					goto add;

				end = strchr(&str[2], 'm');
				if (end == NULL)
					goto add;
				*end++ = '\0';

				str += 2;
				while ((p = strsep(&str, ";")) != NULL) {
					char *n;
					int num;

					num = strtoul(p, &n, 10);
					if (*n != '\0')
						continue;

					switch (num) {
						/* foreground color */
						case 30 ... 39:
							fgcol = num - 30;
							break;

						/* background color */
						case 40 ... 49:
							bgcol = num - 40;
							break;

						/* bold */
						case 1:
							bold = 8;
							break;

						/* underscore */
						case 4:
							if (xstrncat(buf, "%3", sizeof(buf)) == -1)
								goto out;
							break;

						/* blink */
						case 5:
							if (xstrncat(buf, "%4", sizeof(buf)) == -1)
								goto out;
							break;

						/* reverse */
						case 7:
							if (xstrncat(buf, "%2", sizeof(buf)) == -1)
								goto out;
							break;

						/* clear all attributes */
						case 0:
							if (p[1] != 'm')
								break;

							if (xstrncat(buf, "%x", sizeof(buf)) == -1)
								goto out;
							fgcol = -1;
							bgcol = -1;
							bold = 0;
							break;
					}
				}

				if (fgcol >= 0 || bgcol >= 0) {
					if (fgcol < 0)
						fgcol = 7;

					if (i + 2 >= len)
						goto out;

					ret[i++] = '%';
					ret[i++] = ansi_esc_col[fgcol + bold];

					if (bgcol >= 0) {
						if (i + 2 >= len)
							goto out;
						ret[i++] = ',';
						ret[i++] = ansi_esc_col[bgcol];
					}
				} else if (bold) {
					if (xstrncat(buf, "%1", sizeof(buf)) == -1)
						goto out;
				}

				ret[i] = '\0';
				slen = xstrncat(ret, buf, len - i);
				if (slen == -1)
					goto out;
				i += slen;
				str = end;

				break;
			}

			default:
			add:
				ret[i++] = *str++;
				break;
		}
	}
out:

	ret[i] = '\0';
	return (ret);
}

int irc_chan_free(struct pork_acct *acct, void *data) {
	free(data);

	return (0);
}

int irc_chanmode_has_arg(irc_session_t *session, char mode) {
	char *p;

	if (session->chanmodes == NULL) {
		switch (mode) {
			case 'k':
			case 'l':
			case 'e':
			case 'b':
			case 'o':
			case 'h':
			case 'v':
			case 'I':
				return (1);
		}

		return (0);
	}

	p = strchr(session->chanmodes, mode);
	if (p == NULL || p[1] != ',')
		return (0);

	return (1);
}

static int irc_rejoin(struct pork_acct *acct, struct chatroom *chat) {
	struct irc_chan_data *irc_data = chat->data;

	return (irc_send_join(acct->data, chat->title, NULL));
}

int irc_proto_init(struct pork_proto *proto) {
	proto->buddy_update = naken_update_buddy;
	proto->chat_action = irc_chan_action;
	proto->chat_join = irc_join;
	proto->chat_rejoin = irc_rejoin;
	proto->chat_send = irc_chan_send;
	proto->chat_find = irc_find_chat;
	proto->chat_name = irc_chan_get_name;
	proto->chat_leave = NULL;
	proto->chat_users = irc_chan_users;
	proto->chat_kick = irc_chan_kick;
	proto->chat_ban = irc_chan_ban;
	proto->chat_free = irc_chan_free;
	proto->chat_send_notice = irc_chan_notice;
	proto->chat_who = irc_chan_who;
	proto->chat_set_topic = irc_topic;
	proto->chat_invite = irc_invite;

	proto->send_action = irc_action;
	proto->get_profile = naken_whois;
	proto->connect = irc_do_connect;
	proto->connect_abort = irc_connect_abort;
	proto->reconnect = irc_reconnect;
	proto->free = irc_free;
	proto->mode = irc_mode;
	proto->init = irc_init;
	proto->who = irc_who;
	proto->ping = irc_ping;
	proto->whowas = irc_whowas;
	proto->send_notice = irc_notice;
	proto->signoff = irc_quit;
	proto->normalize = xstrncpy;
	proto->read_config = irc_read_config;
	proto->send_msg = irc_privmsg;
	proto->update = irc_update;
	proto->write_config = irc_write_config;
	proto->user_compare = strcasecmp;
	proto->change_nick = irc_change_nick;
	proto->filter_text = irc_text_filter;
	proto->filter_text_out = irc_text_filter;
	proto->quote = irc_quote;
	proto->is_chat = irc_is_chat;
	proto->set_away = irc_away;
	proto->set_back = irc_back;
	proto->ctcp = irc_ctcp;

	proto->file_accept = irc_file_accept;
	proto->file_recv_data = irc_recv_data;
	proto->file_abort = irc_file_abort;
	proto->file_send = irc_file_send;

	return (0);
}
