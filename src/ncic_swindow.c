/*
** Based on pork_swindow.c
** ncic_swindow.c - an ncurses scrolling window widget
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** I've tried to keep this code independent from the rest of the program
** so that it might be useful outside of this particular program.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <unistd.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h>
#include <sys/uio.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_set.h"
#include "ncic_swindow.h"
#include "ncic_cstr.h"
#include "ncic_misc.h"
#include "ncic_imsg.h"
#include "ncic_screen_io.h"

static void swindow_scroll(struct swindow *swindow, int n);

static int swindow_print_msg_wr(struct swindow *swindow,
								struct imsg *imsg,
								uint32_t y,
								uint32_t x,
								uint32_t firstline,
								uint32_t lastline)
{
	uint32_t len = imsg->len;
	chtype *ch = imsg->text;
	uint32_t lines = 0;
	uint32_t lines_printed = 0;
	chtype *end = &ch[len - 1];
	int add = 0;
	chtype cont_char = (chtype) swindow->wordwrap_char;

	if (swindow->cols == 1)
		cont_char = 0;

	while (*ch != 0) {
		chtype *p;

		p = &ch[swindow->cols - 1 - add];
		if (p >= end) {
			++lines;
			if (lines <= lastline) {
				mvwputstr(swindow->win, y + lines_printed, add, ch);
				++lines_printed;
			}
			break;
		}

		if (chtype_get(p[0]) != ' ' && chtype_get(p[1]) != ' ') {
			chtype *temp = p;

			do {
				temp--;
				if (temp <= ch)
					goto out;
			} while (chtype_get(*temp) != ' ');
			p = temp;
		}
out:
		++lines;
		if (lines >= firstline) {
			if (lines > lastline)
				break;

			mvwputnstr(swindow->win, y + lines_printed, add, ch, p - ch + 1);
			++lines_printed;
		}

		if (cont_char != 0) {
			mvwaddch(swindow->win, y + lines_printed, 0, cont_char);
			add = 1;
		}
		ch = &p[1];
	}

	return (lines_printed);
}

static int swindow_print_msg(	struct swindow *swindow,
								struct imsg *imsg,
								uint32_t y,
								uint32_t x,
								uint32_t firstline,
								uint32_t lastline)
{
	chtype *msg;

	if (swindow->wordwrap)
		return (swindow_print_msg_wr(swindow, imsg, y, x, firstline, lastline));

	if (lastline < firstline)
		return (-1);

	msg = imsg_partial(swindow, imsg, firstline);

	mvwputnstr(swindow->win, y, x, msg,
		min(swindow->rows * swindow->cols,
			(lastline - firstline + 1) * swindow->cols));

	return (0);
}

/*
** Initialize a new window.
*/

int swindow_init(	struct swindow *swindow,
					WINDOW *win,
					uint32_t rows,
					uint32_t cols,
					pref_val_t *wopt)
{
	set_default_win_opts(win);

	memset(swindow, 0, sizeof(*swindow));
	swindow->win = win;
	swindow->cols = cols;
	swindow->rows = rows;
	swindow->bottom_blank = rows;
	swindow->visible = 0;
	swindow->dirty = 1;

	swindow->scrollbuf_max = wopt_get_int(wopt, WOPT_SCROLLBUF_LEN);
	swindow->scroll_on_input = wopt_get_bool(wopt, WOPT_SCROLL_ON_INPUT);
	swindow->scroll_on_output = wopt_get_bool(wopt, WOPT_SCROLL_ON_OUTPUT);
	swindow->timestamp = wopt_get_bool(wopt, WOPT_TIMESTAMP);
	swindow->wordwrap = wopt_get_bool(wopt, WOPT_WORDWRAP);
	swindow->wordwrap_char = wopt_get_char(wopt, WOPT_WORDWRAP_CHAR);
	swindow->logged = wopt_get_bool(wopt, WOPT_LOG);
	swindow->logfile = wopt_get_str(wopt, WOPT_LOGFILE);
	swindow->activity_type = wopt_get_int(wopt, WOPT_ACTIVITY_TYPES);
	swindow->log_type = wopt_get_int(wopt, WOPT_LOG_TYPES);

	if (swindow->scrollbuf_max < rows)
		swindow->scrollbuf_max = rows;

	if (swindow->logged)
		swindow_set_log(swindow);

	return (0);
}

