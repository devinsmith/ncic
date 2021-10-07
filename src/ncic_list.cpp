/*
** Heavily based on pork_list.c
** ncic_list.c - Generic doubly linked lists and hash tables.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <unistd.h>
#include <cstdlib>

#include "ncic_util.h"
#include "ncic_list.h"

/*
** Add a node containing the data specified in "data"
** to the head of the list specified by "head"
*/

dlist_t *dlist_add_head(dlist_t *head, void *data) {
	dlist_t *new_node = (dlist_t *)xmalloc(sizeof(dlist_t));

	new_node->data = data;
	new_node->prev = NULL;
	new_node->next = head;

	if (head != NULL)
		head->prev = new_node;

	return (new_node);
}

/*
** Remove "node" from the list pointed to by "head"
*/

dlist_t *dlist_remove(dlist_t *head, dlist_t *node) {
	dlist_t *ret = head;

	if (node->prev != nullptr)
		node->prev->next = node->next;
	else
		ret = node->next;

	if (node->next != nullptr)
		node->next->prev = node->prev;

	free(node);
	return (ret);
}

/*
** Destroy the list pointed to by "head", run the "cleanup"
** function on the data in each node, if it's specified.
*/

void dlist_destroy(	dlist_t *head,
					void *param,
					void (*cleanup)(void *, void *))
{
	dlist_t *cur = head;

	while (cur != nullptr) {
		dlist_t *next = cur->next;

		if (cleanup != nullptr)
			cleanup(param, cur->data);

		free(cur);

		cur = next;
	}
}

static int dlist_default_compare(void *l, void *r) {
	if (l != r)
		return (-1);

	return (0);
}

/*
** Find the node in the list specified by "head"
** whose data field is "data". Use the function
** comp() to determine if the fields are the same.
*/

dlist_t *dlist_find(dlist_t *head, void *data, int (*comp)(void *, void *)) {
	dlist_t *cur;

	if (comp == nullptr)
		comp = dlist_default_compare;

	for (cur = head ; cur != nullptr ; cur = cur->next) {
		if (comp(data, cur->data) == 0)
			return (cur);
	}

	return (nullptr);
}

/*
** Iterate through the list specified by "head", calling
** the function func(node, "data") for each node.
*/

void dlist_iterate(dlist_t *head, void (*func)(void *, void *), void *data) {
	dlist_t *cur;

	for (cur = head ; cur != nullptr ; cur = cur->next)
		func(cur->data, data);
}

int hash_init(	hash_t *hash,
				uint32_t order,
				int (*compare)(void *, void *),
				void (*rem)(void *param, void *data))
{
	if (compare == nullptr)
		return (-1);

	if (order > sizeof(long) * 4)
		return (-1);

	hash->order = order;
	hash->compare = compare;
	hash->remove = rem;
	hash->map = (dlist_t **)xcalloc((uint32_t) (1 << order), sizeof(dlist_t *));

	return (0);
}

dlist_t *hash_find(hash_t *hash, void *data, uint32_t cur_hash) {
	return (dlist_find(hash->map[cur_hash], data, hash->compare));
}

void hash_add(hash_t *hash, void *data, uint32_t cur_hash) {
	hash->map[cur_hash] = dlist_add_head(hash->map[cur_hash], data);
}

int hash_remove(hash_t *hash, void *data, uint32_t cur_hash) {
	dlist_t *node = hash_find(hash, data, cur_hash);

	if (node == nullptr)
		return (-1);

	if (hash->remove != nullptr)
		hash->remove(nullptr, node->data);

	hash->map[cur_hash] = dlist_remove(hash->map[cur_hash], node);
	return (0);
}

void hash_clear(hash_t *hash) {
	if (hash->order > 0) {
		uint32_t i;

		for (i = 0 ; i < (uint32_t) (1 << hash->order) ; i++) {
			dlist_destroy(hash->map[i], nullptr, hash->remove);
			hash->map[i] = nullptr;
		}
	}
}

void hash_destroy(hash_t *hash) {
	hash_clear(hash);
	free(hash->map);
}

void hash_iterate(hash_t *hash, void (*func)(void *, void *), void *data) {
	uint32_t i;

	for (i = 0 ; i < (uint32_t) (1 << hash->order) ; i++)
		dlist_iterate(hash->map[i], func, data);
}