/*
* Based on pork_irc_input.c
** ncic_irc_input.c
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_misc.h"
#include "ncic_queue.h"
#include "ncic_buddy.h"
#include "ncic_color.h"
#include "ncic_inet.h"
#include "ncic_acct.h"
#include "ncic_proto.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_chat.h"
#include "ncic_msg.h"
//#include "ncic_missing.h"

#include "ncic_irc.h"
#include "ncic_naken.h"

static int naken_process_input(irc_session_t *session, char *input, int len);
static struct naken_input *naken_tokenize(irc_session_t *session, char *input);
static int naken_handler_nick(struct pork_acct *acct, struct naken_input *in,
    char *old_name);

static int
naken_process_input(irc_session_t *session, char *input, int len)
{
	struct pork_acct *acct = session->data;
	struct buddy_pref *pref = acct->buddy_pref;
	struct naken_input *in;
	char *tmp;
	int number;

	if (input[0] == '@') {
		struct chatroom *chat;
		struct bgroup *group;

		acct->state = STATE_READY;
		pork_acct_connected(acct);

		chat = chat_new(acct, "main", "main", screen.status_win);
		group = group_add(acct, "User List");
		return 0;
	}

	if (len < 1) {
		screen_win_msg(cur_window(), 0, 0, 0, MSG_TYPE_CMD_OUTPUT, "%s", input);
		return 0;
	}

	if (input[0] == '+' && input[1] == '[') {
		int name_len;

		/* Add a user to the user list. Do a simple tokenization
		 * of the input. +[#]User where
		 * # = the user number (line/id)
		 * User = the user name */

		tmp = strchr(input, ']');
		*tmp = '\0';

		number = atoi(input + 2);

		if ((number + 1) > pref->user_list_size) {
			int i;
			int grow_by;

			grow_by = ((number + 1) - pref->user_list_size) + 1;
			pref->user_list = xrealloc(pref->user_list,
			    (pref->user_list_size + grow_by) * sizeof(char *));
			/* Set all the new cells created to NULL */
			for (i = pref->user_list_size; i < pref->user_list_size + grow_by; i++) {
				/* Set the cell to NULL */
				pref->user_list[i] = NULL;
			}
			pref->user_list_size += grow_by;
		}

		name_len = strlen(tmp + 1);
		if (pref->user_list[number] != NULL) {
			/* This shouldn't happen, if it does naken chat
			 * server is fucked. */
			free(pref->user_list[number]);
		}
		pref->user_list[number] = xmalloc(name_len + 1);
		strncpy(pref->user_list[number], tmp + 1, name_len);
		pref->user_list[number][name_len] = '\0';

		buddy_add(acct, pref->user_list[number], number);

		return 0;
	}

	if (input[0] == '-' && input[1] == '[') {
		/* Remove a user from the user list */
		tmp = strchr(input, ']');
		*tmp = '\0';

		number = atoi(input + 2);

		buddy_remove(acct, pref->user_list[number], number);

		free(pref->user_list[number]);
		pref->user_list[number] = NULL;
		return 0;
	}
	if (strstr(input, ">> At the tone")) {
		return 0;
	}
	in = naken_tokenize(session, input);
	if (input[0] == '>') {
		ncic_recv_sys_alert(acct, in->orig);
	} else {
		screen_win_msg(cur_window(), 0, 0, 0, MSG_TYPE_CMD_OUTPUT, "%s", in->orig);
	}

	free(in->orig);
	free(in->message);
	free(in);
	return 0;
}

static struct naken_input *
naken_tokenize(irc_session_t *session, char *input)
{
	struct naken_input *in = xcalloc(1, sizeof(*in));
	char *sender;
	char *message;
	char *tmp;
	struct pork_acct *acct = session->data;

	in->orig = xstrdup(input);

	/* Identify the type of message we received */
	in->msg_type = MSG_NORMAL;

	if (input[0] == '>') {
		in->msg_type = MSG_SYSTEM_ALERT;
		/* Process system messages. They all start with >> */
		if (strstr(input, ">> Your name is") != NULL) {
			char *old_name;

			/* Handle name change */
			old_name = xstrdup(acct->username);
			/* Search for the first colon in the input string. */
			tmp = strchr(input, ':');
			tmp += 2; /* Go past : and space */
			in->args = tmp;
			naken_handler_nick(acct, in, old_name);
			free(old_name);
		}
		return (in);
	}

	if (input[0] == '<') {
		/* We got a private message */
		in->msg_type = MSG_PRIVATE;
	} else if (input[0] == '#') {
		/* Got a yell */
		in->msg_type = MSG_YELL;
	}

	/* Now we will get the sender, the number, and the message, using
	 * strchr and strstr processing. */
	message = strchr(input, ':');
	if (message != NULL) {
		*message = '\0';
		message += 2; /* Go past the NULL and the space */
	} else {
		in->msg_type = MSG_SYSTEM_NORMAL;
		in->message = NULL;
		return (in);
	}
	sender = input;

	/* Get the sender's number */
	if (in->msg_type == MSG_PRIVATE) {
		tmp = strchr(sender, '>');
	} else if (in->msg_type == MSG_YELL) {
		tmp = strchr(sender, '#');
		tmp = strchr(tmp + 1, '#');
	} else {
		tmp = strchr(sender, ']');
		if (tmp != NULL) {
			in->msg_type = MSG_NORMAL;
		} else {
			in->message = NULL;
			return (in);
		}
	}
	*tmp = '\0';

	in->sender = atoi(input + 1);
	in->message = xstrdup(message);

	return (in);
}

