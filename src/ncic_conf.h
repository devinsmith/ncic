/*
** Based on pork_conf.h
** ncic_conf.h - pork's configuration parser.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_CONF_H__
#define __NCIC_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

struct pork_acct;

int read_conf(const char *path);
int read_global_config(void);
int save_global_config(void);

int read_user_config(struct pork_acct *acct);
int save_user_config(struct pork_acct *acct);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_CONF_H__ */
