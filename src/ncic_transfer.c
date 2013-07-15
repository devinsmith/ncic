/*
** Based on ncic_transfer.c
** ncic_transfer.c
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
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "ncic.h"
//#include "ncic_missing.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_inet.h"
#include "ncic_acct.h"
#include "ncic_io.h"
#include "ncic_imsg.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_set.h"
#include "ncic_proto.h"
#include "ncic_format.h"
#include "ncic_transfer.h"

static u_int32_t next_xfer_refnum;

static dlist_t *transfer_find_node_refnum(	struct pork_acct *acct,
											u_int32_t refnum)
{
	dlist_t *cur = acct->transfer_list;

	while (cur != NULL) {
		struct file_transfer *xfer = cur->data;

		if (xfer->refnum == refnum)
			return (cur);

		cur = cur->next;
	}

	return (NULL);
}

struct file_transfer *transfer_new(	struct pork_acct *acct,
									char *peer,
									int direction,
									char *filename,
									size_t size)
{
	struct file_transfer *xfer;
	char *p;

	xfer = xcalloc(1, sizeof(*xfer));
	xfer->peer_username = xstrdup(peer);
	xfer->type = direction;
	xfer->acct = acct;
	xfer->sock = -1;
	xfer->start_offset = 0;
	xfer->fname_orig = xstrdup(filename);
	xfer->fname_local = xstrdup(filename);
	xfer->file_len = size;
	xfer->status = TRANSFER_STATUS_WAITING;
	xfer->refnum = next_xfer_refnum++;

	p = xstrdup(filename);
	xfer->fname_base = xstrdup(basename(p));

	if (strchr(xfer->fname_base, ' ') != NULL)
		xfer->quote_fname = 1;

	acct->transfer_list = dlist_add_head(acct->transfer_list, xfer);

	free(p);
	return (xfer);
}

int transfer_bind_listen_sock(struct file_transfer *xfer, int sock) {
	in_port_t lport;
	in_port_t lport_max;
	int temp;
	int listenfd;

	if (get_local_addr(sock, &xfer->laddr) != 0)
		return (-1);

	temp = opt_get_int(OPT_TRANSFER_PORT_MIN);
	if (temp < 1024 || temp > 0xffff)
		temp = 1024;
	lport = temp;

	temp = opt_get_int(OPT_TRANSFER_PORT_MAX);
	if (temp < lport)
		temp = lport + 500;
	lport_max = temp;

	do {
		listenfd = sock_listen(&xfer->laddr, lport);
		if (listenfd >= 0)
			break;
		lport++;
	} while (lport < lport_max);

	if (listenfd < 0)
		return (-1);

	xfer->lport = lport;
	xfer->sock = listenfd;
	get_ip(&xfer->laddr, xfer->laddr_ip, sizeof(xfer->laddr_ip));
	return (0);
}

int transfer_resume(struct file_transfer *xfer, off_t offset) {
	int ret;

	if (xfer->fp == NULL)
		return (-1);

	if (offset < 0 || offset >= xfer->file_len)
		return (-1);

	xfer->start_offset = offset;

	ret = fseek(xfer->fp, offset, SEEK_SET);
	if (ret < 0)
		debug("fseek: %lld %s", offset, strerror(errno));

	return (ret);
}

inline struct file_transfer *transfer_find_refnum(	struct pork_acct *acct,
													u_int32_t refnum)
{
	dlist_t *node;

	node = transfer_find_node_refnum(acct, refnum);
	if (node != NULL)
		return (node->data);

	return (NULL);
}

void transfer_recv_data(int fd __notused, u_int32_t cond __notused, void *data)
{
	int ret;
	int written;
	char buf[4096];
	struct file_transfer *xfer = data;

	ret = read(xfer->sock, buf, sizeof(buf));
	if (ret < 1) {
		transfer_lost(xfer);
		return;
	}

	written = fwrite(buf, 1, (size_t) ret, xfer->fp);
	if (written != ret) {
		screen_err_msg("Error writing file %s: %s",
			xfer->fname_local, strerror(errno));
		transfer_lost(xfer);
		return;
	}

	if (xfer->bytes_sent == 0)
		gettimeofday(&xfer->time_started, NULL);

	xfer->bytes_sent += written;
	if (xfer->acct->proto->file_recv_data != NULL)
		xfer->acct->proto->file_recv_data(xfer, buf, ret);
}

void transfer_send_data(int fd __notused, u_int32_t cond __notused, void *data)
{
	int ret;
	int sent;
	char buf[4096];
	struct file_transfer *xfer = data;

	ret = fread(buf, 1, sizeof(buf), xfer->fp);
	if (ret < 0) {
		debug("fread: %s", strerror(errno));
		transfer_lost(xfer);
		return;
	}

	if (xfer->bytes_sent == 0)
		gettimeofday(&xfer->time_started, NULL);

	sent = sock_write(xfer->sock, buf, ret);
	if (sent != ret) {
		screen_err_msg("Error sending data for file %s: %s",
			xfer->fname_local, strerror(errno));
		transfer_lost(xfer);
		return;
	}

	xfer->bytes_sent += sent;
	if (xfer->bytes_sent + xfer->start_offset >= xfer->file_len)
		xfer->status = TRANSFER_STATUS_COMPLETE;

	if (xfer->acct->proto->file_send_data != NULL)
		xfer->acct->proto->file_send_data(xfer, buf, ret);
}

double transfer_time_elapsed(struct file_transfer *xfer) {
	struct timeval time_now;
	double start_time;
	double end_time;

	gettimeofday(&time_now, NULL);

	end_time = time_now.tv_sec + (double) time_now.tv_usec / 1000000;
	start_time = xfer->time_started.tv_sec + (double) xfer->time_started.tv_usec / 1000000;

	return (end_time - start_time);
}

inline double transfer_avg_speed(struct file_transfer *xfer) {
	return ((xfer->bytes_sent >> 10) / transfer_time_elapsed(xfer));
}

int transfer_recv_complete(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	if (xfer->acct->proto->file_recv_complete != NULL &&
		xfer->acct->proto->file_recv_complete(xfer) == -1)
	{
		screen_err_msg(
			"There was an error finishing transfer refnum %u for %s from %s",
			xfer->refnum, xfer->fname_local, xfer->peer_username);

		transfer_abort(xfer);
		return (-1);
	}

	ret = fill_format_str(OPT_FORMAT_FILE_RECV_COMPLETE, buf,
			sizeof(buf), xfer);
	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_FIN);
	}

	return (transfer_destroy(xfer));
}

int transfer_send_complete(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	if (xfer->acct->proto->file_send_complete != NULL &&
		xfer->acct->proto->file_send_complete(xfer) == -1)
	{
		screen_err_msg(
			"There was an error finishing transfer refnum %u for %s to %s",
			xfer->refnum, xfer->fname_local, xfer->peer_username);

		transfer_abort(xfer);
		return (-1);
	}

	ret = fill_format_str(OPT_FORMAT_FILE_SEND_COMPLETE, buf, sizeof(buf),
			xfer);
	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_FIN);
	}

	return (transfer_destroy(xfer));
}

static int transfer_find_filename(struct file_transfer *xfer, char *filename) {
	char buf[4096];
	int ret;
	int fd;
	FILE *fp;
	int tries = 0;
	char *fname_orig;

	if (filename == NULL) {
		size_t offset = 0;
		char *download_dir = opt_get_str(OPT_DOWNLOAD_DIR);
		char *p;

		/*
		** We're going to accept the filename given by our peer.
		*/
		filename = xfer->fname_orig;
		fname_orig = xfer->fname_orig;

		if (download_dir != NULL && !blank_str(download_dir)) {
			ret = xstrncpy(buf, download_dir, sizeof(buf) - 1);
			if (ret == -1)
				return (-1);

			buf[ret] = '/';
			offset = ret + 1;
		}

		ret = xstrncpy(&buf[offset], filename, sizeof(buf) - offset);
		if (ret == -1)
			return (-1);

		/*
		** Don't let people play tricks with '.' and '/' characters in
		** file names.
		*/
		p = &buf[offset];
		if (p[0] == '.')
			p[0] = '_';

		while ((p = strchr(p, '/')) != NULL)
			*p = '_';
	} else {
		/*
		** The user requested a particular name for the file. Use it.
		*/
		ret = expand_path(filename, buf, sizeof(buf));
		fname_orig = filename;
	}

	fd = open(buf, O_WRONLY | O_EXCL | O_CREAT, 0600);
	while (fd == -1 && errno == EEXIST && tries < 300) {
		if (xstrncat(buf, "_", sizeof(buf)) == -1)
			return (-EEXIST);

		fd = open(buf, O_WRONLY | O_EXCL | O_CREAT, 0600);
		tries++;
	}

	if (fd == -1)
		return (-1);

	if (tries >= 300)
		return (-EEXIST);

	if (tries > 0) {
		screen_err_msg("%s already exists -- using %s instead",
			fname_orig, buf);
	}

	fp = fdopen(fd, "w");
	if (fp == NULL) {
		unlink(buf);
		close(fd);
		return (-1);
	}

	free(xfer->fname_local);
	xfer->fname_local = xstrdup(buf);
	xfer->fp = fp;

	return (0);
}