static ssize_t irc_read_data(irc_session_t *session, char *buf, size_t len) {
	int i;
	ssize_t ret = 0;

	for (i = 0 ; i < 5 ; i++) {
		ret = SSL_read(session->sslHandle, buf, len - 1);
//		ret = read(sock, buf, len - 1);
		if (ret == -1) {
			if (errno == EINTR)
				continue;

			debug("sock err: %d:%s", sock, strerror(errno));
			return (-1);
		}

		if (ret == 0) {
			debug("sock err: %d:%s", sock, strerror(errno));
			return (-1);
		}

		buf[ret] = '\0';
		return (ret);
	}

	return (-1);
}

/*
** Returns -1 if the connection died, 0 otherwise.
*/

int irc_input_dispatch(irc_session_t *session) {
	int ret;
	char *p;
	char *cur;
	struct pork_acct *acct = session->data;
	int i;
	char input[2048];

	ret = irc_read_data(session,
			&session->input_buf[session->input_offset],
			sizeof(session->input_buf) - session->input_offset);

	if (ret < 1) {
		pork_sock_err(acct, session->sock);
		return (-1);
	}

	cur = session->input_buf;
	p = cur;
	i = 0;

	while (*p != '\0') {
		if (*p == '\r') {
			p++;
			continue;
		}

		if (*p == '\n') {
			*p++ = '\0';
			input[i] = '\0';
			naken_process_input(session, input, i);
			cur = p;
			i = 0;
		} else {
			input[i++] = *p;
			p++;
		}
	}
#if 0
	while ((p = strchr(cur, '\r')) != NULL) {
		struct naken_input *in;
		char *q;

		*p++ = '\0';
		q = strchr(cur, '\n');
		if (q != NULL)
			*q = '\0';

		if (cur[0] == '@') {
			acct->state = STATE_READY;
			// Do more like 001
			pork_acct_connected(acct);
			continue;
		}
		screen_win_msg(cur_window(), 0, 0, 0, MSG_TYPE_CMD_OUTPUT, "%s", cur);
#if 0
		in = naken_tokenize(cur);


		if (in == NULL) {
			debug("invalid input from server: %s", cur);
			continue;
		}
#endif
		cur = p;
#if 0
		free(in->orig);
		free(in);
#endif
	}
#endif

	if (*cur != '\0') {
		size_t leftover;

		leftover = strlen(cur);

		/* Move the '\0', too */
		memmove(session->input_buf, cur, leftover + 1);
		session->input_offset = leftover;
	} else
		session->input_offset = 0;

	return (0);
}

#if 0
static int irc_handler_privmsg(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	char *host;

	if (in->num_tokens < 3 || in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p++ = '\0';
	host = p;

	/* ^A */
	if (in->args[0] == 0x01) {
		char *dest;

		p = strrchr(&in->args[1], 0x01);
		if (p == NULL)
			goto no_ctcp;
		*p++ = '\0';

		in->cmd = &in->args[1];
		p = strchr(in->cmd, ' ');
		if (p != NULL)
			*p++ = '\0';
		in->args = p;

		if (host != NULL)
			host[-1] = '!';

		if (irc_callback_run(acct->data, in, "CTCP") == 1)
			return (0);
		else if (host != NULL)
			host[-1] = '\0';

		/* XXX */
		dest = irc_text_filter(in->tokens[2]);
		if (in->args != NULL) {
			char *msg;

			msg = irc_text_filter(in->args);
			screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_CHAT_MSG_RECV,
				"%%WCTCP%%M %s %%D[%%x%s%%D]%%x from %%C%s%%D(%%c%s%%D)%%x to %%W%s",
				in->cmd, msg, in->tokens[0], host, dest);

			free(msg);
		} else {
			screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_PRIVMSG_RECV,
				"%%WCTCP%%M %s%%x from %%C%s%%D(%%c%s%%D)%%x to %%W%s",
				in->cmd, in->tokens[0], host, dest);
		}

		free(dest);
		return (0);
	}

no_ctcp:
	if (!acct->proto->user_compare(acct->username, in->tokens[2]))
		pork_recv_msg(acct, in->tokens[2], in->tokens[0], host, in->args, 0);
	else {
		struct chatroom *chat;
		char *p = in->tokens[2];

		/* these should be displayed in a way that lets the user know they aren't ordinary privmsgs */
		while (*p == '@' || *p == '+' || *p == '%')
			p++;

		chat = chat_find(acct, p);
		if (chat == NULL) {
			debug("receviced msg for unjoined chan %s \"%s\"", p, in->orig);
			return (-1);
		}

		chat_recv_msg(acct, chat, in->tokens[2], in->tokens[0], host, in->args);
	}

	return (0);
}
#endif

static int
naken_handler_nick(struct pork_acct *acct, struct naken_input *in, char *old_name)
{
	if (in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (!acct->proto->user_compare(acct->username, old_name)) {
		free(acct->username);
		acct->username = xstrdup(in->args);
	}

	return (chat_nick_change(acct, old_name, in->args));
}

