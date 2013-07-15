/*
** Based on pork_irc_dcc.c
** ncic_irc_dcc.c
** Copyright (C) 2004-2005 Ryan McCabe <ryan@numb.org>
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
#include "ncic_inet.h"
#include "ncic_acct.h"
#include "ncic_proto.h"
#include "ncic_io.h"
#include "ncic_imwindow.h"
#include "ncic_screen_io.h"
#include "ncic_chat.h"
#include "ncic_transfer.h"
#include <ncic_missing.h>

#include "ncic_irc.h"
#include "ncic_irc_dcc.h"

static void irc_dcc_send_handler_connected(int fd, u_int32_t cond, void *data) {
	struct file_transfer *xfer = data;

	pork_io_del(data);
	pork_io_add(fd, IO_COND_READ, xfer, xfer, transfer_recv_data);

	transfer_recv_accepted(xfer);
}

static void irc_file_send_ready(int fd, u_int32_t cond, void *data) {
	struct file_transfer *xfer = data;
	struct dcc *dcc = xfer->data;

	if (cond & IO_COND_READ) {
		u_int32_t offset;

		if (read(fd, &offset, 4) != 4) {
			pork_io_del(xfer);
			close(fd);
			transfer_lost(xfer);
			return;
		} else {
			dcc->ack = ntohl(offset);
			time(&dcc->last_active);
		}
	}

	if (cond & IO_COND_WRITE) {
		if (xfer->bytes_sent + xfer->start_offset < xfer->file_len)
			transfer_send_data(fd, cond, xfer);

		/*
		** Close the connection only after they've acked everything.
		** If our peer messes up and acks it all before it has it all,
		** it's its own fault.
		*/
		if (dcc->ack >= xfer->file_len) {
			pork_io_del(xfer);
			close(fd);
			transfer_send_complete(xfer);
		}
	}

	if (cond & IO_COND_DEAD) {
		pork_io_del(xfer);
		close(fd);
		transfer_lost(xfer);
	}
}

static void irc_file_send_peer_connected(int fd, u_int32_t cond, void *data) {
	struct file_transfer *xfer = data;
	int sock;
	socklen_t len = sin_len(&xfer->laddr);

	pork_io_del(xfer);

	sock = accept(fd, (struct sockaddr *) &xfer->faddr, &len);
	close(fd);

	if (sock == -1) {
		screen_err_msg("Error accepting connection: %s", strerror(errno));
		transfer_abort(xfer);
	}

	/*
	** Let the user figure out whether the connection is from the right person.
	*/
	get_ip(&xfer->faddr, xfer->faddr_ip, sizeof(xfer->faddr_ip));
	xfer->fport = ntohs(sin_port(&xfer->faddr));

	xfer->sock = sock;
	pork_io_add(xfer->sock, IO_COND_RW, xfer, xfer, irc_file_send_ready);
	transfer_send_accepted(xfer);
}

int irc_file_abort(struct file_transfer *xfer) {
	if (!(xfer->protocol_flags & IRC_DCC_REJECTED) &&
		xfer->status == TRANSFER_STATUS_WAITING)
	{
		int ret;
		char buf[4096];

		if (xfer->type == TRANSFER_DIR_SEND) {
			ret = snprintf(buf, sizeof(buf),
				(xfer->quote_fname ? "DCC REJECT GET \"%s\"" : "DCC REJECT GET %s"),
				xfer->fname_base);
		} else {
			ret = snprintf(buf, sizeof(buf),
				(xfer->quote_fname ? "DCC REJECT SEND \"%s\"" : "DCC REJECT SEND %s"),
				xfer->fname_base);
		}

		if (ret > 0 && (size_t) ret < sizeof(buf))
			irc_send_ctcp_reply(xfer->acct->data, xfer->peer_username, buf);
	}

	pork_io_del(xfer);
	close(xfer->sock);
	free(xfer->data);

	return (0);
}

