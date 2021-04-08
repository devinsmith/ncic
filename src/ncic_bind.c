/*
** Based on pork_bind.c
** ncic_bind.c - interface to key bindings
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <unistd.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_input.h"
#include "ncic_bind.h"

/*
** Names for some non-alphanumeric characters.
*/

static const struct keyval {
	char *name;
	int key;
} keyval[] = {
	{ "BACKSPACE",			KEY_BACKSPACE	},
	{ "BREAK",				KEY_BREAK		},
	{ "DELETE",				KEY_DC			},
	{ "DOWN_ARROW",			KEY_DOWN		},
	{ "END",				KEY_END			},
	{ "ENTER",				KEY_ENTER		},
	{ "F1",					KEY_F(1)		},
	{ "F2",					KEY_F(2)		},
	{ "F3",					KEY_F(3)		},
	{ "F4",					KEY_F(4)		},
	{ "F5",					KEY_F(5)		},
	{ "F6",					KEY_F(6)		},
	{ "F7",					KEY_F(7)		},
	{ "F8",					KEY_F(8)		},
	{ "F9",					KEY_F(9)		},
	{ "F10",				KEY_F(10)		},
	{ "F11",				KEY_F(11)		},
	{ "F12",				KEY_F(12)		},
	{ "HOME",				KEY_HOME		},
	{ "INSERT",				KEY_IC			},
	{ "LEFT_ARROW",			KEY_LEFT		},
	{ "PAGE_DOWN",			KEY_NPAGE		},
	{ "PAGE_UP",			KEY_PPAGE		},
	{ "RETURN",				'\n'			},
	{ "RIGHT_ARROW",		KEY_RIGHT		},
	{ "SPACE",				' '				},
	{ "SUSPEND",			KEY_SUSPEND		},
	{ "TAB",				'\t'			},
	{ "UP_ARROW",			KEY_UP			},
};

static int bind_compare(void *l, void *r) {
	int key = (intptr_t)(l);
	struct binding *binding = r;

	return (key - binding->key);
}

static int key_compare(const void *l, const void *r) {
	const char *str = l;
	const struct keyval *kv = r;

	return (strcasecmp(str, kv->name));
}

static void bind_hash_remove(void *param __notused, void *data) {
	struct binding *binding = data;

	free(binding->binding);
	free(binding);
}

/*
** If a binding is found for the key "key", execute
** the bind_set->success() function, passing it
** the binding.
**
** If no binding is found for "key", execute
** the bind_set->failure() function, passing it
** the key for which no binding was found.
*/

int bind_exec(struct key_binds *bind_set, int key) {
	struct binding *binding;

	binding = bind_find(bind_set, key);
	if (binding == NULL) {
		if (bind_set->failure != NULL)
			bind_set->failure(key);
	} else {
		if (bind_set->success != NULL)
			bind_set->success(binding);
	}

	return (0);
}

/*
** Remove the binding for key "key" if it exists.
*/

inline int bind_remove(struct key_binds *bind_set, int key) {
	int ret;

	ret = hash_remove(&bind_set->hash,
                    (void *)(intptr_t)(key), int_hash(key, bind_set->hash.order));

	return (ret);
}

/*
** Execute the command "command" when key "key" is pressed.
*/

void bind_add(struct key_binds *bind_set, int key, char *command) {
	u_int32_t hash = int_hash(key, bind_set->hash.order);
	struct binding *binding = xmalloc(sizeof(*binding));

	bind_remove(bind_set, key);

	binding->key = key;
	binding->binding = xstrdup(command);
	hash_add(&bind_set->hash, binding, hash);
}

/*
** Set some default bindings. This only
** sets the most essential bindings. The rest
** are read from the system-wide ncicrc file.
*/

static void bind_add_default(struct binds *binds) {
	bind_add(&binds->main, '\n', "input send");
	bind_add(&binds->main, 127, "input backspace");
	bind_add(&binds->main, KEY_BACKSPACE, "input backspace");
}

/*
** Initialize the bind hash.
*/

inline int bind_init(struct binds *binds) {
	memset(binds, 0, sizeof(*binds));

	if (hash_init(&binds->main.hash, 5, bind_compare, bind_hash_remove) != 0)
		return (-1);

	if (hash_init(&binds->blist.hash, 3, bind_compare, bind_hash_remove) != 0)
		return (-1);

	bind_add_default(binds);
	return (0);
}