/*
** We don't want the window scrolling when something is written to
** (max_col - 1, max_rows - 1), so scrolling is turned off by default.
** When we need to scroll the window, enable it, scroll the window, then
** disable it.
*/

static inline void swindow_scroll(struct swindow *swindow, int n) {
	scrollok(swindow->win, TRUE);
	wscrl(swindow->win, n);
	scrollok(swindow->win, FALSE);
	swindow->dirty = 1;
}

/*
** Prune the scroll buffer so that the number of total
** messages is not greater than swindow->scrollbuf_max.
*/

void swindow_prune(struct swindow *swindow) {
	int num = swindow->scrollbuf_len - swindow->scrollbuf_max;
	dlist_t *cur = swindow->scrollbuf_end;
	uint32_t serial_top;
	uint32_t serial_bot;

	serial_top = ((struct imsg *) swindow->scrollbuf_top->data)->serial;
	serial_bot = ((struct imsg *) swindow->scrollbuf_bot->data)->serial;

	while (cur != NULL && num > 0) {
		struct imsg *imsg = cur->data;
		dlist_t *next = cur->prev;

		/* Don't prune anything that's still on the screen */
		if (imsg->serial >= serial_top && imsg->serial <= serial_bot)
			break;

		swindow->scrollbuf_lines -= imsg->lines;
		swindow->scrollbuf_len--;
		num--;

		free(imsg->text);
		free(imsg);
		dlist_remove(swindow->scrollbuf, cur);
		cur = next;
	}

	swindow->scrollbuf_end = cur;
}

/*
** Adjust the swindow->scrollbuf_top pointer so that it's pointing to
** the line that's "n" lines up from the current top of the screen.
*/

static void swindow_adjust_top(struct swindow *swindow, uint32_t n) {
	dlist_t *cur = swindow->scrollbuf_top;

	while (1) {
		struct imsg *imsg = cur->data;
		uint32_t visible_lines = imsg->lines;

		if (swindow->top_hidden != 0) {
			visible_lines -= swindow->top_hidden;
			swindow->top_hidden = 0;
		}

		if (visible_lines == n) {
			swindow->top_hidden = 0;
			cur = cur->prev;
			break;
		}

		if (visible_lines > n) {
			swindow->top_hidden = imsg->lines - (visible_lines - n);
			break;
		}

		n -= visible_lines;
		cur = cur->prev;
	}

	swindow->scrollbuf_top = cur;
}

/*
** Recalculate the number of lines for each message.
**
** This should be called when the window is resized
** and when timestamps are turned on or off.
*/

static void swindow_recalculate(struct swindow *swindow,
								uint32_t old_rows,
								uint32_t old_cols)
{
	dlist_t *cur = swindow->scrollbuf;
	uint32_t total_lines = 0;
	struct imsg *imsg_top;
	uint32_t old_top;

	if (swindow->scrollbuf_top == NULL) {
		swindow->bottom_blank = swindow->rows;
		return;
	}

	imsg_top = swindow->scrollbuf_top->data;
	old_top = imsg_top->lines;

	while (cur != NULL) {
		struct imsg *imsg = cur->data;

		imsg->lines = imsg_lines(swindow, imsg);
		total_lines += imsg->lines;
		cur = cur->next;
	}

	swindow->scrollbuf_lines = total_lines;

	/*
	** If the number of rows needed to display the top line has
	** changed, the number of hidden rows for the top message
	** has to be adjusted.
	**
	** The goal is for the line that was on the top of the display
	** before the resize to stay there.
	*/

	if (swindow->top_hidden != 0 && imsg_top->lines != old_top)
		swindow->top_hidden = (old_cols * swindow->top_hidden) / swindow->cols;
}

/*
** Called when the dimensions of the window have changed.
*/

void swindow_resize(struct swindow *swindow,
					uint32_t new_row,
					uint32_t new_col)
{
	uint32_t old_rows = swindow->rows;
	uint32_t old_cols = swindow->cols;
	int ret;

	/*
	** This seems to be necessary to prevent some funny stuff
	** from happening.
	*/

	wclear(swindow->win);
	wnoutrefresh(swindow->win);

	ret = wresize(swindow->win, new_row, new_col);
	if (ret == -1)
		debug("wresize: %d (%u,%u)", ret, new_row, new_col);

	swindow->rows = new_row;
	swindow->cols = new_col;

	if (swindow->scrollbuf_max < swindow->rows)
		swindow->scrollbuf_max = swindow->rows;

	swindow_recalculate(swindow, old_rows, old_cols);
	swindow_redraw(swindow);
}

