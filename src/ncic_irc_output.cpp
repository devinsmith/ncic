/*
** Based on pork_irc_output.c
** ncic_irc_output.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <cstdio>
#include <ncurses.h>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_queue.h"
#include "ncic_inet.h"
#include "ncic_io.h"
#include "ncic_acct.h"
#include "ncic_imwindow.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"

#include "ncic_irc.h"
#include "ncic_naken.h"

static int irc_send_server(irc_session_t *session, char *cmd, size_t len) {
	return SSL_write (session->sslHandle, cmd, len);
}

int irc_send(irc_session_t *session, char *command, size_t len) {
	int ret;

	if (session->sock < 0) {
		struct irc_cmd_q *cmd = (struct irc_cmd_q *)xmalloc(sizeof(*cmd));

		cmd->cmd = xstrdup(command);
		cmd->len = len;

		if (queue_add(session->outq, cmd) != 0) {
			screen_err_msg("Error: %s: Error adding IRC command to the outbound queue.",
				((struct pork_acct *) session->data)->username);

			free(cmd->cmd);
			free(cmd);
			return (-1);
		}

		return (0);
	}

	ret = irc_send_server(session, command, len);
	if (ret == -1)
		pork_sock_err(session->data, session->sock);

	return (ret);
}

int irc_flush_outq(irc_session_t *session) {
	struct irc_cmd_q *cmd;
	int ret = 0;

	while ((cmd = (struct irc_cmd_q *)queue_get((pork_queue_t *)session->outq)) != nullptr) {
		if (irc_send_server(session, cmd->cmd, cmd->len) > 0) {
			ret++;
			free(cmd->cmd);
			free(cmd);
		} else {
			debug("adding %s back to the queue", cmd->cmd);
			queue_putback_head(session->outq, cmd);
		}
	}

	return (ret);
}

int irc_connect(struct pork_acct *acct,
				const char *server,
				int *sock)
{
	struct sockaddr_storage ss;
	struct sockaddr_storage local;
	in_port_t port_num;
	char *irchost = getenv("IRCHOST");
	char *port;
	char buf[IRC_OUT_BUFLEN];
	char *passwd = nullptr;

	if (server == nullptr || xstrncpy(buf, server, sizeof(buf)) == -1)
		return (-1);

	memset(&ss, 0, sizeof(ss));
	memset(&local, 0, sizeof(local));

	port = strchr(buf, ':');
	if (port != nullptr) {
		*port++ = '\0';

		passwd = strchr(port, ':');
		if (passwd != nullptr) {
			*passwd++ = '\0';
		}
	} else
		port = DEFAULT_SECURE_PORT;

	if (get_port(port, &port_num) != 0) {
		screen_err_msg("Error: %s: Invalid IRC server port: %s",
			acct->username, port);
		memset(buf, 0, sizeof(buf));
		return (-1);
	}

	if (get_addr(buf, &ss) != 0) {
		screen_err_msg("Error: %s: Invalid IRC server host: %s",
			acct->username, buf);
		memset(buf, 0, sizeof(buf));
		return (-1);
	}

	if (irchost != nullptr) {
		if (get_addr(irchost, &local) != 0) {
			screen_err_msg("Error: %s: Invalid local hostname: %s",
				acct->username, irchost);
			memcpy(&local, &acct->laddr, sizeof(local));
		}
	} else
		memcpy(&local, &acct->laddr, sizeof(local));

	free(acct->fport);
	acct->fport = xstrdup(port);

	free(acct->server);
	acct->server = xstrdup(buf);

	if (passwd != nullptr && passwd[0] != '\0') {
		free_str_wipe(acct->passwd);
		acct->passwd = xstrdup(passwd);
	}

	sin_set_port(&local, acct->lport);
	memset(buf, 0, sizeof(buf));
	return (nb_connect(&ss, &local, port_num, sock));
}

int irc_send_raw(irc_session_t *session, char *str) {
	int ret;
	char *buf;
	size_t len;

	len = strlen(str) + 3;
	buf = (char *)xmalloc(len);
	snprintf(buf, len, "%s\r\n", str);

	ret = irc_send(session, buf, len - 1);
	free(buf);
	return (ret);
}

int irc_send_mode(irc_session_t *session, char *mode_str) {
	int ret;
	char buf[IRC_OUT_BUFLEN];

	ret = snprintf(buf, sizeof(buf), "MODE %s\r\n", mode_str);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_send_pong(irc_session_t *session, char *dest) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	ret = snprintf(buf, sizeof(buf), "\r\n");
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_set_away(irc_session_t *session, char *msg) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	if (msg != nullptr)
		ret = snprintf(buf, sizeof(buf), "%% is now away (%s)\r\n", msg);
	else
		ret = snprintf(buf, sizeof(buf), "%% is now away\r\n");

	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_send_invite(irc_session_t *session, char *channel, char *user) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	ret = snprintf(buf, sizeof(buf), "INVITE %s %s\r\n", user, channel);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_send_login(irc_session_t *session) {
	char buf[IRC_OUT_BUFLEN];
	struct pork_acct *acct = static_cast<struct pork_acct *>(session->data);
	int ret;

	ret = snprintf(buf, sizeof(buf), ".n%s\r\n", acct->username);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	ret = irc_send(session, buf, ret);
	if (ret == -1)
		return (-1);

	ret = snprintf(buf, sizeof(buf), ".Z\r\n");
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_send_privmsg(irc_session_t *session, char *dest, char *msg) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	ret = snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", dest, msg);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int
naken_send(irc_session_t *session, char *msg)
{
	char buf[IRC_OUT_BUFLEN];
	int ret;

	ret = snprintf(buf, sizeof(buf), "%s\r\n", msg);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_send_ctcp(irc_session_t *session, char *dest, char *msg) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	if (!strcmp(dest, "*"))
		dest = cur_window()->target;

	ret = snprintf(buf, sizeof(buf), "PRIVMSG %s :\x01%s\x01\r\n", dest, msg);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_send_quit(irc_session_t *session, const char *reason) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	ret = snprintf(buf, sizeof(buf), ".q\r\n");
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	pork_io_del(session);
	return (irc_send(session, buf, ret));
}

int irc_send_notice(irc_session_t *session, char *dest, char *msg) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	ret = snprintf(buf, sizeof(buf), "NOTICE %s :%s\r\n", dest, msg);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

int irc_send_action(irc_session_t *session, char *dest, char *msg) {
	char buf[IRC_OUT_BUFLEN];
	int ret;

	if (dest == nullptr || msg == nullptr)
		return (-1);

	ret = snprintf(buf, sizeof(buf), "ACTION %s", msg);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}
