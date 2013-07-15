/*
** Based on pork_imwindow.c
** ncic_imwindow.c - interface for manipulating conversation/chat/info windows.
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
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "ncic.h"
//#include <pork_missing.h>
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_imsg.h"
#include "ncic_set.h"
#include "ncic_swindow.h"
#include "ncic_imwindow.h"
#include "ncic_buddy_list.h"
#include "ncic_proto.h"
#include "ncic_acct.h"
#include "ncic_cstr.h"
#include "ncic_color.h"
#include "ncic_misc.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_format.h"
#include "ncic_chat.h"

extern struct screen screen;

struct imwindow *imwindow_new(	uint32_t rows,
								uint32_t cols,
								uint32_t refnum,
								uint32_t type,
								struct pork_acct *owner,
								char *target)
{
	WINDOW *swin;
	struct imwindow *imwindow;
	struct input *input;
	char nname[NUSER_LEN];

	swin = newwin(rows, cols, 0, 0);
	if (swin == NULL) {
		debug("Unable to create %ux%u window", rows, cols);
		return (NULL);
	}

	owner->proto->normalize(nname, target, sizeof(nname));

	imwindow = xcalloc(1, sizeof(*imwindow));
	imwindow->refnum = refnum;
	imwindow->type = type;
	imwindow->target = xstrdup(nname);
	imwindow->name = color_quote_codes(target);
	imwindow->active_binds = &screen.binds.main;
	imwindow->owner = owner;
	imwindow->owner->ref_count++;

	wopt_init(imwindow, target);

	swindow_init(&imwindow->swindow, swin, rows,
		cols, imwindow->opts);

	/*
	** Use a separate input handler.
	*/

	if (wopt_get_bool(imwindow->opts, WOPT_PRIVATE_INPUT)) {
		input = xmalloc(sizeof(*input));
		input_init(input, cols);
	} else
		input = &screen.input;

	imwindow->input = input;

	if (wopt_get_bool(imwindow->opts, WOPT_SHOW_BLIST)) {
		if (owner->blist != NULL)
			imwindow_blist_show(imwindow);
	}

	return (imwindow);
}

inline void imwindow_rename(struct imwindow *imwindow, char *new_name) {
	free(imwindow->name);
	imwindow->name = color_quote_codes(new_name);
}

inline void imwindow_resize(struct imwindow *imwindow,
							uint32_t rows,
							uint32_t cols)
{
	swindow_resize(&imwindow->swindow, rows, cols);
	swindow_scroll_to_start(&imwindow->swindow);
	swindow_scroll_to_end(&imwindow->swindow);
}

int imwindow_set_priv_input(struct imwindow *imwindow, int val) {
	int old_val;

	old_val = !(imwindow->input == &screen.input);
	if (old_val == val)
		return (-1);

	/*
	** Give this imwindow its own input buffer and history.
	*/
	if (val == 1) {
		struct input *input = xmalloc(sizeof(*input));

		input_init(input, imwindow->swindow.cols);
		imwindow->input = input;
	} else {
		/*
		** Destroy this imwindow's private input, and set its input
		** to the global input.
		*/

		input_destroy(imwindow->input);
		free(imwindow->input);
		imwindow->input = &screen.input;
	}

	return (0);
}

inline int imwindow_blist_refresh(struct imwindow *imwindow) {
	struct pork_acct *owner = imwindow->owner;

	if (!imwindow->blist_visible)
		return (0);

	if (!owner->blist->slist.dirty)
		return (0);

	blist_draw_border(owner->blist, imwindow->input_focus);
	blist_refresh(owner->blist);
	return (1);
}

inline int imwindow_refresh(struct imwindow *imwindow) {
	int was_dirty_win;
	int was_dirty_blist;

	was_dirty_win = swindow_refresh(&imwindow->swindow);
	was_dirty_blist = imwindow_blist_refresh(imwindow);

	return (was_dirty_win || was_dirty_blist);
}