int swindow_refresh(struct swindow *swindow) {
	if (swindow->visible && swindow->dirty) {
		wnoutrefresh(swindow->win);
		swindow->dirty = 0;

		return (1);
	}

	return (0);
}

/*
** Redraw the window keeping the line that's currently
** at the top of the screen in place.
*/

void swindow_redraw(struct swindow *swindow) {
	dlist_t *cur = swindow->scrollbuf_top;
	uint32_t curs_pos = 0;

	if (cur == NULL)
		return;
	/*
	** If part of the top message is scrolled off
	** the top, print the visible part and advance
	** the pointer to the next message.
	*/
	if (swindow->top_hidden != 0) {
		struct imsg *imsg = cur->data;

		curs_pos += imsg->lines - swindow->top_hidden;
		if (curs_pos > swindow->rows) {
			swindow->bottom_blank = 0;
			swindow->bottom_hidden = curs_pos - swindow->rows;
			swindow->scrollbuf_bot = cur;
			swindow_print_msg(swindow, imsg, 0, 0, swindow->top_hidden + 1,
				curs_pos + swindow->rows);
		} else
			swindow_print_msg(swindow, imsg, 0, 0, swindow->top_hidden + 1, -1);

		cur = cur->prev;
	}

	while (cur != NULL && curs_pos < swindow->rows) {
		struct imsg *imsg = cur->data;

		if (curs_pos + imsg->lines > swindow->rows) {
			swindow->scrollbuf_bot = cur;
			swindow->bottom_blank = 0;
			swindow->bottom_hidden = imsg->lines - (swindow->rows - curs_pos);

			swindow_print_msg(swindow, imsg, curs_pos, 0,
				1, swindow->rows - curs_pos);
			break;
		}

		swindow_print_msg(swindow, imsg, curs_pos, 0, 1, -1);
		curs_pos += imsg->lines;

		if (cur->prev == NULL) {
			swindow->scrollbuf_bot = cur;
			swindow->bottom_blank = swindow->rows - curs_pos;
			swindow->bottom_hidden = 0;
			swindow->held = 0;
		} else if (curs_pos == swindow->rows) {
			swindow->scrollbuf_bot = cur;
			swindow->bottom_hidden = 0;
			swindow->bottom_blank = 0;
		}

		cur = cur->prev;
	}

	swindow->dirty = 1;
}

/*
** It's the caller's responsibility to make sure there's nothing
** funny in the message that's being added -- things like tabs
** and other control/non-printing characters will mess up the
** display.
*/

int swindow_add(struct swindow *swindow, struct imsg *imsg, uint32_t msgtype) {
	uint32_t msg_line_start = 1;
	dlist_t *old_head = swindow->scrollbuf;
	int y_pos;

	/*
	** If the window is scrolled up, and the scroll on
	** output flag is set, scroll to the bottom of the
	** window before doing anything else.
	*/

	if ((swindow->scrollbuf_bot != swindow->scrollbuf ||
		swindow->bottom_hidden != 0) &&
		swindow->scroll_on_output)
	{
		swindow_scroll_to_end(swindow);
	}

	if (swindow->logged && (swindow->log_type & msgtype)) {
#ifndef WIN32
		struct iovec wvec[2];
		char *plaintext;

		plaintext = cstr_to_plaintext(imsg->text, imsg->len);
		wvec[0].iov_base = plaintext;
		wvec[0].iov_len = imsg->len;
		wvec[1].iov_base = "\n";
		wvec[1].iov_len = 1;

		if (writev(swindow->log_fd, wvec, 2) != (int) imsg->len + 1) {
			screen_err_msg("Error writing logfile: %s",
				strerror(errno));
		}

		free(plaintext);
#endif
	}

	swindow->scrollbuf = dlist_add_head(swindow->scrollbuf, imsg);
	swindow->scrollbuf_len++;
	swindow->scrollbuf_lines += imsg->lines;

	/*
	** If this is the first message in the window, save a pointer
	** to it for use with the scrolling routines.
	*/

	if (old_head == NULL) {
		swindow->scrollbuf_end = swindow->scrollbuf;
		swindow->scrollbuf_top = swindow->scrollbuf;
	}

	/*
	** If the window is scrolled back, but the scroll
	** on output flag is not set, there's nothing left
	** to do here.
	*/

	if (old_head != swindow->scrollbuf_bot) {
		swindow->held += imsg->lines;
		if (swindow->activity_type & msgtype)
			swindow->activity = 1;
		return (0);
	}

	/*
	** The window is scrolling normally (i.e. the user has not scrolled up).
	** Set the current position to the new head of the list
	** (the element containing the message we just added).
	*/

	swindow->scrollbuf_bot = swindow->scrollbuf;

	if (imsg->lines <= swindow->bottom_blank) {
		y_pos = swindow->rows - swindow->bottom_blank;
		swindow->bottom_blank -= imsg->lines;
	} else {
		uint32_t evict = imsg->lines - swindow->bottom_blank;

		swindow->bottom_blank = 0;
		swindow_adjust_top(swindow, evict);

		/*
		** The message could require more rows than the
		** screen has. In this case, print as much of the
		** message as possible such that the end of the message
		** is on the last row.
		*/

		y_pos = swindow->rows - imsg->lines;
		if (y_pos < 0) {
			y_pos = 0;
			msg_line_start = imsg->lines - swindow->rows + 1;
		}

		swindow_scroll(swindow, evict);
	}

	swindow_print_msg(swindow, imsg, y_pos, 0, msg_line_start, -1);
	swindow->dirty = 1;

	if (!swindow->visible && (swindow->activity_type & msgtype))
		swindow->activity = 1;

	/*
	** If there's a maximum scroll buffer length, enforce it.
	*/

	if (swindow->scrollbuf_len > swindow->scrollbuf_max)
		swindow_prune(swindow);

	return (0);
}

