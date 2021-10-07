/*
** Based on pork_timer.c
** ncic_timer.c - Timer implementation
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_command.h"
#include "ncic_timer.h"

static u_int32_t last_refnum;

static int timer_find_cb(void *l, void *r) {
	char *str = (char *)l;
	struct timer_entry *timer = (struct timer_entry *)r;

	return (strcmp(str, timer->command));
}

static int timer_find_refnum_cb(void *l, void *r) {
	u_int32_t refnum = POINTER_TO_UINT(l);
	struct timer_entry *timer = (struct timer_entry *)r;

	return (refnum - timer->refnum);
}

static void timer_destroy_cb(void *param __notused, void *data) {
	struct timer_entry *timer = (struct timer_entry *)data;

	free(timer->command);
	free(timer);
}

u_int32_t timer_add(dlist_t **timer_list,
					char *command,
					time_t interval,
					u_int32_t times)
{
	struct timer_entry *timer = (struct timer_entry *)xcalloc(1, sizeof(*timer));

	timer->last_run = time(nullptr);
	timer->command = xstrdup(command);
	timer->refnum = last_refnum++;
	timer->interval = interval;
	timer->times = times;

	*timer_list = dlist_add_head(*timer_list, timer);

	return (timer->refnum);
}

int timer_del(dlist_t **timer_list, char *command) {
	dlist_t *node;
	struct timer_entry *timer;

	node = dlist_find(*timer_list, command, timer_find_cb);
	if (node == nullptr)
		return (-1);

	timer = (struct timer_entry *)node->data;

	*timer_list = dlist_remove(*timer_list, node);

	free(timer->command);
	free(timer);
	return (0);
}

int timer_del_refnum(dlist_t **timer_list, u_int32_t refnum) {
	dlist_t *node;
	struct timer_entry *timer;

	node = dlist_find(*timer_list, UINT_TO_POINTER(refnum),
				timer_find_refnum_cb);
	if (node == nullptr)
		return (-1);

	timer = (struct timer_entry *)node->data;

	*timer_list = dlist_remove(*timer_list, node);

	free(timer->command);
	free(timer);

	return (0);
}

int timer_run(dlist_t **timer_list) {
	dlist_t *cur = *timer_list;
	int triggered = 0;

	while (cur != nullptr) {
		dlist_t *next = cur->next;
		struct timer_entry *timer = (struct timer_entry *)cur->data;

		if (timer->last_run + timer->interval <= time(nullptr)) {
			char *command = xstrdup(timer->command);

			triggered++;

			run_mcommand(command);
			free(command);

			if (timer->times != 1) {
				time(&timer->last_run);
				if (timer->times > 1)
					timer->times--;
			} else {
				*timer_list = dlist_remove(*timer_list, cur);
				free(timer->command);
				free(timer);
			}
		}

		cur = next;
	}

	return (triggered);
}

void timer_destroy(dlist_t **timer_list) {
	dlist_destroy(*timer_list, nullptr, timer_destroy_cb);
	*timer_list = nullptr;
}
