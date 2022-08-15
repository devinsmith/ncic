/*
** Heavily based on pork_cstr.c
** ncic_cstr.c - routines for dealing with strings of type chtype *.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <sys/types.h>

#include "ncic_util.h"
#include "ncic_color.h"
#include "ncic_cstr.h"

/*
** I define what I call "cstrings" for
** use in this program.
**
** ncurses uses a data type called "chtype" to hold
** a character with attribute set (f.e. having color, being bold,
** underlined, etc).
**
** I define an array of these characters that's terminated with a chtype
** character having the value of zero as a "cstr"
*/

/*
** Return the length of the cstring pointed
** by "ch"
*/

size_t cstrlen(chtype *ch) {
	size_t i = 0;

	while (*ch++ != 0)
		++i;

	return (i);
}

/*
** Convert the cstring specified by "cstr"
** to its ASCII representation. This will result
** in the loss of all attributes of the characters
** that comprise it.
*/

char *cstr_to_plaintext(const chtype *cstr, size_t len) {
	char *str = (char *)xmalloc(len + 1);
	size_t i;

	for (i = 0 ; i < len && cstr[i] != 0 ; i++)
		str[i] = chtype_get(cstr[i]);

	str[i] = '\0';
	return (str);
}

/*
** The reverse of above. The characters that comprise the cstring to
** be constructed will have their attributes set according to a format
** string starting with % and then containing color information in the
** form <foreground>[,<background>]. See the function color_parse_code() for
** more information.
**
** Returns the length of the string.
*/

int plaintext_to_cstr(chtype *ch, size_t len, ...) {
	char *str;
	u_int32_t i = 0;
	va_list ap;

	va_start(ap, len);

	len--;

	while ((str = va_arg(ap, char *)) != nullptr) {
		u_int32_t spos = 0;
		attr_t color_attr = 0;

		for (; i < len && str[spos] != '\0' ; i++) {
			if (str[spos] == '%') {
				if (str[spos + 1] != '\0' && str[++spos] != '%') {
					int ret = color_parse_code(&str[spos], &color_attr);
					if (ret == -1)
						ch[i] = (unsigned char)str[--spos];
					else {
						i--;
						spos += ret;
						continue;
					}
				} else
					ch[i] = (unsigned char)str[spos];
			} else if (str[spos] == '\t') {
				size_t pad = PORK_TABSTOP - i % PORK_TABSTOP;
				size_t j;

				for (j = 0 ; j < pad && i < len ; j++)
					ch[i++] = ' ' | color_attr;
				i--;
			} else
				ch[i] = (unsigned char)str[spos];

			ch[i] |= color_attr;

			spos++;
		}
	}

	va_end(ap);

	ch[i] = 0;
	return (i);
}

/*
** The same as above, only color attributes are ignored.
** Returns the length of the string.
*/

int plaintext_to_cstr_nocolor(chtype *ch, size_t len, ...) {
	char *str;
	u_int32_t i = 0;
	va_list ap;

	len--;

	va_start(ap, len);

	while ((str = va_arg(ap, char *)) != nullptr) {
		for (; i < len && *str != '\0' ; i++) {
			if (*str == '\t') {
				size_t pad = PORK_TABSTOP - i % PORK_TABSTOP;
				size_t j;

				for (j = 0 ; j < pad && i < len ; j++)
					ch[i++] = ' ';
				i--;
			} else
				ch[i] = (unsigned char)*str;

			str++;
		}
	}

	va_end(ap);

	ch[i] = 0;
	return (i);
}


/*
** Write the cstring pointed to by "ch"
** to the screen at the current cursor position.
*/

inline size_t wputstr(WINDOW *win, chtype *ch) {
	size_t i = 0;

	while (ch[i] != 0) {
		int c = chtype_get(ch[i]);

		if (iscntrl(c)) {
			waddch(win, chtype_ctrl(c));
		} else
			waddch(win, ch[i]);

		i++;
	}

	return (i);
}

/*
** Write the cstring pointed to by "ch"
** to the screen at the position (x, y).
*/

size_t mvwputstr(WINDOW *win, int y, int x, chtype *ch) {
	wmove(win, y, x);

	return (wputstr(win, ch));
}

/*
** Write the first n chtype chars of the cstring pointed to by "ch"
** to the screen at the current cursor position.
*/

size_t wputnstr(WINDOW *win, chtype *ch, size_t n) {
	size_t i;

	for (i = 0 ; i < n && ch[i] != 0 ; i++) {
		int c = chtype_get(ch[i]);

		if (iscntrl(c)) {
			waddch(win, chtype_ctrl(c));
		} else
			waddch(win, ch[i]);
	}

	return (i);
}

size_t wputncstr(WINDOW *win, char *str, size_t n) {
	size_t i;

	for (i = 0 ; i < n && *str != '\0' ; i++) {
		if (iscntrl(*str))
			waddch(win, chtype_ctrl(*str));
		else
			waddch(win, (unsigned char)*str);

		str++;
	}

	return (i);
}

/*
** Write the first n chtype chars of the cstring pointed to by "ch"
** to the screen at position (x, y).
*/

size_t mvwputnstr(WINDOW *win, int y, int x, chtype *ch, size_t n) {
	wmove(win, y, x);

	return (wputnstr(win, ch, n));
}
