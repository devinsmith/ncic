/*
** Based on pork_screen.c
** ncic_screen.c - screen management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <unistd.h>
#include <ncurses.h>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_misc.h"
#include "ncic_imwindow.h"
#include "ncic_proto.h"
#include "ncic_acct.h"
#include "ncic_chat.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_io.h"
#include "ncic_status.h"
#include "ncic_log.h"

/*
** Find the window having the specified refnum, and return
** a pointer to the node that holds it.
**
** This depends on the list being sorted.
*/

static dlist_t *screen_find_refnum(u_int32_t refnum) {
	dlist_t *cur = screen.window_list;

	do {
		struct imwindow *imwindow = (struct imwindow *)cur->data;

		if (imwindow->refnum == refnum)
			return (cur);

		if (imwindow->refnum > refnum)
			break;

		cur = cur->next;
	} while (cur != screen.window_list);

	return (nullptr);
}

static void screen_window_list_add(dlist_t *new_node) {
	struct imwindow *imwindow = (struct imwindow *)new_node->data;

	/*
	** The window list is a sorted circular doubly linked list.
	** The window_list pointer points to the window having the lowest
	** refnum.
	*/

	if (screen.window_list == nullptr) {
		new_node->prev = new_node;
		new_node->next = new_node;
		screen.window_list = new_node;
	} else {
		dlist_t *cur = screen.window_list;

		do {
			struct imwindow *imw = (struct imwindow *)cur->data;

			if (imwindow->refnum < imw->refnum) {
				if (cur == screen.window_list)
					screen.window_list = new_node;
				break;
			}

			cur = cur->next;
		} while (cur != screen.window_list);

		new_node->next = cur;
		new_node->prev = cur->prev;

		new_node->next->prev = new_node;
		new_node->prev->next = new_node;
	}
}

static void screen_window_list_remove(dlist_t *node) {
	dlist_t *save = node;

	if (node == screen.window_list)
		screen.window_list = node->next;

	if (node == screen.window_list)
		screen.window_list = nullptr;

	save->prev->next = node->next;
	save->next->prev = node->prev;
}

/*
** Create the program's status window. This window will have the buddy
** list visible in it by default.
*/

int screen_init(int rows, int cols) {
	struct imwindow *imwindow;
	struct pork_acct *acct;

	memset(&screen, 0, sizeof(screen));

	screen.rows = rows;
	screen.cols = cols;

	bind_init(&screen.binds);
	input_init(&screen.input, cols);

	if (status_init() == -1)
		return (-1);

  log_tmsg(0, "Setting up NULL account");

	acct = pork_acct_init(opt_get_str(OPT_TEXT_NO_NAME), PROTO_NULL);
	if (acct == nullptr)
		return (-1);
	acct->refnum = 0;

	screen.null_acct = acct;

  IoManager::instance().add(STDIN_FILENO, IO_COND_READ, &screen, &screen,
		keyboard_input);

	rows = std::max(1, (int) rows - STATUS_ROWS);

	imwindow = imwindow_new(rows, cols, 1, WIN_TYPE_STATUS, acct, "Main");
	if (imwindow == nullptr)
		return (-1);

	screen_add_window(imwindow);
	screen.status_win = imwindow;
	return (0);
}

void screen_destroy(void) {
	dlist_t *cur = screen.window_list;

	do {
		dlist_t *next = cur->next;

		imwindow_destroy((imwindow *)cur->data);
		free(cur);

		cur = next;
	} while (cur != screen.window_list);

	opt_destroy();
	input_destroy(&screen.input);
	bind_destroy(&screen.binds);
	hash_destroy(&screen.alias_hash);
	delwin(screen.status_bar);
	wclear(curscr);
}

void screen_add_window(struct imwindow *imwindow) {
	dlist_t *new_node = (dlist_t *)xmalloc(sizeof(*new_node));

	new_node->data = imwindow;
	screen_window_list_add(new_node);

	/*
	** If this is the first window, make it current.
	*/

	if (screen.cur_window == nullptr)
		screen_window_swap(new_node);
}

