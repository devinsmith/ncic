/*
** Based on pork_color.h
** ncic_color.h - color functions.
** Copyright (C) 2002-2004 Amber Adams <amber@ojnk.net>
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_COLOR_H__
#define __NCIC_COLOR_H__

#ifdef __cplusplus
extern "C" {
#endif

void color_init(void);
int color_parse_code(const char *code, attr_t *attr);
int color_get_str(attr_t attr, char *buf, size_t len);
char *color_quote_codes(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_COLOR_H__ */
