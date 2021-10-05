/*
** Heavily based on pork_color.c
** ncic_color.c - color functions.
** Copyright (C) 2002-2004 Amber Adams <amber@ojnk.net>
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <cctype>
#include <cstring>

#include "ncic_util.h"
#include "ncic_color.h"

static inline int color_get_code(char code);
static inline int color_get_highlighting(char code);

/*
** Initialize the colors that we're going to be using.
*/

void color_init(void) {
	int i;
	int n = 1;
	int can_use_default;

	start_color();
	can_use_default = use_default_colors();

	for (i = 0 ; i < 8 ; i++) {
		int bgcolor = i;
		int j;

		if (bgcolor == COLOR_BLACK && can_use_default == OK)
			bgcolor = -1;

		for (j = 0 ; j < 8 ; j++) {
			if (n < COLOR_PAIRS)
				init_pair(n++, j, bgcolor);
		}
	}
}

/*
** Takes a color code string and makes it into attributes which are stored
** where attr points. Returns the length of the color code that was read,
** if one was read successfully. Returns -1 if an invalid color code was
** encountered.
**
** The basic form of the color code is <foreground>[,<background>], where
** foreground and background are one of the following:
**
** d = black	D = bold black
** r = red		R = bold red
** g = green	G = bold green
** y = yellow	Y = bold yellow
** b = blue		B = bold blue
** m = magenta	M = bold magenta
** c = cyan		C = bold cyan
** w = white	W = bold white
**
** In addition, the following codes are also available:
** x = clear all attributes
**
** Highlighting modes:
**		1 = bold		2 = reverse		3 = underline
**		4 = blink		5 = standout	6 = dim
**
** Highlighting modes, unlike colors, cascade. For example, %b%1%2 is
** bold and reverse blue; %1%2%b is just blue.
**
** Highlighting modes can be removed with %-<code>. For example,
** %1this%-1 is a test will display test as bold and the rest of the
** text non-bold.
*/

int color_parse_code(const char *code, attr_t *attr) {
	int fgcol;
	int bgcol = 0;
	int ret = 1;
	attr_t new_attr = 0;

	if (*code == 'x') {
		*attr = 0;
		return (ret);
	}

	if (isdigit(*code)) {
		int highlighting;

		highlighting = color_get_highlighting(*code);
		if (highlighting == -1)
			return (-1);

		*attr |= highlighting;
		return (ret);
	}

	if (*code == '-' && isdigit(code[1])) {
		int highlighting;

		highlighting = color_get_highlighting(code[1]);
		if (highlighting == -1)
			return (-1);

		*attr &= ~highlighting;
		return (2);
	}

	fgcol = color_get_code(*code);
	if (fgcol == -1)
		return (-1);

	if (isupper(*code))
		new_attr |= A_BOLD;

	if (code[1] == ',') {
		bgcol = color_get_code(code[2]);
		if (bgcol == -1)
			bgcol = 0;
		else {
			/*
			** If a black background is specifically requested,
			** force it by swapping the foreground and background colors
			** and setting the reverse highlighting attribute.
			*/
			if (tolower(code[2]) == 'd') {
				bgcol = fgcol;
				fgcol = 0;
				new_attr |= A_REVERSE;
			}

			ret = 3;
		}
	}

	new_attr |= COLOR_PAIR((bgcol * 8 + fgcol) + 1);
	*attr = new_attr;
	return (ret);
}

static inline int color_get_code(char code) {
	static const char codes[] = { 4, 6, 0, -1, -1, 2, -1, -1, -1, -1, -1, 5, -1, -1, 5, -1, 1, -1, -1, -1, -1, 7, -1, 3 };

	code = tolower(code);
	if (code < 'b' || code > 'y')
		return (-1);

	return ((int) codes[code - 'b']);
}

static inline int color_get_highlighting(char code) {
	static const int colors[] = { A_BOLD, A_REVERSE, A_UNDERLINE, A_BLINK, A_STANDOUT, A_DIM };

	if (code < '1' || code > '6')
		return (-1);

	return (colors[code - '1']);
}

int color_get_str(attr_t attr, char *buf, size_t len) {
	int num;
	int fgcol;
	int bgcol;
	char c;
	int ret;
	int i = 0;
	static const char color_codes[] = "drgybmcw";

	num = PAIR_NUMBER(attr) - 1;
	fgcol = num % 8;
	bgcol = num / 8;

	c = color_codes[fgcol];
	if (attr & A_BOLD)
		c = toupper(c);

	ret = snprintf(&buf[i], len - i, "%%%c", c);
	if (ret < 0 || (size_t) ret >= len - i)
		return (-1);
	i += ret;

	if (bgcol != 0) {
		ret = snprintf(&buf[i], len - i, ",%c", color_codes[bgcol]);
		if (ret < 0 || (size_t) ret >= len - i)
			return (-1);
		i += ret;
	}

	if (attr & A_REVERSE) {
		ret = snprintf(&buf[i], len - i, "%%2");
		if (ret < 0 || (size_t) ret >= len - i)
			return (-1);
		i += ret;
	}

	if (attr & A_UNDERLINE) {
		ret = snprintf(&buf[i], len - i, "%%3");
		if (ret < 0 || (size_t) ret >= len - i)
			return (-1);
		i += ret;
	}

	if (attr & A_BLINK) {
		ret = snprintf(&buf[i], len - i, "%%4");
		if (ret < 0 || (size_t) ret >= len - i)
			return (-1);
		i += ret;
	}

	if (attr & A_STANDOUT) {
		ret = snprintf(&buf[i], len - i, "%%5");
		if (ret < 0 || (size_t) ret >= len - i)
			return (-1);
		i += ret;
	}

	if (attr & A_DIM) {
		ret = snprintf(&buf[i], len - i, "%%6");
		if (ret < 0 || (size_t) ret >= len - i)
			return (-1);
		i += ret;
	}

	return (0);
}

char *color_quote_codes(const char *str) {
	size_t len;
	size_t i = 0;
	char *p;

	if (strchr(str, '%') == nullptr)
		return (xstrdup(str));

	len = (strlen(str) * 3) / 2;
	p = (char *)xmalloc(len);

	--len;
	while (*str != '\0' && i < len) {
		if (*str == '%') {
			p[i++] = '%';

			if (i >= len)
				break;
		}

		p[i++] = *str++;
	}

	p[i] = '\0';
	return (p);
}
