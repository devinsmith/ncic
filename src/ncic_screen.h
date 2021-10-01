/*
** Based on pork_screen.h
** ncic_screen.h - screen management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_SCREEN_H__
#define __NCIC_SCREEN_H__

extern struct screen screen;

enum {
	FOCUS_MSG,
	FOCUS_BUDDY
};

struct blist;
struct imwindow;
struct pork_acct;

#include "ncic_input.h"
#include "ncic_bind.h"

struct screen {
	u_int32_t rows;
	u_int32_t cols;
	dlist_t *cur_window;
	dlist_t *window_list;
	dlist_t *timer_list;
	struct imwindow *status_win;
  struct pork_acct *acct;
	WINDOW *status_bar;
	u_int32_t quiet:1;
	struct pork_acct *null_acct;
	struct input input;
	struct binds binds;
	hash_t alias_hash;
};

#define cur_window() ((struct imwindow *) (screen.cur_window->data))

int screen_init(u_int32_t rows, u_int32_t cols);
void screen_destroy(void);

int screen_renumber(struct imwindow *imwindow, u_int32_t refnum);
void screen_add_window(struct imwindow *imwindow);
void screen_resize(u_int32_t rows, u_int32_t cols);
void screen_window_swap(dlist_t *new_cur);
int screen_goto_window(u_int32_t refnum);
void screen_refresh(void);
void screen_bind_all_unbound(struct pork_acct *acct);
struct imwindow *screen_new_window(struct pork_acct *o, char *dest, char *name);
struct imwindow *screen_new_chat_window(struct pork_acct *acct, char *name);
int screen_close_window(struct imwindow *imwindow);
void screen_cycle_fwd(void);
void screen_cycle_bak(void);
void screen_doupdate(void);

#endif /* __NCIC_SCREEN_H__ */
