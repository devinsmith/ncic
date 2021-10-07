/*
** Based on pork_status.c
** ncic_status.c - status bar implementation.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <cstring>

#include "ncic_util.h"
#include "ncic_misc.h"
#include "ncic_set.h"
#include "ncic_cstr.h"
#include "ncic_imwindow.h"
#include "ncic_screen.h"
#include "ncic_status.h"
#include "ncic_format.h"

extern struct screen screen;

int status_init(void) {
	WINDOW *win;

	win = newwin(STATUS_ROWS, screen.cols, screen.rows - STATUS_ROWS, 0);
	if (win == nullptr)
		return (-1);

	set_default_win_opts(win);
	screen.status_bar = win;
	return (0);
}

/*
** Draw the status bar, using the format string OPT_FORMAT_STATUS. This
** parses the format string every time the status bar is redrawn, which
** could waste some cpu time, except that today's computers are sickeningly fast
** and the status bar isn't redrawn all that much anyway.
*/

void status_draw(struct pork_acct *acct) {
	char buf[1024];
	chtype status_bar[screen.cols + 1];
	struct imwindow *win = cur_window();
	int type;

	if (win->type == WIN_TYPE_CHAT)
		type = OPT_FORMAT_STATUS_CHAT;
	else
		type = OPT_FORMAT_STATUS;

	fill_format_str(type, buf, sizeof(buf), win, acct);
	format_apply_justification(buf, status_bar, array_elem(status_bar));

	mvwputstr(screen.status_bar, 0, 0, status_bar);
	wnoutrefresh(screen.status_bar);
}
