/*
** Heavily based on pork_slist.c
** ncic_slist.c - an ncurses scrolling list widget
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_cstr.h"
#include "ncic_misc.h"
#include "ncic_slist2.h"

static void slist_renumber(struct slist *slist, int start_line);

static void
slist_destroy_cell(struct slist *slist, struct slist_cell *cell)
{
	free(cell->label);

	if (slist->cell_free_cb != NULL)
		slist->cell_free_cb(cell);

	free(cell);
}

static void
slist_scroll_screen_up(struct slist *slist)
{
	struct slist_cell *cell;

	scrollok(slist->win, TRUE);
	wscrl(slist->win, -1);
	scrollok(slist->win, FALSE);

	if (slist->has_border) {
		cell = slist->cell_list[slist->bot_line];
		if (cell->line != SLIST_LAST_ROW(slist)) {
			wmove(slist->win, SLIST_LAST_ROW(slist) + slist->has_border, 0);
			wclrtoeol(slist->win);
		}
	}

	cell = slist->cell_list[slist->top_line];

	wmove(slist->win, cell->line, 1);
	wclrtoeol(slist->win);
	mvwputstr(slist->win, cell->line, SLIST_FIRST_COL(slist), cell->label);
	slist->dirty = 1;
}

static void
slist_scroll_screen_down(struct slist *slist)
{
	struct slist_cell *cell;

	scrollok(slist->win, TRUE);
	wscrl(slist->win, 1);
	scrollok(slist->win, FALSE);

	cell = slist->cell_list[slist->bot_line];

	wmove(slist->win, cell->line, 1);
	wclrtoeol(slist->win);
	mvwputstr(slist->win, cell->line, SLIST_FIRST_COL(slist), cell->label);
	slist->dirty = 1;
}

int
slist_init(struct slist *slist, u_int32_t rows, u_int32_t cols,
    u_int32_t xpos, u_int32_t ypos)
{
	WINDOW *win;

	win = newwin(rows, cols, ypos, xpos);
	if (win == NULL)
		return (-1);
	set_default_win_opts(win);

	memset(slist, 0, sizeof(*slist));

	slist->win = win;
	slist->rows = rows;
	slist->cols = cols;
	slist->has_border = 1;
	slist->cursor_line = -1;

	slist->n_cells = 1;
	slist->cell_list = xmalloc(slist->n_cells * sizeof(struct slist_cell *));
	memset(slist->cell_list, 0, slist->n_cells * sizeof(struct slist_cell *));

	return (0);
}

void slist_cell_free_cb(struct slist *slist, void (*cell_free_cb)(void *)) {
	slist->cell_free_cb = cell_free_cb;
}

void
slist_destroy(struct slist *slist)
{
	//int i;

#if 0
	for (i = 0; i < slist->n_cells; i++) {
		slist_destroy_cell(slist, slist->cell_list[i]);
		slist->cell_list[i] = NULL;
	}
#endif
	delwin(slist->win);
	free(slist->cell_list);
	memset(slist, 0, sizeof(*slist));
}

int slist_resize(	struct slist *slist,
					u_int32_t rows,
					u_int32_t cols,
					u_int32_t screen_cols)
{
	struct slist_cell *cell;

	slist->rows = rows;
	slist->cols = cols;

	wresize(slist->win, slist->rows, slist->cols);
	mvwin(slist->win, 0, screen_cols - slist->cols);

	wmove(slist->win, 0, 0);
	wclrtobot(slist->win);

	slist_renumber(slist, 0 + slist->has_border);
	slist_draw(slist);

	if (slist->cursor_line != -1) {
		cell = slist->cell_list[slist->cursor_line];

		if (cell->line == -1)
			slist->cursor_line = slist->bot_line;
		else if (cell->line == -2)
			slist->cursor_line = slist->top_line;
	} else {
		/*
		** This should only happen if the list is
		** completely empty.
		*/
		slist->cursor_line = slist->top_line;
	}
	slist->dirty = 1;
	return (0);
}