int transfer_get(struct file_transfer *xfer, char *filename) {
	int ret;

	if (transfer_find_filename(xfer, filename) == -1) {
		screen_err_msg("Error finding a suitable filename. Specify one explictly");
		return (-1);
	}

	ret = xfer->acct->proto->file_accept(xfer);
	if (ret == -1) {
		screen_err_msg("Error accepting %s", xfer->fname_local);
		transfer_lost(xfer);
		return (-1);
	}

	xfer->status = TRANSFER_STATUS_ACTIVE;
	return (0);
}

u_int32_t transfer_get_all(struct pork_acct *acct) {
	dlist_t *cur;
	u_int32_t successes = 0;

	cur = acct->transfer_list;
	while (cur != NULL) {
		struct file_transfer *xfer = cur->data;
		dlist_t *next = cur->next;

		if (xfer->type == TRANSFER_DIR_RECV &&
			xfer->status == TRANSFER_STATUS_WAITING)
		{
			if (transfer_get(xfer, xfer->fname_local) != -1)
				successes++;
		}

		cur = next;
	}

	return (successes);
}

u_int32_t transfer_abort_all(struct pork_acct *acct, u_int32_t type) {
	dlist_t *cur;
	u_int32_t successes = 0;

	cur = acct->transfer_list;
	while (cur != NULL) {
		struct file_transfer *xfer = cur->data;
		dlist_t *next = cur->next;

		if (type == TRANSFER_DIR_ANY || xfer->type == type) {
			transfer_abort(xfer);
			successes++;
		}
		cur = next;
	}

	return (successes);
}

