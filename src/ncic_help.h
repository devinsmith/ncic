/*
** Based on pork_help.h
** ncic_help.h - /help command implementation.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_HELP_H__
#define __NCIC_HELP_H__

#ifdef __cplusplus
extern "C" {
#endif

#define HELP_TABSTOP			4
#define HELP_SECTION_STYLE		"%W"
#define HELP_HEADER_STYLE		"%W"

int pork_help_print(const char *section, char *command);
int pork_help_get_cmds(const char *section, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_HELP_H__ */