int
slist_cursor_up(struct slist *slist)
{
	int i;
	struct slist_cell *cell;
	struct slist_cell *cursor;

	/* If the cursor hasn't been initialized yet don't let it move.
	 * Also if the cursor is on the very first slist_cell, don't
	 * let it move up. */
	if (slist->cursor == NULL || slist->cursor->index == 0)
		return (-1);

	cursor = NULL;
	for (i = slist->cursor->index - 1; i >= 0; i--) {
		cursor = slist->cell_list[i];
		if (cursor != NULL)
			break;
	}
	if (cursor == NULL)
		return (-1);

	slist->cursor_line = i;
	slist->cursor = cursor;

	cell = cursor;
	if (cell->line < 0) {
		int j;
		struct slist_cell *bot_cell = slist->cell_list[slist->bot_line];


		for (j = slist->top_line - 1; j > 0; j--) {
			slist->top_line = j;
			if (slist->cell_list[j] != NULL)
				break;
		}
		if (bot_cell->line == SLIST_LAST_ROW(slist))
			bot_cell->line = -1;

		slist_renumber(slist, 0 + slist->has_border);
		slist_scroll_screen_up(slist);
	}

	slist->dirty = 1;
	return (0);
}

/* Move the cursor down one line (if possible) */
int
slist_cursor_down(struct slist *slist)
{
	struct slist_cell *cell;
	struct slist_cell *cursor = NULL;
	int i;

	/* If the cursor hasn't been initialized yet don't let it move */
	if (slist->cursor == NULL)
		return (-1);

	/* Find the next cell in the list that the cursor can be placed on. */
	for (i = slist->cursor->index + 1; i < slist->n_cells; i++) {
		cursor = slist->cell_list[i];
		if (cursor != NULL)
			break;
	}
	/* If we went through the entire list of cells, and cursor ends up
	 * being NULL then we can't move further down */
	if (cursor == NULL)
		return (-1);

	cell = cursor;

	/*
	** The top line scrolls off the top.
	*/
	if (cell->line < 0) {
		int j;
		struct slist_cell *top_cell = slist->cell_list[slist->top_line];;

		top_cell->line = -2;
		for (j = slist->top_line + 1; j < slist->n_cells; j++) {
			slist->top_line = j;
			if (slist->cell_list[j] != NULL)
				break;
		}
		slist_renumber(slist, 0 + slist->has_border);
		slist_scroll_screen_down(slist);
	}

	slist->cursor = cell;
	slist->cursor_line = i;
	slist->dirty = 1;
	return (0);
}

int slist_cursor_start(struct slist *slist) {
	int ret;
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while ((ret = slist_cursor_up(slist)) == 0)
		scrolled++;

	return (scrolled);
}

int slist_cursor_end(struct slist *slist) {
	int ret;
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while ((ret = slist_cursor_down(slist)) == 0)
		scrolled++;

	return (scrolled);
}

int slist_cursor_pgdown(struct slist *slist) {
	int ret;
	int lines = slist->rows - (slist->has_border * 2);
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while (lines-- > 0 && (ret = slist_cursor_down(slist)) == 0)
		scrolled++;

	return (scrolled);
}

int slist_cursor_pgup(struct slist *slist) {
	int ret;
	int lines = slist->rows - (slist->has_border * 2);
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while (lines-- > 0 && (ret = slist_cursor_up(slist)) == 0)
		scrolled++;

	return (scrolled);
}

inline struct slist_cell *slist_get_cursor(struct slist *slist) {
	if (slist->cursor == NULL)
		return (NULL);

	return (slist->cursor);
}

static void
slist_renumber(struct slist *slist, int start_line)
{
	int i;
	int number = start_line;

	/* Give all valid cells a starting number */
	for (i = slist->top_line; i < slist->n_cells; i++) {
		struct slist_cell *cell;

		cell = slist->cell_list[i];
		if (cell == NULL)
			continue;

		if (number > SLIST_LAST_ROW(slist)) {
			cell->line = -1;
		} else {
			cell->line = number++;
			slist->bot_line = i;
		}
	}
}

struct slist_cell *
slist_add(struct slist *slist, struct slist_cell *cell, int number)
{
	if (slist->cursor_line == -1)
		slist->cursor_line = 0;

	if (slist->cursor == NULL)
		slist->cursor = cell;

	/* If a user logs in and the cell list doesn't contain enough
	 * cells, then we need to resize the cell list */
	if ((number + 1) > slist->n_cells) {
		int i;
		int grow_by;

		grow_by = ((number + 1) - slist->n_cells) + 1;
		slist->cell_list = xrealloc(slist->cell_list,
		    (slist->n_cells + grow_by) * sizeof(struct slist_cell *));
		/* Set all the new cells created to NULL */
		for (i = slist->n_cells; i < slist->n_cells + grow_by; i++) {
			/* Set the cell to NULL */
			slist->cell_list[i] = NULL;
		}
		slist->n_cells += grow_by;
	}

	/* If someone already has that number, free it up */
	if (slist->cell_list[number] != NULL)
		free(slist->cell_list[number]);

	cell->index = number;
	slist->cell_list[number] = cell;

	/* Line assignments should be done in a renumber function */
	slist_renumber(slist, 0 + slist->has_border);

	slist->next_free_line++;
	return cell;
}