void imwindow_blist_show(struct imwindow *imwindow) {
	uint32_t new_width;

	if (imwindow->owner->blist == NULL)
		return;

	if (imwindow->blist_visible)
		return;

	imwindow->blist_visible = 1;
	imwindow->owner->blist->slist.dirty = 1;
	new_width = imwindow->swindow.cols - imwindow->owner->blist->slist.cols;
	imwindow_resize(imwindow, imwindow->swindow.rows, new_width);
}

void imwindow_buffer_find(struct imwindow *imwindow, char *str, uint32_t opt) {
	screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_LASTLOG, "Matching lines:");
	swindow_print_matching(&imwindow->swindow, str, opt);
	screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_LASTLOG, "End of matches");
}

void imwindow_blist_hide(struct imwindow *imwindow) {
	uint32_t new_width;

	if (!imwindow->blist_visible)
		return;

	imwindow->blist_visible = 0;
	if (imwindow->owner->blist != NULL) {
		imwindow->owner->blist->slist.dirty = 1;
		new_width = imwindow->swindow.cols + imwindow->owner->blist->slist.cols;
	} else
		new_width = screen.cols;

	imwindow->input_focus = BINDS_MAIN;
	imwindow->active_binds = &screen.binds.main;

	imwindow_resize(imwindow, imwindow->swindow.rows, new_width);
}

void imwindow_blist_toggle(struct imwindow *imwindow) {
	if (!imwindow->blist_visible)
		imwindow_blist_show(imwindow);
	else
		imwindow_blist_hide(imwindow);
}

void imwindow_blist_draw(struct imwindow *imwindow) {
	struct blist *blist = imwindow->owner->blist;

	if (blist == NULL || !imwindow->blist_visible)
		return;

	blist_draw(imwindow->owner->blist);
}

void imwindow_destroy(struct imwindow *imwindow) {
	swindow_destroy(&imwindow->swindow);

	if (wopt_get_bool(imwindow->opts, WOPT_PRIVATE_INPUT)) {
		input_destroy(imwindow->input);
		free(imwindow->input);
	}

	if (imwindow->owner != NULL)
		imwindow->owner->ref_count--;

	wopt_destroy(imwindow);
	free(imwindow->name);
	free(imwindow->target);
	free(imwindow);
}

void imwindow_switch_focus(struct imwindow *imwindow) {
	if (!imwindow->blist_visible)
		return;

	if (imwindow->input_focus == BINDS_MAIN) {
		imwindow->input_focus = BINDS_BUDDY;
		imwindow->active_binds = &screen.binds.blist;
	} else {
		imwindow->input_focus = BINDS_MAIN;
		imwindow->active_binds = &screen.binds.main;
	}
}

struct imwindow *imwindow_find(struct pork_acct *owner, const char *target) {
	dlist_t *list_start = screen.window_list;
	dlist_t *cur = list_start;
	char nname[NUSER_LEN];

	owner->proto->normalize(nname, target, sizeof(nname));

	do {
		struct imwindow *imwindow = cur->data;

		if (imwindow->owner == owner && imwindow->type == WIN_TYPE_PRIVMSG &&
			!strcasecmp(imwindow->target, nname))
		{
			return (imwindow);
		}

		cur = cur->next;
	} while (cur != list_start);

	return (NULL);
}

struct imwindow *imwindow_find_chat_target(	struct pork_acct *owner,
											const char *target)
{
	dlist_t *list_start = screen.window_list;
	dlist_t *cur = list_start;
	char nname[NUSER_LEN];

	owner->proto->normalize(nname, target, sizeof(nname));

	do {
		struct imwindow *imwindow = cur->data;

		if (imwindow->owner == owner &&
			imwindow->type == WIN_TYPE_CHAT &&
			!strcasecmp(imwindow->target, nname))
		{
			return (imwindow);
		}

		cur = cur->next;
	} while (cur != list_start);

	return (NULL);
}

struct imwindow *imwindow_find_name(struct pork_acct *owner, const char *name) {
	dlist_t *list_start = screen.window_list;
	dlist_t *cur = list_start;

	do {
		struct imwindow *imwindow = cur->data;

		if (imwindow->owner == owner && !strcasecmp(imwindow->name, name))
			return (imwindow);

		cur = cur->next;
	} while (cur != list_start);

	return (NULL);
}