void bind_destroy(struct binds *binds) {
	hash_destroy(&binds->main.hash);
	hash_destroy(&binds->blist.hash);
}

inline void bind_set_handlers(	struct key_binds *bind_set,
								void (*success)(struct binding *binding),
								void (*failure)(int key))
{
	bind_set->success = success;
	bind_set->failure = failure;
}

/*
** Find the binding for the key, "key"
*/

struct binding *bind_find(struct key_binds *bind_set, int key) {
	dlist_t *node;
	u_int32_t hash = int_hash(key, bind_set->hash.order);

	node = hash_find(&bind_set->hash, (void *)(intptr_t)(key), hash);
	if (node != NULL)
		return (node->data);

	return (NULL);
}

/*
** Translate a key's name to its character code.
** (i.e. ^A -> 0x01)
**
** This is messy.
**
** Basically ^X == ^x, META-^X == META-^x, META-X != META-x, X != x.
*/

int bind_get_keycode(char *keystr) {
	struct keyval *kv;
	int key;

	kv = bsearch(keystr, keyval, array_elem(keyval), sizeof(struct keyval),
			key_compare);

	if (kv != NULL)
		return (kv->key);

	if (!strncasecmp(keystr, "0x", 2)) {
		char *end;
		int val = strtol(keystr, &end, 16);

		if (*end != '\0')
			return (-1);

		return (val);
	}

	if (!strncasecmp(keystr, "META", 4)) {
		unsigned int meta_num = 1;

		keystr += 4;

		if (keystr[0] == '-')
			keystr++;
		else if (isdigit(keystr[0])) {
			char meta_str[128];
			char *p;

			if (xstrncpy(meta_str, keystr, sizeof(meta_str)) == -1)
				return (-1);

			p = strchr(meta_str, '-');
			if (p == NULL)
				return (-1);
			*p++ = '\0';

			if (str_to_uint(meta_str, &meta_num) == -1 || meta_num > 15)
				return (-1);

			keystr = p;
		} else
			return (-1);

		kv = bsearch(keystr, keyval, array_elem(keyval), sizeof(struct keyval),
				key_compare);

		if (kv != NULL)
			return (META_KEY(kv->key, meta_num));

		if (keystr[0] == '^' && keystr[1] != '\0' && keystr[2] == '\0')
			key = META_KEY(CTRL_KEY(toupper(keystr[1])), meta_num);
		else if (keystr[0] != '\0' && keystr[1] == '\0')
			key = META_KEY(keystr[0], meta_num);
		else if (!strncasecmp(keystr, "0x", 2)) {
			char *end;
			int val = strtol(keystr, &end, 16);

			if (*end != '\0')
				key = -1;
			else
				key = META_KEY(val, meta_num);
		} else
			key = -1;

		return (key);
	}

	if (keystr[0] == '^' && keystr[1] != '\0' && keystr[2] == '\0')
		return (CTRL_KEY(toupper(keystr[1])));

	if (keystr[1] == '\0')
		return (keystr[0]);

	return (-1);
}

/*
** Given a key code, fill in "result" with its name.
** This is used when listing the current bindings.
*/

void bind_get_keyname(int key, char *result, size_t len) {
	u_int32_t i;
	char buf[32];
	int meta_num;

	result[0] = '\0';

	meta_num = META_NUM(key);
	if (meta_num != 0) {
		if (meta_num > 1)
			snprintf(result, len, "META%d-", meta_num);
		else
			xstrncpy(result, "META-", len);

		key &= ~META_MASK;
	}

	for (i = 0 ; i < array_elem(keyval) ; i++) {
		if (key == keyval[i].key) {
			xstrncat(result, keyval[i].name, len);
			return;
		}
	}

	if (key <= 0xff && iscntrl(key)) {
		char ctrl_key = key + 'A' - 1;

		if (isprint(ctrl_key)) {
			if (isalpha(ctrl_key))
				ctrl_key = toupper(ctrl_key);

			snprintf(buf, sizeof(buf), "^%c", ctrl_key);
		} else
			snprintf(buf, sizeof(buf), "0x%02x", key);
	} else if (key <= 0xff && isprint(key))
		snprintf(buf, sizeof(buf), "%c", key);
	else
		snprintf(buf, sizeof(buf), "0x%02x", key);

	xstrncat(result, buf, len);
}
