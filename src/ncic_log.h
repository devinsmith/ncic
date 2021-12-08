/*
 * Copyright (c) 2008 Devin Smith <devin@devinsmith.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef NCIC_LOG_H
#define NCIC_LOG_H

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

void log_init();
void log_exit();

void log_set_fp(FILE *fp);
int log_set_logfile(const char *file);
void log_msg(int lvl, const char *fmt, ...);
void log_msgraw(int lvl, const char *fmt, ...);
void log_tmsg(int lvl, const char *fmt, ...);
void log_flush();
void log_set_level(int lvl);

#endif /* NCIC_LOG_H */

