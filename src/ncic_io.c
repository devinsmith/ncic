/*
** Based on pork_io.c
** ncic_io.c - I/O handler.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_io.h"
#include "ncic_inet.h"

static dlist_t *io_list;

static int pork_io_find_cb(void *l, void *r) {
	struct io_source *io = (struct io_source *) r;

	return (!(l == io->key));
}

static void pork_io_destroy_cb(void *param __notused, void *data) {
	free(data);
}

static void pork_io_remove(dlist_t *node) {
	pork_io_destroy_cb(NULL, node->data);
	io_list = dlist_remove(io_list, node);
}

int pork_io_init(void) {
	io_list = NULL;
	return (0);
}

void pork_io_destroy(void) {
	dlist_destroy(io_list, NULL, pork_io_destroy_cb);
	io_list = NULL;
}

int pork_io_add(int fd,
				u_int32_t cond,
				void *data,
				void *key,
				void (*callback)(int fd, u_int32_t cond, void *data))
{
	dlist_t *node;
	struct io_source *io;

	/*
	** If there's already an entry for this key, delete it
	** and replace it with the new one.
	*/

	node = dlist_find(io_list, key, pork_io_find_cb);
	if (node != NULL)
		pork_io_remove(node);

	io = xcalloc(1, sizeof(*io));
	io->fd = fd;
	io->cond = cond;
	io->data = data;
	io->key = key;
	io->callback = callback;

	io_list = dlist_add_head(io_list, io);
	return (0);
}

int pork_io_del(void *key) {
	dlist_t *node;
	struct io_source *io;

	node = dlist_find(io_list, key, pork_io_find_cb);
	if (node == NULL)
		return (-1);

	io = node->data;
	io->fd = -2;
	io->callback = NULL;
	return (0);
}

int pork_io_dead(void *key) {
	dlist_t *node;

	node = dlist_find(io_list, key, pork_io_find_cb);
	if (node == NULL)
		return (-1);

	((struct io_source *) node->data)->fd = -1;
	return (0);
}

int pork_io_set_cond(void *key, u_int32_t new_cond) {
	dlist_t *node;

	node = dlist_find(io_list, key, pork_io_find_cb);
	if (node == NULL)
		return (-1);

	((struct io_source *) node->data)->cond = new_cond;
	return (0);
}

int pork_io_add_cond(void *key, u_int32_t new_cond) {
	dlist_t *node;

	node = dlist_find(io_list, key, pork_io_find_cb);
	if (node == NULL)
		return (-1);

	((struct io_source *) node->data)->cond |= new_cond;
	return (0);
}

int pork_io_del_cond(void *key, u_int32_t new_cond) {
	dlist_t *node;

	node = dlist_find(io_list, key, pork_io_find_cb);
	if (node == NULL)
		return (-1);

	((struct io_source *) node->data)->cond &= ~new_cond;
	return (0);
}

static int pork_io_find_dead_fds(dlist_t *io_list) {
	dlist_t *cur = io_list;
	int bad_fd = 0;

	while (cur != NULL) {
		struct io_source *io = cur->data;
		dlist_t *next = cur->next;

		if (io->fd < 0 || sock_is_error(io->fd)) {
			debug("fd %d is dead", io->fd);
			if (io->callback != NULL)
				io->callback(io->fd, IO_COND_DEAD, io->data);

			pork_io_remove(cur);
			bad_fd++;
		}

		cur = next;
	}

	return (bad_fd);
}

int pork_io_run(void) {
	fd_set rfds;
	fd_set wfds;
	fd_set xfds;
	int max_fd = -1;
	int ret;
	struct timeval tv = { 0, 600000 };
	dlist_t *cur;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&xfds);

	cur = io_list;
	while (cur != NULL) {
		struct io_source *io = cur->data;
		dlist_t *next = cur->next;

		if (io->fd >= 0) {
			if (io->cond & IO_COND_ALWAYS && io->callback != NULL)
				io->callback(io->fd, IO_COND_ALWAYS, io->data);

			if (io->cond & IO_COND_READ)
				FD_SET(io->fd, &rfds);

			if (io->cond & IO_COND_WRITE)
				FD_SET(io->fd, &wfds);

			if (io->cond & IO_COND_EXCEPTION)
				FD_SET(io->fd, &xfds);

			if (io->fd > max_fd)
				max_fd = io->fd;
		} else {
			if (io->callback != NULL)
				io->callback(io->fd, IO_COND_DEAD, io->data);

			pork_io_remove(cur);
		}

		cur = next;
	}

	if (max_fd < 0)
		return (-1);

	/*
	** If there's a bad fd in the set better find it, otherwise
	** we're going to get into an infinite loop.
	*/
	ret = select(max_fd + 1, &rfds, &wfds, &xfds, &tv);
	if (ret < 1) {
		if (ret == -1 && errno == EBADF)
			pork_io_find_dead_fds(io_list);

		return (ret);
	}

	cur = io_list;
	while (cur != NULL) {
		struct io_source *io = cur->data;
		dlist_t *next = cur->next;

		if (io->fd >= 0) {
			u_int32_t cond = 0;

			if ((io->cond & IO_COND_READ) && FD_ISSET(io->fd, &rfds))
				cond |= IO_COND_READ;

			if ((io->cond & IO_COND_WRITE) && FD_ISSET(io->fd, &wfds))
				cond |= IO_COND_WRITE;

			if ((io->cond & IO_COND_EXCEPTION) && FD_ISSET(io->fd, &xfds))
				cond |= IO_COND_EXCEPTION;

			if (cond != 0 && io->callback != NULL)
				io->callback(io->fd, cond, io->data);
		}

		cur = next;
	}

	return (ret);
}
