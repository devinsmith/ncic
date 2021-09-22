/*
** Heavily based on pork_screen_io.h
** pork_screen_io.h - screen management.
** Copyright (C) 2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_SCREEN_IO_H__
#define __NCIC_SCREEN_IO_H__

#include "ncic.h"

struct imwindow;
struct pork_acct;

void screen_print_str(struct imwindow *, char *buf, size_t len, int type);
void screen_win_msg(struct imwindow *win,
					int ts,
					int banner,
					int color,
					int type,
					char *fmt, ...) __format((printf, 6, 7));

void screen_nocolor_msg(char *fmt, ...) __format((printf, 1, 2));
void screen_err_msg(char *fmt, ...) __format((printf, 1, 2));
void screen_cmd_output(char *fmt, ...) __format((printf, 1, 2));

void screen_win_target_msg(	struct pork_acct *acct,
							char *target,
							int timestamp,
							char *fmt, ...) __format((printf, 4, 5));

int screen_get_query_window(struct pork_acct *acct,
							char *name,
							struct imwindow **winr);

int screen_make_query_window(struct pork_acct *acct,
							char *name,
							struct imwindow **winr);

int screen_draw_input(void);
int screen_set_quiet(int status);
int screen_prompt_user(char *prompt, char *buf, size_t len);
void screen_doupdate(void);

/* yeah, yeah */

#ifdef ENABLE_DEBUGGING

#define pork_sock_err(a, x) \
	do { \
		int __psockerr; \
		if ((__psockerr = sock_is_error((x)))) { \
			screen_err_msg("(%s:%d) network error: %s: %s", __FILE__, __LINE__, ((struct pork_acct *) (a))->username, strerror(__psockerr)); \
		} \
	} while (0)

#else

#define pork_sock_err(a, x) \
	do { \
		int __psockerr; \
		if ((__psockerr = sock_is_error((x)))) { \
			screen_err_msg("network error: %s: %s", ((struct pork_acct *) (a))->username, strerror(__psockerr)); \
		} \
	} while (0)

#endif

#endif /* __NCIC_SCREEN_IO_H__ */