/* Called when a message sent by a user is written to a window. */
inline int swindow_input(struct swindow *swindow) {
	/*
	** If the window is scrolled up, and the scroll on
	** input flag is set, scroll to the bottom of the
	** window before doing anything else.
	*/

	if ((swindow->scrollbuf_bot != swindow->scrollbuf ||
		swindow->bottom_hidden != 0) &&
		swindow->scroll_on_input)
	{
		swindow_scroll_to_end(swindow);
	}

	return (0);
}

/*
** Scroll to the end of the scroll buffer (the bottom line).
*/

void swindow_scroll_to_end(struct swindow *swindow) {
	dlist_t *cur;
	uint32_t lines = 0;

	if (swindow->scrollbuf == NULL)
		return;

	/* Avoid a redraw if it's already at the bottom */
	if (swindow->scrollbuf_bot == swindow->scrollbuf &&
		swindow->bottom_hidden == 0)
	{
		return;
	}

	cur = swindow->scrollbuf;
	do {
		struct imsg *imsg = cur->data;

		lines += imsg->lines;
		if (lines >= swindow->rows) {
			swindow->scrollbuf_top = cur;
			swindow->top_hidden = lines - swindow->rows;
			break;
		}

		if (cur->next == NULL) {
			swindow->scrollbuf_top = cur;
			swindow->top_hidden = 0;
			break;
		}
		cur = cur->next;
	} while (1);

	swindow->held = 0;
	wclear(swindow->win);
	swindow_redraw(swindow);
}

/*
** Scroll to the beginning of the scroll buffer (the top line).
*/

void swindow_scroll_to_start(struct swindow *swindow) {
	if (swindow->scrollbuf == NULL)
		return;

	/* Avoid a redraw if it's already at the top */
	if (swindow->scrollbuf_top == swindow->scrollbuf_end &&
		swindow->top_hidden == 0)
	{
		return;
	}

	swindow->top_hidden = 0;
	swindow->scrollbuf_top = swindow->scrollbuf_end;

	wclear(swindow->win);
	swindow_redraw(swindow);
}

static uint32_t swindow_scroll_down_by(struct swindow *swindow, uint32_t lines) {
	uint32_t i;

	/*
	** Adjust the pointer to the top line on the screen.
	*/

	for (i = 0 ; i < lines ; i++) {
		struct imsg *imsg;

		if (swindow->scrollbuf_bot->prev == NULL &&
			swindow->bottom_hidden == 0)
		{
			swindow->held = 0;
			break;
		}

		imsg = swindow->scrollbuf_top->data;

		if (++swindow->top_hidden == imsg->lines) {
			swindow->top_hidden = 0;
			if (swindow->scrollbuf_top->prev == NULL)
				return (1);
			swindow->scrollbuf_top = swindow->scrollbuf_top->prev;
		}

		if (swindow->bottom_hidden > 0)
			swindow->bottom_hidden--;
		else {
			swindow->scrollbuf_bot = swindow->scrollbuf_bot->prev;
			imsg = swindow->scrollbuf_bot->data;
			swindow->bottom_hidden = imsg->lines - 1;
		}
	}

	return (i);
}

