/*
** Based on pork_misc.c
** ncic_misc.c - miscellaneous functions.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "ncic_util.h"
#include "ncic_input.h"
#include "ncic_misc.h"

void resize_terminal(int rows, int cols) {
#ifdef HAVE_RESIZETERM
	resizeterm(rows, cols);
#else
	endwin();
	doupdate();
#endif
}

/*
** Sets default options for windows.
*/

void set_default_win_opts(WINDOW *win) {
	leaveok(win, FALSE);
	scrollok(win, FALSE);
	meta(win, TRUE);
	keypad(win, TRUE);
	idlok(win, TRUE);
}

/*
** Setup the ncurses environment.
*/

int initialize_environment(void) {
	WINDOW *win;

	win = initscr();
	if (win == NULL)
		return (-1);

	halfdelay(10);
	noecho();
	raw();
	set_default_win_opts(win);

	return (0);
}

int wgetinput(WINDOW *win) {
	int c;
	int meta_cnt = 0;

	while ((c = wgetch(win)) == 0x1b)
		meta_cnt++;

	if (meta_cnt > 15)
		meta_cnt = 15;

	if (c == -1)
		return (-1);

	if (meta_cnt > 0)
		c = META_KEY(c, meta_cnt);

	return (c);
}

/*
** "Normalize" the string "str"
**
** AIM screen names need to be normalized
** before they're sent to the server.
**
** What this means is converting alphabetic characters
** to lowercase and removing any space characters.
*/

int normalize(char *buf, const char *str, size_t len) {
	size_t i;

	len--;
	for (i = 0 ; *str != '\0' && i < len ; str++) {
		if (*str == ' ')
			continue;

		buf[i++] = tolower(*str);
	}

	buf[i] = '\0';
	return (0);
}

int date_to_str(time_t timespec, char *buf, size_t len) {
	char *p;

	p = asctime(localtime(&timespec));
	if (p == NULL)
		return (-1);

	if (xstrncpy(buf, p, len) == -1)
		return (-1);

	p = strchr(buf, '\n');
	if (p != NULL)
		*p = '\0';

	return (0);
}

/*
** Convert "timespec" (in minutes) to a string of the
** form "DAYSd HOURSh MINm"
*/

int time_to_str(uint32_t timespec, char *buf, size_t len) {
	uint32_t days;
	uint32_t hours;
	uint32_t min;
	uint32_t i = 0;

	if (timespec == 0) {
		int ret;

		ret = snprintf(buf, len, "0m");
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}

		return (0);
	}

	days = timespec / (60 * 24);
	hours = (timespec % (60 * 24)) / 60;
	min = timespec - (days * (60 * 24)) - (hours * 60);

	if (days > 0) {
		int ret;

		ret = snprintf(buf, len, "%ud", days);
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}

		len -= ret;
		i += ret;
	}

	if (hours > 0) {
		int ret;

		ret = snprintf(&buf[i], len, "%uh", hours);
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}

		len -= ret;
		i += ret;
	}

	if (min > 0) {
		int ret;

		ret = snprintf(&buf[i], len, "%um", min);
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}
	}

	return (0);
}

int time_to_str_full(uint32_t timespec, char *buf, size_t len) {
	uint32_t days;
	uint32_t hours;
	uint32_t min;
	uint32_t seconds;
	uint32_t i = 0;

	days = timespec / (60 * 60 * 24);
	timespec -= days * (60 * 60 * 24);

	hours = timespec / (60 * 60);
	timespec -= hours * 60 * 60;

	min = timespec / 60;
	timespec -= min * 60;

	seconds = timespec;

	if (days > 0) {
		int ret;

		ret = snprintf(buf, len, "%u day%s ",
				days, (days != 1 ? "s" : ""));
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}

		len -= ret;
		i += ret;
	}

	if (hours > 0) {
		int ret;

		ret = snprintf(&buf[i], len, "%u hour%s ",
				hours, (hours != 1 ? "s" : ""));
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}

		len -= ret;
		i += ret;
	}

	if (min > 0) {
		int ret;

		ret = snprintf(&buf[i], len, "%u minute%s ",
				min, (min != 1 ? "s" : ""));
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}

		len -= ret;
		i += ret;
	}

	if (seconds > 0) {
		int ret;

		ret = snprintf(&buf[i], len, "%u second%s ",
				seconds, (seconds != 1 ? "s" : ""));
		if (ret < 0 || (size_t) ret >= len) {
			*buf = '\0';
			return (-1);
		}

		len -= ret;
		i += ret;
	}

	if (i > 0) {
		buf[i - 1] = '\0';
		return (0);
	}

	if (xstrncpy(buf, "0 seconds", len) == -1)
		return (-1);

	return (0);
}