struct file_transfer *transfer_find(struct pork_acct *acct,
									char *peer,
									char *file)
{
	dlist_t *cur;

	for (cur = acct->transfer_list ; cur != NULL ; cur = cur->next) {
		struct file_transfer *xfer = cur->data;

		if (!acct->proto->user_compare(peer, xfer->peer_username) &&
			!strcmp(file, xfer->fname_base))
		{
			return (xfer);
		}
	}

	return (NULL);
}

int transfer_send(struct pork_acct *acct, char *dest, char *filename) {
	FILE *fp;
	size_t len;
	struct file_transfer *xfer;
	char buf[4096];

	if (acct->proto->file_send == NULL)
		return (-1);

	if (expand_path(filename, buf, sizeof(buf)) == -1) {
		screen_err_msg("Error: The path is too long");
		return (-1);
	}

	fp = fopen(buf, "r");
	if (fp == NULL) {
		debug("fopen: %s: %s", buf, strerror(errno));
		screen_err_msg("Error: Cannot open %s for reading", buf);
		return (-1);
	}

	if (file_get_size(fp, &len) != 0) {
		screen_err_msg("Error: Cannot determine the size of %s", buf);
		fclose(fp);
		return (-1);
	}

	xfer = transfer_new(acct, dest, TRANSFER_DIR_SEND, buf, len);
	if (xfer == NULL) {
		screen_err_msg("Error sending %s to %s -- aborting", buf, dest);
		fclose(fp);
		return (-1);
	}

	xfer->fp = fp;

	if (acct->proto->file_send(xfer) == -1) {
		transfer_lost(xfer);
		return (-1);
	}

	return (0);
}