int irc_file_accept(struct file_transfer *xfer) {
	int ret;
	int sock = -1;
	struct dcc *dcc;

	ret = nb_connect(&xfer->faddr, &xfer->laddr, xfer->fport, &sock);
	xfer->sock = sock;

	if (ret == 0) {
		close(sock);
		transfer_lost(xfer);
		return (-1);
	} else if (ret == -EINPROGRESS) {
		pork_io_add(sock, IO_COND_WRITE, xfer, xfer,
			irc_dcc_send_handler_connected);
	} else {
		screen_err_msg("Error connecting to %s:%d (%s)",
			xfer->faddr_ip, xfer->fport, strerror(errno));
		transfer_abort(xfer);
		return (-1);
	}

	dcc = xcalloc(1, sizeof(*dcc));
	xfer->data = dcc;

	return (0);
}

int irc_file_send(struct file_transfer *xfer) {
	char buf[4096];
	struct dcc *dcc;
	irc_session_t *session = xfer->acct->data;

	if (transfer_bind_listen_sock(xfer, session->sock) == -1) {
		screen_err_msg("Unable to bind a listening socket -- aborting transfer");
		transfer_abort(xfer);
		return (-1);
	}

	pork_io_add(xfer->sock, IO_COND_RW, xfer, xfer, irc_file_send_peer_connected);

	/* Only ipv4 support for now. */
	snprintf(buf, sizeof(buf),
		(xfer->quote_fname ? "DCC SEND \"%s\" %u %hu %llu" : "DCC SEND %s %u %hu %llu"),
		xfer->fname_base, htonl(SIN4(&xfer->laddr)->sin_addr.s_addr),
		xfer->lport, (long long int)xfer->file_len);

	dcc = xcalloc(1, sizeof(*dcc));
	xfer->data = dcc;

	irc_send_ctcp(session, xfer->peer_username, buf);
	transfer_request_send(xfer);
	return (0);
}

int irc_recv_data(struct file_transfer *xfer, char *buf, size_t len) {
	u_int32_t offset;
	struct dcc *dcc = xfer->data;

	time(&dcc->last_active);
	offset = htonl(xfer->bytes_sent);

	if (sock_write(xfer->sock, &offset, 4) != 4) {
		close(xfer->sock);
		transfer_lost(xfer);
		return (-1);
	}

	if (xfer->bytes_sent + xfer->start_offset >= xfer->file_len) {
		pork_io_del(xfer);
		close(xfer->sock);
		transfer_recv_complete(xfer);
	}

	return (0);
}

int irc_handler_dcc_send(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	char *filename;
	char *addr;
	char *temp;
	u_int32_t fport;
	u_int32_t filelen;
	struct file_transfer *xfer;
	char *host;

	host = strchr(in->tokens[0], '!');
	if (host != NULL)
		*host++ = '\0';

	if (in->args[0] == '\"') {
		p = terminate_quote(&in->args[1]);
		if (p == NULL || p[0] != ' ')
			return (-1);
		p++;
		filename = &in->args[1];
	} else {
		p = in->args;
		filename = strsep(&p, " ");
		if (filename == NULL)
			return (-1);
	}

	if (filename[0] == '\0')
		return (-1);

	addr = strsep(&p, " ");
	if (addr == NULL)
		return (-1);

	temp = strsep(&p, " ");
	if (temp == NULL)
		return (-1);

	if (str_to_uint(temp, &fport) == -1)
		return (-1);

	temp = strsep(&p, " ");
	if (temp == NULL)
		return (-1);

	if (str_to_uint(temp, &filelen) == -1)
		return (-1);

	/*
	** If p is not null or not empty, it points to
	** a an optional checksum. Ignore it for now.
	*/

	xfer = transfer_new(acct, in->tokens[0], TRANSFER_DIR_RECV,
			filename, filelen);

	if (xfer == NULL)
		return (-1);

	xfer->fport = fport;

	if (get_addr(addr, &xfer->faddr) == -1) {
		debug("aborting xfer %u", xfer->refnum);
		transfer_abort(xfer);
		return (-1);
	} else {
		get_ip(&xfer->faddr, xfer->faddr_ip, sizeof(xfer->faddr_ip));
		sin_set_port(&xfer->faddr, htons(fport));
	}

	memcpy(&xfer->laddr, &acct->laddr, sizeof(xfer->laddr));
	get_ip(&xfer->laddr, xfer->laddr_ip, sizeof(xfer->laddr_ip));

	transfer_request_recv(xfer);
	return (1);
}

