/*
** Heavily based on pork_imwindow.h
** ncic_imwindow.h - interface for manipulating conversation/chat/info windows.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_IMWINDOW_H__
#define __NCIC_IMWINDOW_H__

#ifdef __cplusplus
extern "C" {
#endif

struct pork_acct;
struct imsg;

#include "ncic_set.h"
#include "ncic_swindow.h"

enum {
	WIN_TYPE_STATUS,
	WIN_TYPE_PRIVMSG,
	WIN_TYPE_CHAT
};

struct imwindow {
	struct swindow swindow;
	struct input *input;
	struct pork_acct *owner;
	struct key_binds *active_binds;
	char *target;
	char *name;
	void *data;
	uint32_t refnum;
	uint32_t type:2;
	uint32_t typing:2;
	uint32_t input_focus:1;
	uint32_t ignore_activity:1;
	uint32_t skip:1;
	pref_val_t opts[WOPT_NUM_OPTS];
};

struct imwindow *imwindow_new(	uint32_t rows,
								uint32_t cols,
								uint32_t refnum,
								uint32_t type,
								struct pork_acct *owner,
								const char *target);

void imwindow_resize(	struct imwindow *imwindow,
						uint32_t rows,
						uint32_t cols);

int imwindow_set_priv_input(struct imwindow *imwindow, int val);
void imwindow_send_msg(struct imwindow *win);
void imwindow_recv_msg(struct imwindow *win);
int imwindow_bind_acct(struct imwindow *imwindow, uint32_t refnum);
int imwindow_bind_next_acct(struct imwindow *imwindow);
int imwindow_refresh(struct imwindow *imwindow);
void imwindow_destroy(struct imwindow *imwindow);
void imwindow_switch_focus(struct imwindow *imwindow);
void imwindow_buffer_find(struct imwindow *imwindow, char *str, uint32_t opt);

struct imwindow *imwindow_find_refnum(uint32_t refnum);
struct imwindow *imwindow_find(struct pork_acct *owner, const char *target);
struct imwindow *imwindow_find_name(struct pork_acct *owner, const char *name);
struct imwindow *imwindow_find_chat_target(	struct pork_acct *owner,
											const char *target);

int imwindow_add(struct imwindow *imwindow,
						struct imsg *imsg,
						uint32_t type);

int imwindow_ignore(struct imwindow *imwindow);
int imwindow_unignore(struct imwindow *imwindow);
int imwindow_skip(struct imwindow *imwindow);
int imwindow_unskip(struct imwindow *imwindow);
int imwindow_dump_buffer(struct imwindow *imwindow, char *file);
void imwindow_rename(struct imwindow *imwindow, char *new_name);
void imwindow_scroll_up(struct imwindow *imwindow);
void imwindow_scroll_down(struct imwindow *imwindow);
void imwindow_scroll_by(struct imwindow *imwindow, int lines);
void imwindow_scroll_page_up(struct imwindow *imwindow);
void imwindow_scroll_page_down(struct imwindow *imwindow);
void imwindow_scroll_start(struct imwindow *imwindow);
void imwindow_scroll_end(struct imwindow *imwindow);
void imwindow_clear(struct imwindow *imwindow);
void imwindow_erase(struct imwindow *imwindow);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_IMWINDOW_H__ */
