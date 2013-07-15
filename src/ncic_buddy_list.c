/*
** Based on pork_buddy_list.c
** ncic_buddy_list.c - Buddy list screen widget.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
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
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ncic.h"
//#include "ncic_missing.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_buddy.h"
#include "ncic_imwindow.h"
#include "ncic_slist2.h"
#include "ncic_buddy_list.h"
#include "ncic_cstr.h"
#include "ncic_misc.h"
#include "ncic_status.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_format.h"
#include "ncic_color.h"
#include "ncic_acct.h"

static void blist_cell_destroy_cb(void *d) {
	struct slist_cell *cell = d;

	if (cell->type == TYPE_LIST_CELL) {
		struct bgroup *group = cell->data;

		group->blist_line = NULL;
	} else {
		struct buddy *buddy = cell->data;

		buddy->blist_line = NULL;
	}
}

static void blist_make_buddy_label(struct buddy *buddy, chtype *ch, size_t len)
{
	char buf[1024];
	char *string = buf;

	if (fill_format_str(OPT_FORMAT_BLIST, buf, sizeof(buf), buddy) == -1)
		string = "ERROR";

	format_apply_justification(buf, ch, len);
}

static void blist_make_group_label(struct bgroup *group, chtype *ch, size_t len)
{
	char buf[1024];
	char *string = buf;

	if (fill_format_str(OPT_FORMAT_BLIST_GROUP, buf, sizeof(buf), group) == -1)
		string = "ERROR";

	format_apply_justification(buf, ch, len);
}

void
blist_update_label(struct blist *blist, struct slist_cell *cell)
{
	if (blist == NULL || cell == NULL)
		return;

	if (cell->type == TYPE_FLAT_CELL) {
		struct buddy *buddy = cell->data;

		blist_make_buddy_label(buddy, cell->label, blist->label_len);
	} else {
		struct bgroup *group = cell->data;

		blist_make_group_label(group, cell->label, blist->label_len);
	}

	if (cell->line >= 0) {
		if (cell == blist->slist.cursor)
			blist_draw_cursor(blist, 1);
		else
			blist_draw_line(blist, cell);
	}
}

int blist_init(struct pork_acct *acct) {
	int cols = DEFAULT_BLIST_WIDTH;
	struct blist *blist;
	int ret;

	if (acct == NULL || acct->blist != NULL)
		return (-1);

	blist = xcalloc(1, sizeof(*blist));

	ret = slist_init(&blist->slist, screen.rows - STATUS_ROWS, cols,
			screen.cols - cols, 0);
	if (ret != 0) {
		free(blist);
		return (-1);
	}

	slist_cell_free_cb(&blist->slist, blist_cell_destroy_cb);

	blist->label_len = SLIST_LAST_COL(&blist->slist) + 1;

	blist_draw(blist);
	acct->blist = blist;
	return (0);
}

struct slist_cell *
blist_add_group(struct blist *blist, struct bgroup *group) {
	struct slist_cell *cell;
	struct slist_cell *ret;

	if (blist == NULL)
		return (NULL);

	cell = xcalloc(1, sizeof(*cell));
	cell->type = TYPE_LIST_CELL;
	cell->parent = NULL;
	cell->children = NULL;
	cell->collapsed = 0;
	cell->refnum = group->refnum;
	cell->label = xmalloc(sizeof(chtype) * blist->label_len);
	cell->data = group;
	blist_make_group_label(group, cell->label, blist->label_len);

	ret = slist_add(&blist->slist, cell, 0);

	if (ret == NULL)
		return (NULL);

	group->blist_line = ret;
	blist_draw(blist);
	return (ret->data);
}

struct slist_cell *
blist_add(struct blist *blist, struct buddy *buddy) {
	struct slist_cell *cell;
	struct slist_cell *ret;

	if (blist == NULL)
		return (NULL);

	cell = xcalloc(1, sizeof(*cell));
	cell->type = TYPE_FLAT_CELL;
	cell->parent = buddy->group->blist_line;
	cell->refnum = buddy->refnum;
	cell->label = xmalloc(sizeof(chtype) * blist->label_len);
	cell->data = buddy;
	blist_make_buddy_label(buddy, cell->label, blist->label_len);

	ret = slist_add(&blist->slist, cell, buddy->id + 1);
	if (ret == NULL)
		return (NULL);

	cell = buddy->group->blist_line;

	buddy->blist_line = ret;
	blist_make_group_label(buddy->group, cell->label, blist->label_len);
	blist_draw(blist);
	return (ret);
}

void blist_del(struct blist *blist, struct buddy *buddy) {
	struct slist_cell *cell;

	if (blist == NULL)
		return;

	cell = buddy->blist_line;
	if (cell == NULL)
		return;
	buddy->blist_line = NULL;
	slist_del(&blist->slist, cell);

	cell = buddy->group->blist_line;
	blist_make_group_label(buddy->group, cell->label, blist->label_len);

	blist_draw(blist);
}

void blist_del_group(struct blist *blist, struct bgroup *group) {
	struct slist_cell *cell;

	if (blist == NULL)
		return;

	cell = group->blist_line;
	if (cell == NULL)
		return;
	group->blist_line = NULL;

	slist_del(&blist->slist, cell);
	blist_draw(blist);
}

int blist_cursor_up(struct blist *blist) {
	int ret;

	if (blist == NULL)
		return (-1);

	blist_draw_cursor(blist, 0);
	ret = slist_cursor_up(&blist->slist);
	blist_draw_cursor(blist, 1);

	return (ret);
}

int blist_cursor_down(struct blist *blist) {
	int ret;

	if (blist == NULL)
		return (-1);

	blist_draw_cursor(blist, 0);
	ret = slist_cursor_down(&blist->slist);
	blist_draw_cursor(blist, 1);

	return (ret);
}

int blist_cursor_start(struct blist *blist) {
	int ret;

	if (blist == NULL)
		return (-1);

	blist_draw_cursor(blist, 0);
	ret = slist_cursor_start(&blist->slist);
	blist_draw_cursor(blist, 1);

	return (ret);
}

int blist_cursor_end(struct blist *blist) {
	int ret;

	if (blist == NULL)
		return (-1);

	blist_draw_cursor(blist, 0);
	ret = slist_cursor_end(&blist->slist);
	blist_draw_cursor(blist, 1);

	return (ret);
}

int blist_cursor_pgdown(struct blist *blist) {
	int ret;

	if (blist == NULL)
		return (-1);

	blist_draw_cursor(blist, 0);
	ret = slist_cursor_pgdown(&blist->slist);
	blist_draw_cursor(blist, 1);

	return (ret);
}

int blist_cursor_pgup(struct blist *blist) {
	int ret;

	if (blist == NULL)
		return (-1);

	blist_draw_cursor(blist, 0);
	ret = slist_cursor_pgup(&blist->slist);
	blist_draw_cursor(blist, 1);

	return (ret);
}

inline struct slist_cell *blist_get_cursor(struct blist *blist) {
	if (blist == NULL)
		return (NULL);

	return (slist_get_cursor(&blist->slist));
}

int blist_resize(	struct blist *blist,
					u_int32_t rows,
					u_int32_t cols,
					u_int32_t screen_cols)
{
	int ret;
	int needs_update = 0;

	if (blist == NULL)
		return (0);

	if (blist->slist.cols != cols)
		needs_update = 1;

	ret = slist_resize(&blist->slist, rows, cols, screen_cols);
	if (ret != 0)
		return (-1);

	blist->label_len = SLIST_LAST_COL(&blist->slist) + 1;

	if (needs_update)
		blist_changed_width(blist);

	return (0);
}

inline void blist_destroy(struct blist *blist) {
	if (blist == NULL)
		return;

	slist_destroy(&blist->slist);
	memset(blist, 0, sizeof(*blist));
}

inline void blist_draw(struct blist *blist) {
	slist_draw(&blist->slist);
	blist_draw_cursor(blist, 1);
}

inline int blist_refresh(struct blist *blist) {
	return (slist_refresh(&blist->slist));
}

void blist_draw_border(struct blist *blist, int border_state) {
	attr_t border_attr;

	if (border_state == 0)
		border_attr = opt_get_color(OPT_COLOR_BLIST_NOFOCUS);
	else
		border_attr = opt_get_color(OPT_COLOR_BLIST_FOCUS);

	slist_clear_bot(&blist->slist);

	wattrset(blist->slist.win, border_attr);
	box(blist->slist.win, ACS_VLINE, ACS_HLINE);
	wattrset(blist->slist.win, 0);
}

inline void blist_draw_cursor(struct blist *blist, int status) {
	attr_t curs_attr = 0;

	if (status)
		curs_attr = opt_get_color(OPT_COLOR_BLIST_SELECTOR);

	slist_draw_cursor(&blist->slist, curs_attr);
}

inline void blist_draw_line(struct blist *blist, struct slist_cell *cell) {
	slist_draw_line(&blist->slist, cell);
}

void
blist_changed_format(struct blist *blist)
{
	int i;
	struct slist_cell *cur;

	for (i = 0; i < blist->slist.n_cells; i++) {
		cur = blist->slist.cell_list[i];
		if (cur == NULL)
			continue;

		blist_update_label(blist, cur);
	}
}

void
blist_changed_width(struct blist *blist)
{
	int i;
	struct slist_cell *cur;

	for (i = 0; i < blist->slist.n_cells; i++) {
		struct slist_cell *cell;
		cur = blist->slist.cell_list[i];
		if (cur == NULL)
			continue;

		cell = cur;
		free(cell->label);
		cell->label = xmalloc(sizeof(chtype) * blist->label_len);

		blist_update_label(blist, cur);
	}
}