int irc_handler_dcc_resume(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	char *filename;
	char *q;
	int temp;
	in_port_t port;
	off_t position;
	struct file_transfer *xfer = NULL;
	char buf[1024];
	dlist_t *cur;
	int ret;
	char *host;

	host = strchr(in->tokens[0], '!');
	if (host != NULL)
		*host++ = '\0';

	if (in->args[0] == '\"') {
		p = terminate_quote(&in->args[1]);
		if (p == NULL || p[0] != ' ')
			return (-1);
		p++;
		filename = &in->args[1];
	} else {
		p = in->args;
		filename = strsep(&p, " ");
		if (filename == NULL)
			return (-1);
	}

	q = strsep(&p, " ");
	if (q == NULL || p == NULL)
		return (-1);

	if (str_to_int(q, &temp) == -1)
		return (-1);

	if (temp < 0)
		return (-1);
	port = temp;

	if (str_to_int(p, &temp) == -1)
		return (-1);

	if (temp < 0)
		return (-1);
	position = temp;

	for (cur = acct->transfer_list ; cur != NULL ; cur = cur->next) {
		struct file_transfer *txfer = cur->data;

		if (!strcasecmp(txfer->peer_username, in->tokens[0]) &&
			txfer->lport == port)
		{
			xfer = txfer;
			break;
		}
	}

	if (xfer == NULL)
		return (-1);

	if (transfer_resume(xfer, position) == -1) {
		debug("aborting xfer %u", xfer->refnum);
		transfer_abort(xfer);
		return (-1);
	}

	ret = snprintf(buf, sizeof(buf),
			(xfer->quote_fname ? "DCC ACCEPT \"%s\" %hu %lld" : "DCC ACCEPT %s %hu %lld"),
			filename, port, (long long int)position);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	irc_send_ctcp(acct->data, xfer->peer_username, buf);
	return (1);
}

int irc_handler_dcc_accept(struct pork_acct *acct, struct irc_input *in) {
	/* PRIVMSG User2 :DCC ACCEPT filename port position */
	return (-1);
}

int irc_handler_dcc_reject(struct pork_acct *acct, struct irc_input *in) {
	char *type;
	char *p;
	char *host;

	host = strchr(in->tokens[0], '!');
	if (host != NULL)
		*host++ = '\0';

	p = in->args;
	type = strsep(&p, " ");
	if (type == NULL || p == NULL || p[0] == '\0')
		return (-1);

	if (!strcasecmp(type, "GET") || !strcasecmp(type, "SEND")) {
		struct file_transfer *xfer;

		if (p[0] == '\"') {
			if (terminate_quote(&p[1]) == NULL)
				return (-1);
		} else {
			char *end;

			end = strchr(p, ' ');
			if (end != NULL)
				*end = '\0';
		}

		if (p[0] == '\0')
			return (-1);

		xfer = transfer_find(acct, in->tokens[0], p);
		if (xfer == NULL) {
			debug("unknown transfer %s/%s", in->tokens[0], p);
			return (-1);
		}

		xfer->protocol_flags |= IRC_DCC_REJECTED;
		if (transfer_cancel_remote(xfer) == 0)
			return (1);

		return (-1);
	}

	return (-1);
}
