/*
** Based on pork_status.h
** ncic_status.h - status bar implementation.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_STATUS_H__
#define __NCIC_STATUS_H__

#define STATUS_ROWS 2

struct pork_acct;

int status_init(void);
void status_draw(struct pork_acct *acct);

#endif /* __NCIC_STATUS_H__ */
