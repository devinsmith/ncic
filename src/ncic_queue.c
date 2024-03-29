/*
** Based on pork_queue.c
** ncic_queue.c - Generic queues
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <stdlib.h>
#include <string.h>

#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_queue.h"

pork_queue_t *queue_new(u_int32_t max_entries) {
	pork_queue_t *q = xcalloc(1, sizeof(*q));

	q->max = max_entries;
	return (q);
}

int queue_putback_head(pork_queue_t *q, void *data) {
	dlist_t *new_node;

	if (q->max > 0 && q->entries >= q->max)
		return (-1);

	new_node = xcalloc(1, sizeof(*new_node));
	new_node->data = data;

	if (q->entries == 0) {
		new_node->prev = NULL;
		new_node->next = NULL;

		q->head = new_node;
		q->tail = new_node;
	} else {
		new_node->prev = NULL;
		new_node->next = q->head;
		q->head->prev = new_node;
		q->head = new_node;
	}

	q->entries++;
	return (0);
}

int queue_add(pork_queue_t *q, void *data) {
	dlist_t *new_node;

	if (q->max > 0 && q->entries >= q->max)
		return (-1);

	new_node = xcalloc(1, sizeof(*new_node));
	new_node->data = data;
	new_node->next = NULL;

	if (q->entries == 0) {
		new_node->prev = NULL;

		q->head = new_node;
		q->tail = new_node;
	} else {
		new_node->prev = q->tail;

		q->tail->next = new_node;
		q->tail = new_node;
	}

	q->entries++;
	return (0);
}

void *queue_get(pork_queue_t *q) {
	void *ret;

	if (q->entries == 0)
		return (NULL);

	q->entries--;
	ret = q->head->data;

	if (q->head == q->tail) {
		free(q->head);
		q->head = NULL;
		q->tail = NULL;
	} else {
		dlist_t *old_head = q->head;

		old_head->next->prev = NULL;
		q->head = old_head->next;
		free(old_head);
	}

	return (ret);
}

void queue_destroy(pork_queue_t *q, void (*cleanup)(void *)) {
	dlist_t *cur = q->head;

	while (cur != NULL) {
		dlist_t *next = cur->next;

		if (cleanup != NULL)
			cleanup(cur->data);

		free(cur);
		cur = next;
	}
}
