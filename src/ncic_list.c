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
#include <stdlib.h>

#include "ncic_util.h"
#include "ncic_list.h"

/*
** Returns the length of the list whose head
** node is passed in.
*/

size_t dlist_len(dlist_t *head) {
	size_t len = 0;

	while (head != NULL) {
		++len;
		head = head->next;
	}

	return (len);
}

/*
** Add a node containing the data specified in "data"
** to the head of the list specified by "head"
*/

dlist_t *dlist_add_head(dlist_t *head, void *data) {
	dlist_t *new_node = xmalloc(sizeof(dlist_t));

	new_node->data = data;
	new_node->prev = NULL;
	new_node->next = head;

	if (head != NULL)
		head->prev = new_node;

	return (new_node);
}

/*
** Add a node containing the data specified in "data"
** to the list specified by "head" after the node "node"
*/

dlist_t *dlist_add_after(dlist_t *head, dlist_t *node, void *data) {
	dlist_t *new_node = xmalloc(sizeof(dlist_t));

	new_node->data = data;
	new_node->prev = node;
	new_node->next = node->next;

	if (node->next != NULL)
		node->next->prev = new_node;

	node->next = new_node;

	return (head);
}

/*
** Return the last node in the list
** specified by "head"
*/

dlist_t *dlist_tail(dlist_t *head) {
	dlist_t *cur = head;

	if (cur == NULL)
		return (head);

	while (cur->next != NULL)
		cur = cur->next;

	return (cur);
}

/*
** Add a node containing the data specified in "data"
** to the tail of the list specified by "head"
*/

dlist_t *dlist_add_tail(dlist_t *head, void *data) {
	dlist_t *tail = dlist_tail(head);

	if (tail == NULL)
		return (dlist_add_head(head, data));

	return (dlist_add_after(head, tail, data));
}

/*
** Remove the node that's at the head of the list
** and adjust the pointer to the the head of the list
** to point to the node after the one we removed.
*/

void *dlist_remove_head(dlist_t **list_head) {
	dlist_t *next_node = (*list_head)->next;
	void *ret = (*list_head)->data;

	free(*list_head);

	*list_head = next_node;
	if (next_node != NULL)
		next_node->prev = NULL;

	return (ret);
}

/*
** Remove "node" from the list pointed to by "head"
*/

dlist_t *dlist_remove(dlist_t *head, dlist_t *node) {
	dlist_t *ret = head;

	if (node->prev != NULL)
		node->prev->next = node->next;
	else
		ret = node->next;

	if (node->next != NULL)
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

	while (cur != NULL) {
		dlist_t *next = cur->next;

		if (cleanup != NULL)
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

	if (comp == NULL)
		comp = dlist_default_compare;

	for (cur = head ; cur != NULL ; cur = cur->next) {
		if (comp(data, cur->data) == 0)
			return (cur);
	}

	return (NULL);
}

/*
** Iterate through the list specified by "head", calling
** the function func(node, "data") for each node.
*/

void dlist_iterate(dlist_t *head, void (*func)(void *, void *), void *data) {
	dlist_t *cur;

	for (cur = head ; cur != NULL ; cur = cur->next)
		func(cur->data, data);
}

int hash_init(	hash_t *hash,
				uint32_t order,
				int (*compare)(void *, void *),
				void (*rem)(void *param, void *data))
{
	if (compare == NULL)
		return (-1);

	if (order > sizeof(long) * 4)
		return (-1);

	hash->order = order;
	hash->compare = compare;
	hash->remove = rem;
	hash->map = xcalloc((uint32_t) (1 << order), sizeof(dlist_t *));

	return (0);
}

inline dlist_t *hash_find(hash_t *hash, void *data, uint32_t cur_hash) {
	return (dlist_find(hash->map[cur_hash], data, hash->compare));
}

inline void hash_add(hash_t *hash, void *data, uint32_t cur_hash) {
	hash->map[cur_hash] = dlist_add_head(hash->map[cur_hash], data);
}

int hash_remove(hash_t *hash, void *data, uint32_t cur_hash) {
	dlist_t *node = hash_find(hash, data, cur_hash);

	if (node == NULL)
		return (-1);

	if (hash->remove != NULL)
		hash->remove(NULL, node->data);

	hash->map[cur_hash] = dlist_remove(hash->map[cur_hash], node);
	return (0);
}

void hash_clear(hash_t *hash) {
	if (hash->order > 0) {
		uint32_t i;

		for (i = 0 ; i < (uint32_t) (1 << hash->order) ; i++) {
			dlist_destroy(hash->map[i], NULL, hash->remove);
			hash->map[i] = NULL;
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

inline int hash_exists(hash_t *hash, void *data, uint32_t cur_hash) {
	dlist_t *ret = hash_find(hash, data, cur_hash);

	return (ret != NULL);
}
