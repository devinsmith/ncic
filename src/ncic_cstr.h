/*
** Heavily based on pork_cstr.h
** ncic_cstr.h - routines for dealing with strings of type chtype *.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_CSTR_H__
#define __NCIC_CSTR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PORK_TABSTOP		4

#define chtype_set(x, c)	((x) = ((x) & ~A_CHARTEXT) | (c))
#define chtype_get(x)		((x) & A_CHARTEXT)
#define chtype_ctrl(x)		(((x) + 'A' - 1) | A_REVERSE)

size_t cstrlen(chtype *ch);
char *cstr_to_plaintext(const chtype *cstr, size_t n);

int plaintext_to_cstr(chtype *ch, size_t len, ...);
int plaintext_to_cstr_nocolor(chtype *ch, size_t len, ...);

size_t wputstr(WINDOW *win, chtype *ch);
size_t wputnstr(WINDOW *win, chtype *ch, size_t n);
size_t wputncstr(WINDOW *win, char *str, size_t n);
size_t mvwputstr(WINDOW *win, int y, int x, chtype *ch);
size_t mvwputnstr(WINDOW *win, int y, int x, chtype *ch, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_CSTR_H__ */
