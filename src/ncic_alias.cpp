/*
** Based on pork_alias.c
** ncic_alias.c - interface to aliases.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <cstring>
#include <cstdlib>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_alias.h"

static int alias_compare(void *l, void *r) {
	const char *str = static_cast<const char *>(l);
	const struct alias *alias = static_cast<struct alias *>(r);

	return (strcasecmp(str, alias->alias));
}

static void alias_hash_remove(void *param __notused, void *data) {
	auto *alias = static_cast<struct alias *>(data);

	free(alias->alias);
	free(alias->orig);
	free(alias->args);
	free(alias);
}

inline int alias_remove(hash_t *alias_hash, char *alias) {
	int ret;

	ret = hash_remove(alias_hash, alias,
			string_hash(alias, alias_hash->order));

	return (ret);
}

int alias_add(hash_t *alias_hash, char *alias, char *str) {
	char *temp;
	char *args;
	u_int32_t hash;
	struct alias *new_alias;

	alias_remove(alias_hash, alias);

	new_alias = (struct alias *)xmalloc(sizeof(*new_alias));
	new_alias->alias = xstrdup(alias);

	temp = xstrdup(str);

	args = strchr(temp, ' ');
	if (args != nullptr) {
		new_alias->args = xstrdup(args);
		*args = '\0';
	} else
		new_alias->args = nullptr;

	new_alias->orig = xstrdup(temp);

	hash = string_hash(alias, alias_hash->order);
	hash_add(alias_hash, new_alias, hash);

	free(temp);
	return (0);
}

struct alias *alias_find(hash_t *alias_hash, char *str) {
	dlist_t *node;
	u_int32_t hash = string_hash(str, alias_hash->order);

	node = hash_find(alias_hash, str, hash);
	if (node != nullptr)
		return (struct alias *)(node->data);

	return (nullptr);
}

/*
** Like strcpy(3), only it doesn't terminate strings.
*/

static inline void alias_strcpy(char *dest, char *src, size_t len) {
	char save = dest[len];

	strcpy(dest, src);
	dest[len] = save;
}

/*
** This resolves an alias. Given a string "<command> <args>"
** if <command> is an alias, this function will look up the
** alias it refers to, then effectively replace <command> with
** its alias and its alias' arguments. The function will iterate
** until the first word is no longer an alias.
**
** Returns -1 if the original string wasn't an alias.
**
** Returns 1 if we ran out of space while pasting together a chain
** of aliases.
**
** Returns 0 on success.
*/

int alias_resolve(hash_t *alias_hash, char *str, char **result) {
	static char buf[8192];
	u_int32_t num_left = sizeof(buf) - 1;
	char *p;
	u_int32_t iterations;

	p = strchr(str, ' ');
	if (p != nullptr)
		*p = '\0';

	/*
	** Avoid doing extra work if first word of the string
	** passed to us isn't an alias.
	*/

	if (alias_find(alias_hash, str) == nullptr) {
		if (p != nullptr)
			*p = ' ';
		return (-1);
	}

	buf[num_left] = '\0';

	if (p != nullptr) {
		size_t len;

		*p = ' ';
		len = strlen(p);
		if (len > num_left)
			return (1);

		num_left -= len;
		alias_strcpy(&buf[num_left], p, len);
		*p = '\0';
	}

	/*
	** Keep people from doing stupid things.
	** I can't imagine this number has to be anywhere near as high
	** as 20000, but it's not like it matters. If they want to waste
	** some CPU time, let them, just don't loop forever.
	*/

	for (iterations = 0 ; iterations < 20000 ; iterations++) {
		struct alias *alias;

		/*
		** If the current string is not an alias,
		** we've completely resolved the original alias string.
		*/

		alias = alias_find(alias_hash, str);
		if (alias == nullptr) {
			size_t len = strlen(str);

			if (len > num_left)
				return (1);

			num_left -= len;
			alias_strcpy(&buf[num_left], str, len);

			*result = &buf[num_left];
			return (0);
		}

		if (alias->args != nullptr) {
			size_t len = strlen(alias->args);

			if (len > num_left)
				return (1);

			num_left -= len;
			alias_strcpy(&buf[num_left], alias->args, len);
		}

		str = alias->orig;
	}

	return (1);
}

int alias_init(hash_t *alias_hash) {
	memset(alias_hash, 0, sizeof(*alias_hash));
	return (hash_init(alias_hash, 5, alias_compare, alias_hash_remove));
}
