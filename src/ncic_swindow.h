/*
** Heavily based on pork_swindow.h
** ncic_swindow.h - an ncurses scrolling window widget
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_SWINDOW_H__
#define __NCIC_SWINDOW_H__

#include "ncic_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SWINDOW_FIND_ICASE		0x01
#define SWINDOW_FIND_BASIC		0x02

struct imsg;

struct swindow {
	WINDOW *win;
	uint32_t serial;

	uint32_t rows;
	uint32_t cols;

	uint32_t scrollbuf_max;
	uint32_t scrollbuf_len;
	uint32_t scrollbuf_lines;

	uint32_t held;
	uint32_t bottom_blank;

	/*
	** How many lines of the message that's currently
	** at the top or bottom of the screen aren't
	** currently being displayed on screen.
	*/
	uint32_t top_hidden;
	uint32_t bottom_hidden;

	/* pointers to the head and tail of the message list */
	dlist_t *scrollbuf;
	dlist_t *scrollbuf_end;

	/* pointers to the top and bottom lines currently displayed */
	dlist_t *scrollbuf_top;
	dlist_t *scrollbuf_bot;

	/* window-specific preferences */
	uint32_t activity_type;
	uint32_t log_type;

	char *logfile;
	int log_fd;

	char wordwrap_char;
	uint32_t visible:1;
	uint32_t dirty:1;
	uint32_t activity:1;
	uint32_t beep_on_output:1;
	uint32_t scroll_on_input:1;
	uint32_t scroll_on_output:1;
	uint32_t timestamp:1;
	uint32_t logged:1;
	uint32_t wordwrap:1;
};

int swindow_init(	struct swindow *swindow,
					WINDOW *win,
					uint32_t rows,
					uint32_t cols,
					pref_val_t *wopt);

int swindow_destroy(struct swindow *swindow);
int swindow_add(struct swindow *swindow, struct imsg *imsg, uint32_t type);
int swindow_input(struct swindow *swindow);
void swindow_redraw(struct swindow *swindow);
void swindow_clear(struct swindow *swindow);
void swindow_erase(struct swindow *swindow);
int swindow_refresh(struct swindow *swindow);

void swindow_resize(struct swindow *swindow,
					uint32_t new_row,
					uint32_t new_col);

int swindow_print_matching(	struct swindow *swindow,
							const char *regex,
							uint32_t options);

int swindow_dump_buffer(struct swindow *swindow, char *file);
int swindow_set_log(struct swindow *swindow);
void swindow_end_log(struct swindow *swindow);
void swindow_set_logfile(struct swindow *swindow, char *logfile);
void swindow_set_timestamp(struct swindow *swindow, uint32_t value);
void swindow_set_wordwrap(struct swindow *swindow, uint32_t value);
void swindow_prune(struct swindow *swindow);

void swindow_scroll_to_end(struct swindow *swindow);
void swindow_scroll_to_start(struct swindow *swindow);
int swindow_scroll_by(struct swindow *swindow, int lines);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_SWINDOW_H__ */