void screen_resize(u_int32_t rows, u_int32_t cols) {
	dlist_t *cur;
	int ret;

	resize_terminal(rows, cols);

	screen.rows = rows;
	screen.cols = cols;

	cur = screen.window_list;
	do {
		struct imwindow *imwindow = (struct imwindow *)cur->data;
		u_int32_t im_cols = cols;

		imwindow_resize(imwindow,
			std::max(1, (int) rows - STATUS_ROWS), im_cols);
		input_resize(imwindow->input, cols);

		cur = cur->next;
	} while (screen.window_list != cur);

	ret = mvwin(screen.status_bar, std::max(0, (int) rows - STATUS_ROWS), 0);
	if (ret == -1) {
		delwin(screen.status_bar);
		status_init();
	}
}

void screen_window_swap(dlist_t *new_cur) {
	struct imwindow *imwindow;
	u_int32_t last_own_input = 0;
	u_int32_t cur_own_input;

	if (screen.cur_window != nullptr) {
		imwindow = cur_window();

		last_own_input = wopt_get_bool(imwindow->opts, WOPT_PRIVATE_INPUT);
		imwindow->swindow.activity = 0;
		imwindow->swindow.visible = 0;
	}

	screen.cur_window = new_cur;

	imwindow = cur_window();
	imwindow->swindow.visible = 1;
	imwindow->swindow.activity = 0;
	cur_own_input = wopt_get_bool(imwindow->opts, WOPT_PRIVATE_INPUT);

	/*
	** If the current window and the one that we're going to switch
	** to don't share an input buffer, history, etc, the input
	** line must be refreshed. Force a redraw.
	*/

	if (cur_own_input || last_own_input) {
		imwindow->input->dirty = 1;
		screen_draw_input();
	}

	status_draw(imwindow->owner);

	/*
	** To force ncurses to redraw it on the physical screen.
	*/
	redrawwin(imwindow->swindow.win);
	imwindow->swindow.dirty = 1;
}

int screen_goto_window(u_int32_t refnum) {
	dlist_t *cur = screen_find_refnum(refnum);

	if (cur == nullptr)
		return (-1);

	screen_window_swap(cur);
	return (0);
}

void screen_refresh(void) {
	struct imwindow *imwindow = cur_window();

	wclear(curscr);
	wnoutrefresh(curscr);
	wclear(imwindow->swindow.win);
	wnoutrefresh(imwindow->swindow.win);
	swindow_redraw(&imwindow->swindow);
	status_draw(imwindow->owner);
	imwindow_refresh(imwindow);
	imwindow->input->dirty = 1;
	screen_draw_input();
	screen_doupdate();
}

/*
** When we really want a query window.
*/

void screen_cycle_fwd(void) {
	dlist_t *cur = screen.cur_window;
	struct imwindow *win;

	do {
		cur = cur->next;
		win = (imwindow *)cur->data;
	} while (win->skip && cur != screen.cur_window);

	screen_window_swap(cur);
}

void screen_cycle_bak(void) {
	dlist_t *cur = screen.cur_window;
	struct imwindow *win;

	do {
		cur = cur->prev;
		win = (imwindow *)cur->data;
	} while (win->skip && cur != screen.cur_window);

	screen_window_swap(cur);
}

void screen_bind_all_unbound() {
	dlist_t *node;

	node = screen.window_list;

	do {
		struct imwindow *imwindow = (struct imwindow *)node->data;

		if (imwindow->owner == screen.null_acct) {
			imwindow_bind_acct(imwindow);
    }

		node = node->next;
	} while (node != screen.window_list);
}

int screen_close_window(struct imwindow *imwindow) {
	dlist_t *node = screen_find_refnum(imwindow->refnum);

	if (node == nullptr)
		return (-1);

	/*
	** The status window can't be closed.
	*/

	if (imwindow->type == WIN_TYPE_STATUS)
		return (-1);

	if (imwindow->type == WIN_TYPE_CHAT && imwindow->data != NULL) {
		struct chatroom *chat = (struct chatroom *)imwindow->data;

		chat_leave(imwindow->owner, chat->title, 0);
	}

	if (imwindow->swindow.visible)
		screen_cycle_fwd();

	screen_window_list_remove(node);
	imwindow_destroy(imwindow);
	free(node);

	return (0);
}