struct imwindow *imwindow_find_refnum(uint32_t refnum) {
	dlist_t *cur = screen.window_list;

	do {
		struct imwindow *imwindow = cur->data;

		if (imwindow->refnum == refnum)
			return (imwindow);

		cur = cur->next;
	} while (cur != screen.window_list);

	return (NULL);
}

inline void imwindow_send_msg(struct imwindow *win) {
	swindow_input(&win->swindow);
}

inline void imwindow_recv_msg(struct imwindow *win) {
	if (wopt_get_bool(win->opts, WOPT_BEEP_ON_OUTPUT))
		beep();
}

/*
** Bind the account whose reference number is "refnum" to the window
** "imwindow".
*/

int imwindow_bind_acct(struct imwindow *imwindow, uint32_t refnum) {
	struct pork_acct *owner;
	struct pork_acct *old_acct = imwindow->owner;

	if (imwindow->type == WIN_TYPE_CHAT &&
		imwindow->owner != screen.null_acct)
	{
		return (-1);
	}

	owner = pork_acct_get_data(refnum);
	if (owner == NULL)
		return (-1);

	if (old_acct != owner) {
		imwindow->owner->ref_count--;
		imwindow->owner = owner;
		imwindow->owner->ref_count++;

		if (imwindow->blist_visible) {
			if (imwindow->owner->blist == NULL)
				imwindow_blist_hide(imwindow);
			else {
				imwindow_blist_draw(imwindow);
				imwindow_blist_refresh(imwindow);
			}
		}
	}

	return (0);
}

inline int imwindow_dump_buffer(struct imwindow *imwindow, char *file) {
	return (swindow_dump_buffer(&imwindow->swindow, file));
}

/*
** Binds the next account in this window.
*/

int imwindow_bind_next_acct(struct imwindow *imwindow) {
	uint32_t next_refnum;

	if (pork_acct_next_refnum(imwindow->owner->refnum, &next_refnum) == -1)
		return (-1);

	return (imwindow_bind_acct(imwindow, next_refnum));
}

inline void imwindow_scroll_up(struct imwindow *imwindow) {
	swindow_scroll_by(&imwindow->swindow, -1);
}

inline void imwindow_scroll_down(struct imwindow *imwindow) {
	swindow_scroll_by(&imwindow->swindow, 1);
}

inline void imwindow_scroll_by(struct imwindow *imwindow, int lines) {
	swindow_scroll_by(&imwindow->swindow, lines);
}

inline void imwindow_scroll_page_up(struct imwindow *imwindow) {
	swindow_scroll_by(&imwindow->swindow, -imwindow->swindow.rows);
}

inline void imwindow_scroll_page_down(struct imwindow *imwindow) {
	swindow_scroll_by(&imwindow->swindow, imwindow->swindow.rows);
}

inline void imwindow_scroll_start(struct imwindow *imwindow) {
	swindow_scroll_to_start(&imwindow->swindow);
}

inline void imwindow_scroll_end(struct imwindow *imwindow) {
	swindow_scroll_to_end(&imwindow->swindow);
}

inline void imwindow_clear(struct imwindow *imwindow) {
	swindow_clear(&imwindow->swindow);
}

inline void imwindow_erase(struct imwindow *imwindow) {
	swindow_erase(&imwindow->swindow);
}

inline int imwindow_ignore(struct imwindow *imwindow) {
	int ret = imwindow->ignore_activity;

	imwindow->ignore_activity = 1;
	return (ret);
}

inline int imwindow_unignore(struct imwindow *imwindow) {
	int ret = imwindow->ignore_activity;

	imwindow->ignore_activity = 0;
	return (ret);
}

inline int imwindow_skip(struct imwindow *imwindow) {
	int ret = imwindow->skip;

	imwindow->skip = 1;
	return (ret);
}

inline int imwindow_unskip(struct imwindow *imwindow) {
	int ret = imwindow->skip;

	imwindow->skip = 0;
	return (ret);
}

inline int imwindow_add(struct imwindow *imwindow,
						struct imsg *imsg,
						uint32_t type)
{
	return (swindow_add(&imwindow->swindow, imsg, type));
}