int transfer_abort(struct file_transfer *xfer) {
	int ret = 0;

	if (xfer->acct->proto->file_abort != NULL)
		ret = xfer->acct->proto->file_abort(xfer);

	transfer_destroy(xfer);
	return (ret);
}

int transfer_destroy(struct file_transfer *xfer) {
	struct pork_acct *acct = xfer->acct;
	dlist_t *node;

	node = transfer_find_node_refnum(acct, xfer->refnum);
	if (node == NULL)
		return (-1);

	acct->transfer_list = dlist_remove(acct->transfer_list, node);

	free(xfer->peer_username);
	free(xfer->fname_orig);
	free(xfer->fname_local);
	free(xfer->fname_base);

	if (xfer->fp != NULL)
		fclose(xfer->fp);

	free(xfer);
	return (0);
}

inline char *transfer_get_local_hostname(struct file_transfer *xfer) {
	if (xfer->laddr_host[0] == '\0') {
		get_hostname(&xfer->laddr,
			xfer->laddr_host, sizeof(xfer->laddr_host));
	}

	return (xfer->laddr_host);
}

inline char *transfer_get_remote_hostname(struct file_transfer *xfer) {
	if (xfer->faddr_host[0] == '\0') {
		get_hostname(&xfer->faddr,
			xfer->faddr_host, sizeof(xfer->faddr_host));
	}

	return (xfer->laddr_host);
}

int transfer_recv_accepted(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	if (xfer->start_offset != 0) {
		ret = fill_format_str(OPT_FORMAT_FILE_RECV_RESUME, buf, sizeof(buf),
				xfer);
	} else {
		ret = fill_format_str(OPT_FORMAT_FILE_RECV_ACCEPT, buf, sizeof(buf),
				xfer);
	}

	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_STATUS);
	}

	return (0);
}

int transfer_request_recv(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_FILE_RECV_ASK, buf, sizeof(buf), xfer);
	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_STATUS);
	}

	return (0);
}

int transfer_request_send(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_FILE_SEND_ASK, buf, sizeof(buf), xfer);
	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_STATUS);
	}

	return (0);
}

int transfer_send_accepted(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	if (xfer->start_offset != 0) {
		ret = fill_format_str(OPT_FORMAT_FILE_SEND_RESUME, buf, sizeof(buf),
				xfer);
	} else {
		ret = fill_format_str(OPT_FORMAT_FILE_SEND_ACCEPT, buf, sizeof(buf),
				xfer);
	}

	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_START);
	}

	xfer->status = TRANSFER_STATUS_ACTIVE;
	return (0);
}

int transfer_lost(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_FILE_LOST, buf, sizeof(buf), xfer);
	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_STATUS);
	}

	return (transfer_abort(xfer));
}

int transfer_cancel_local(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_FILE_CANCEL_LOCAL, buf, sizeof(buf), xfer);
	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_FIN);
	}

	return (transfer_abort(xfer));
}

int transfer_cancel_remote(struct file_transfer *xfer) {
	char buf[4096];
	int ret;

	ret = fill_format_str(OPT_FORMAT_FILE_CANCEL_REMOTE,
		buf, sizeof(buf), xfer);
	if (ret > 0) {
		screen_print_str(cur_window(), buf, (size_t) ret,
			MSG_TYPE_FILE_XFER_FIN);
	}

	return (transfer_abort(xfer));
}

char *transfer_status_str(struct file_transfer *xfer) {
	static char *file_status[] = { "WAITING", "ACTIVE", "COMPLETE" };

	return (file_status[xfer->status]);
}