static uint32_t swindow_scroll_up_by(struct swindow *swindow, uint32_t lines) {
	dlist_t *cur;

	if (swindow->top_hidden == 0 && swindow->scrollbuf_top->next == NULL)
		return (0);

	if (swindow->top_hidden >= lines) {
		swindow->top_hidden -= lines;
		return (1);
	}

	lines -= swindow->top_hidden;
	swindow->top_hidden = 0;

	cur = swindow->scrollbuf_top->next;
	while (lines > 0 && cur != NULL) {
		struct imsg *msg = cur->data;

		if (msg->lines >= lines) {
			swindow->scrollbuf_top = cur;
			swindow->top_hidden = msg->lines - lines;
			return (1);
		}

		lines -= msg->lines;
		cur = cur->next;
	}

	if (lines > 0)
		swindow->scrollbuf_top = swindow->scrollbuf_end;

	return (1);
}

int swindow_scroll_by(struct swindow *swindow, int lines) {
	/* Can't scroll if there are less (or equal to) lines than rows */
	if (swindow->scrollbuf_lines > swindow->rows) {
		int ret;

		if (lines < 0)
			ret = swindow_scroll_up_by(swindow, -lines);
		else
			ret = swindow_scroll_down_by(swindow, lines);

		if (ret > 0) {
			wclear(swindow->win);
			swindow_redraw(swindow);
			swindow_refresh(swindow);

			/* If the window's visible, a delay is not acceptable. */
			if (swindow->visible)
				screen_doupdate();

			return (1);
		}
	}

	return (0);
}

/*
** Print all messages in the buffer for this window that match
** the specified regular expression.
*/

int swindow_print_matching(	struct swindow *swindow,
							const char *regex,
							uint32_t options)
{
	int cflags = REG_EXTENDED;
	regex_t preg;
	dlist_t *cur;
	dlist_t *match_list = NULL;

	if (regex == NULL)
		return (-1);

	if (options & SWINDOW_FIND_ICASE)
		cflags |= REG_ICASE;

	if (options & SWINDOW_FIND_BASIC)
		cflags &= ~REG_EXTENDED;

	if (regcomp(&preg, regex, cflags) != 0)
		return (-1);

	for (cur = swindow->scrollbuf ; cur != NULL ; cur = cur->next) {
		struct imsg *imsg = cur->data;
		char *buf;

		buf = cstr_to_plaintext(imsg->text, imsg->len);
		if (buf != NULL) {
			if (regexec(&preg, buf, 0, NULL, 0) == 0)
				match_list = dlist_add_head(match_list, imsg);
			free(buf);
		}
	}

	regfree(&preg);

	/*
	** Better to compile a list of matches and print them after scanning
	** the whole buffer. If we started scanning at the oldest message and
	** printed matches as we traversed the scrollbuffer list, bad interactions
	** with swindow_prune() could occur.
	*/
	cur = match_list;
	while (cur != NULL) {
		dlist_t *next = cur->next;

		swindow_add(swindow, imsg_copy(swindow, cur->data), MSG_TYPE_LASTLOG);
		free(cur);
		cur = next;
	}

	return (0);
}

/*
** Turn timestamping on or off, depending on the value of "value"
*/

inline void swindow_set_timestamp(struct swindow *swindow, uint32_t value) {
	swindow->timestamp = value;
}

inline void swindow_set_wordwrap(struct swindow *swindow, uint32_t value) {
	/*
	** Allow for updating after the continued char changed but the
	** wordwrap enabled setting didn't.
	*/
	swindow->wordwrap = value;
	swindow_recalculate(swindow, swindow->rows, swindow->cols);
	wclear(swindow->win);
	swindow_redraw(swindow);
}

/*
** dump everything in the window to the specified file.
*/

int swindow_dump_buffer(struct swindow *swindow, char *file) {
#ifndef WIN32
	int fd;
	dlist_t *cur;
	struct iovec wvec[2];

	if (swindow->scrollbuf_end == NULL)
		return (-1);

	fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0600);
	if (fd == -1)
		return (-1);

	wvec[1].iov_base = "\n";
	wvec[1].iov_len = 1;

	for (cur = swindow->scrollbuf_end ; cur != NULL ; cur = cur->prev) {
		struct imsg *imsg = cur->data;

		wvec[0].iov_base = cstr_to_plaintext(imsg->text, imsg->len);
		wvec[0].iov_len = imsg->len;

		if (writev(fd, wvec, 2) != (int) imsg->len + 1) {
			screen_err_msg("Error writing buffer to %s: %s",
				file, strerror(errno));
		}

		free(wvec[0].iov_base);
	}

	close(fd);