/* Deletes a cell from the slist */
struct slist_cell *
slist_del(struct slist *slist, struct slist_cell *cell)
{
	int i;
	int cell_num;

	cell_num = -1;

	if (cell == NULL)
		return (NULL);

	/* Find the cell in the slist cell list */
	for (i = 0; i < slist->n_cells; i++) {
		if (cell == slist->cell_list[i]) {
			cell_num = i;
			break;
		}
	}

	/* If we didn't find it, don't try to delete anything */
	if (cell_num == -1)
		return (NULL);

	/* If the cursor is positioned on the cell we are about to delete
	 * we will first try to move it to the next line. If that doesn't
	 * work we can go back a line */
	if (slist->cursor == slist->cell_list[cell_num]) {
		for (i = cell_num + 1; i < slist->n_cells; i++) {
			if (slist->cell_list[i] != NULL) {
				slist->cursor = slist->cell_list[i];
				break;
			}
		}
		if (slist->cursor == slist->cell_list[cell_num]) {
			for (i = cell_num - 1; i >= 0; i--) {
				if (slist->cell_list[i] != NULL) {
					slist->cursor = slist->cell_list[i];
					break;
				}
			}
		}
	}

	slist_destroy_cell(slist, slist->cell_list[cell_num]);
	slist->cell_list[cell_num] = NULL;

	slist_renumber(slist, 0 + slist->has_border);

	return (0);
}

void
slist_draw_line(struct slist *slist, struct slist_cell *cell)
{
	int ret;

	if (cell->line < 0)
		return;

	wmove(slist->win, cell->line, SLIST_FIRST_COL(slist));
	ret = mvwputnstr(slist->win, cell->line, slist->has_border, cell->label,
			SLIST_LAST_COL(slist) - SLIST_FIRST_COL(slist) + 1);

	/*
	** Pad it out with spaces.
	*/

	while (++ret <= SLIST_LAST_COL(slist))
		waddch(slist->win, ' ');

	slist->dirty = 1;
}

void
slist_draw_cursor(struct slist *slist, attr_t curs_attr)
{
	int i;
	struct slist_cell *cell;

	if (slist == NULL || slist->cursor == NULL)
		return;

	cell = slist->cursor;
	if (cell->line < 0)
		return;

	wmove(slist->win, cell->line, SLIST_FIRST_COL(slist));

	if (curs_attr) {
		/*
		** I don't call wattron and then wputstr here because
		** ncurses' waddch fuction seems to ignore the attributes
		** that you just set with wattron if a chtype already has
		** color attributes set.
		*/

		for (i = 0 ; i <= SLIST_LAST_COL(slist) && cell->label[i] != 0 ; i++)
			waddch(slist->win, chtype_get(cell->label[i]) | curs_attr);
	} else {
		/*
		** If no attributes are specified, just use the ones in
		** the chtype already.
		*/

		for (i = 0 ; i <= SLIST_LAST_COL(slist) && cell->label[i] != 0 ; i++)
			waddch(slist->win, cell->label[i]);
	}

	/*
	** Pad it out with spaces.
	*/

	while (++i <= SLIST_LAST_COL(slist))
		waddch(slist->win, ' ' | curs_attr);

	slist->dirty = 1;
}

void
slist_draw(struct slist *slist)
{
	int i;

	/* Starting at a certain location, draw to the bottom. If there are
	 * to many users in the list, i may not always start at 0. */
	for (i = 0; i < slist->n_cells; i++) {
		struct slist_cell *cell;

		cell = slist->cell_list[i];
		if (cell == NULL)
			continue;

		slist_draw_line(slist, cell);
	}
	slist->dirty = 1;
}

void
slist_clear_bot(struct slist *slist)
{
	struct slist_cell *cell;

	cell = slist->cell_list[slist->bot_line];
	if (cell == NULL)
		return;

	if (cell->line != SLIST_LAST_ROW(slist)) {
		wmove(slist->win, cell->line + 1, 0);
		wclrtobot(slist->win);
		slist->dirty = 1;
	}
}

int slist_refresh(struct slist *slist) {
	if (!slist->dirty)
		return (0);

	wnoutrefresh(slist->win);
	slist->dirty = 0;
	return (1);
}
