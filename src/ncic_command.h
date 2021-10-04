/*
** Based on pork_command.h
** ncic_command.h - interface to commands typed by the user
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_COMMAND_H__
#define __NCIC_COMMAND_H__

#define USER_COMMAND(x)	void x (char *args __notused)

struct command {
	char *name;
	void (*cmd)(char *);
};

int run_mcommand(char *str);
int run_command(char *str);

#endif /* __NCIC_COMMAND_H__ */
