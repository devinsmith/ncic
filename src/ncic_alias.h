/*
** Based on pork_alias.h
** ncic_alias.h - interface to aliases.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_ALIAS_H__
#define __NCIC_ALIAS_H__

struct alias {
	char *alias;
	char *orig;
	char *args;
};

inline int alias_init(hash_t *alias_hash);
struct alias *alias_find(hash_t *alias_hash, char *str);
int alias_add(hash_t *alias_hash, char *alias, char *str);
int alias_remove(hash_t *alias_hash, char *alias);
int alias_resolve(hash_t *alias_hash, char *str, char **result);

#endif /* __NCIC_ALIAS_H__ */