#endif
	return (0);
}

/*
** Set the window to log to "logfile"
*/

void swindow_set_logfile(struct swindow *swindow, char *logfile) {
	swindow->logfile = logfile;

	/*
	** If the window is currently being logged, close
	** the log having the old filename, and open a new
	** one.
	*/

	if (swindow->logged) {
		swindow_end_log(swindow);
		swindow_set_log(swindow);
	}
}

/*
** Turn logging for this window on, and open the logfile for writing.
*/

int swindow_set_log(struct swindow *swindow) {
	int fd;
	time_t cur_time;
	struct tm *tm;
	char timebuf[128];
	uint32_t len;

	if (swindow->logfile == NULL) {
		screen_err_msg("No logfile has been specified for this window");
		return (-1);
	}

	fd = open(swindow->logfile, O_CREAT | O_APPEND | O_WRONLY, 0600);
	if (fd == -1) {
		swindow->logged = 0;
		screen_err_msg("Unable to open %s for writing: %s",
			swindow->logfile, strerror(errno));
		return (-1);
	}

	cur_time = time(NULL);
	tm = localtime(&cur_time);

	len = strftime(timebuf, sizeof(timebuf),
		"\n---------- Log started on %a %b %d %T %Z %Y ----------\n\n", tm);
	write(fd, timebuf, len);

	swindow->log_fd = fd;
	swindow->logged = 1;

	return (0);
}

/*
** End the log for this window.
*/

void swindow_end_log(struct swindow *swindow) {
	time_t cur_time;
	struct tm *tm;
	char timebuf[128];
	uint32_t len;

	if (swindow->log_fd == -1)
		return;

	cur_time = time(NULL);
	tm = localtime(&cur_time);

	len = strftime(timebuf, sizeof(timebuf),
		"\n---------- Log ended on %a %b %d %T %Z %Y ----------\n\n", tm);
	write(swindow->log_fd, timebuf, len);

	close(swindow->log_fd);
	swindow->logged = 0;
}

/*
** Clear the window. This amounts to filling the screen with one
** screen full of blank lines. The window can be scrolled up to
** view text that was in the window before it was cleared. When the
** window is scrolled back down, the blank lines that were inserted
** will go away.
*/

void swindow_clear(struct swindow *swindow) {
	struct imsg *imsg;

	if (swindow->scrollbuf == NULL)
		return;

	imsg = swindow->scrollbuf->data;

	swindow->scrollbuf_top = swindow->scrollbuf;
	swindow->scrollbuf_bot = swindow->scrollbuf;
	swindow->top_hidden = imsg->lines;
	swindow->bottom_hidden = 0;
	swindow->held = 0;
	swindow->bottom_blank = swindow->rows;
	swindow->dirty = 1;

	wclear(swindow->win);
}

/*
** Cleanup function for use with dlist_destroy()
*/

static void swindow_free(void *param __notused, void *data) {
	struct imsg *imsg = data;

	free(imsg->text);
	free(imsg);
}

/*
** Remove all the messages from the scroll buffer and clear
** the screen.
*/

void swindow_erase(struct swindow *swindow) {
	dlist_destroy(swindow->scrollbuf, NULL, swindow_free);

	swindow->scrollbuf = NULL;
	swindow->scrollbuf_top = NULL;
	swindow->scrollbuf_bot = NULL;
	swindow->scrollbuf_end = NULL;
	swindow->top_hidden = 0;
	swindow->bottom_hidden = 0;
	swindow->scrollbuf_len = 0;
	swindow->scrollbuf_lines = 0;
	swindow->held = 0;
	swindow->serial = 0;
	swindow->activity = 0;
	swindow->bottom_blank = swindow->rows;
	swindow->dirty = 1;

	wclear(swindow->win);
}

int swindow_destroy(struct swindow *swindow) {
	if (swindow->logged)
		swindow_end_log(swindow);

	dlist_destroy(swindow->scrollbuf, NULL, swindow_free);
	delwin(swindow->win);

	return (0);
}
