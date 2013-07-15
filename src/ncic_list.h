/*
** Heavily based on pork_list.h
** ncic_list.h - Generic doubly linked lists and hash tables.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_LIST_H__
#define __NCIC_LIST_H__

#include <sys/types.h>
#include <stdint.h>

typedef struct dlist {
	struct dlist *next;
	struct dlist *prev;
	void *data;
} dlist_t;

dlist_t *dlist_add_head(dlist_t *head, void *data);
void dlist_destroy(dlist_t *head, void *param, void (*cleanup)(void *, void *));
void *dlist_remove_head(dlist_t **list);
dlist_t *dlist_remove(dlist_t *head, dlist_t *node);
dlist_t *dlist_find(dlist_t *head, void *data, int (*comp)(void *, void *));
dlist_t *dlist_add_after(dlist_t *head, dlist_t *node, void *data);
dlist_t *dlist_add_tail(dlist_t *head, void *data);
dlist_t *dlist_tail(dlist_t *head);
void dlist_iterate(dlist_t *head, void (*func)(void *, void *), void *data);
size_t dlist_len(dlist_t *head);

typedef struct hash {
	uint32_t order;
	int (*compare)(void *l, void *r);
	void (*remove)(void *param, void *data);
	dlist_t **map;
} hash_t;

int hash_init(	hash_t *hash,
				uint32_t order,
				int (*compare)(void *, void *),
				void (*rem)(void *param, void *data));

dlist_t *hash_find(hash_t *hash, void *data, uint32_t cur_hash);
void hash_add(hash_t *hash, void *data, uint32_t cur_hash);
int hash_remove(hash_t *hash, void *data, uint32_t cur_hash);
void hash_clear(hash_t *hash);
void hash_destroy(hash_t *hash);
int hash_exists(hash_t *hash, void *data, uint32_t cur_hash);
void hash_iterate(hash_t *hash, void (*func)(void *, void *), void *data);

#endif /* __NCIC_LIST_H__ */
