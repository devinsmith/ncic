/*
** Heavily based on pork_slist.h
** ncic_slist.h - an ncurses scrolling list widget
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_SLIST_H__
#define __NCIC_SLIST_H__

#define SLIST_FIRST_ROW(x) (0 + (x)->has_border)
#define SLIST_FIRST_COL(x) (0 + (x)->has_border)

#define SLIST_LAST_ROW(x) \
	((int) ((x)->rows - (x)->has_border - 1))

#define SLIST_LAST_COL(x) \
	((int) ((x)->cols - (x)->has_border - 1))

#define TYPE_LIST_CELL	0
#define TYPE_FLAT_CELL	1

struct slist_cell {
	u_int32_t type:1;
	u_int32_t collapsed:1;

	u_int32_t refnum;
	unsigned short index;

	/*
	** Line can be -1 meaning off the screen at the bottom,
	** -2, meaning off the screen at the top, or a positive
	** integer, indicating what physical line it is displayed on.
	*/
	int line;

	chtype *label;
	struct slist_cell *parent;
	dlist_t *children;
	void *data;
};

struct slist {
	u_int32_t rows;
	u_int32_t cols;
	u_int32_t dirty:1;
	u_int32_t has_border:1;
	WINDOW *win;
	unsigned int n_cells;
	struct slist_cell **cell_list;
	struct slist_cell *cursor;
	int cursor_line;
	int top_line;
	int bot_line;
	int next_free_line;
	struct slist_cell *root;

	void (*cell_free_cb)(void *);
};

int slist_init(	struct slist *slist,
				u_int32_t rows,
				u_int32_t cols,
				u_int32_t xpos,
				u_int32_t ypos);

void slist_cell_free_cb(struct slist *slist, void (*cell_free_cb)(void *));
int slist_resize(	struct slist *slist,
					u_int32_t new_rows,
					u_int32_t new_cols,
					u_int32_t screen_cols);

int slist_cursor_up(struct slist *slist);
int slist_cursor_down(struct slist *slist);
int slist_cursor_start(struct slist *slist);
int slist_cursor_end(struct slist *slist);
int slist_cursor_pgup(struct slist *slist);
int slist_cursor_pgdown(struct slist *slist);

struct slist_cell *slist_del(struct slist *slist, struct slist_cell *cell);
struct slist_cell *slist_add(struct slist *slist, struct slist_cell *cell, int number);

void slist_destroy(struct slist *slist);
void slist_draw(struct slist *slist);
void slist_draw_cursor(struct slist *slist, attr_t curs_attr);
void slist_draw_line(struct slist *slist, struct slist_cell *cell);
void slist_clear_bot(struct slist *slist);
int slist_refresh(struct slist *slist);

inline struct slist_cell *slist_get_cursor(struct slist *slist);

#endif /* __NCIC_SLIST_H__ */
