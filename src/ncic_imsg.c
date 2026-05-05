/*
** Based on pork_imsg.c
** ncic_imsg.c
** Copyright (C) 2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <string.h>

#include "ncic_util.h"
#include "ncic_set.h"
#include "ncic_swindow.h"
#include "ncic_imsg.h"
#include "ncic_cstr.h"

static uint32_t imsg_wordwrapped_lines(struct swindow *swindow,
										struct imsg *imsg)
{
	uint32_t len = imsg->len;
	chtype *ch = imsg->text;
	chtype *end = &ch[len - 1];
	uint32_t lines = 0;
	int add = 0;
	chtype cont_char = opt_get_char(OPT_WORDWRAP_CHAR);

	if (len <= swindow->cols)
		return (1);

	if (swindow->cols == 1)
		return (len);

	while (*ch != 0) {
		chtype *p;

		p = &ch[swindow->cols - 1 - add];
		if (p >= end) {
			++lines;
			break;
		}

		/*
		** If the last character on current line or the first character
		** on the next line is a space, there's no need to break the line
		*/
		if (chtype_get(p[0]) != ' ' && chtype_get(p[1]) != ' ') {
			chtype *temp = p;

			/* Find the last space, if any */
			do {
				temp--;
				if (temp <= ch) {
					/*
					** No spaces on the line, just split it at the
					** last column
					*/
					goto out;
				}
			} while (chtype_get(*temp) != ' ');
			/* Found a space, split the line after the space. */
			p = temp;
		}
out:
		if (cont_char != 0)
			add = 1;

		ch = &p[1];
		lines++;
	}

	return (lines);
}

/*
** Compute the number of rows the message will occupy
** on the screen.
*/

uint32_t imsg_lines(struct swindow *swindow, struct imsg *imsg) {
	if (imsg->len <= swindow->cols)
		return (1);

	if (!swindow->wordwrap)
		return ((imsg->len / swindow->cols) + (imsg->len % swindow->cols != 0));

	return (imsg_wordwrapped_lines(swindow, imsg));
}

struct imsg *imsg_new(struct swindow *swindow, chtype *msg, size_t len) {
	struct imsg *imsg;

	imsg = xmalloc(sizeof(*imsg));
	imsg->text = msg;
	imsg->serial = swindow->serial++;
	imsg->len = len;
	imsg->lines = imsg_lines(swindow, imsg);

	return (imsg);
}

/*
** Return a pointer to the first character of the nth
** line of the message, given the current screen width.
*/

chtype *imsg_partial(struct swindow *swindow, struct imsg *imsg, uint32_t n) {
	chtype *text = imsg->text;
	uint32_t offset;
	static chtype nul_ch = 0;

	if (n <= 1)
		return (text);

	offset = --n * swindow->cols;

	/*
	** offset can end up larger than the length
	** of the string when the screen has been cleared.
	*/

	if (offset < imsg->len) {
		text += offset;
		return (text);
	}

	/*
	** If the offset is greater than the string's length
	** the caller doesn't want to write anything to the screen
	*/

	return (&nul_ch);
}
