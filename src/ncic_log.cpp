/*
 * Copyright (c) 2008-2021 Devin Smith <devin@devinsmith.net>
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

#include <ctime>

#include "ncic_log.h"

static int s_log_level = -1;
static FILE	*s_log_fp = nullptr;
static bool s_log_cleanup_added = false;

void log_init()
{
  if (!s_log_cleanup_added) {
    if (atexit(log_exit))
      return;
    s_log_cleanup_added = true;
  }
  s_log_level = 0;
}

void log_set_level(int lvl)
{
  s_log_level = lvl;
}

void log_exit()
{
  if (s_log_fp != nullptr)
  {
    fflush(s_log_fp);
    fclose(s_log_fp);
  }
}

int log_set_logfile(const char *file)
{
  s_log_fp = fopen(file, "a");
	if (s_log_fp == nullptr)
		return -1;
	return 0;
}

void log_set_fp(FILE *fp)
{
  s_log_fp = fp;
}

void log_msg(int lvl, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  if (s_log_level >= lvl) {
    if (s_log_fp != nullptr) {
      // fprintf it
      vfprintf(s_log_fp, fmt, ap);
      fprintf(s_log_fp, "\n");
      fflush(s_log_fp);
    }
    else {
      /* Just output to stdout */
      (void) vfprintf(stdout, fmt, ap);
      fprintf(stdout, "\n");
    }
  }
  va_end(ap);
}

void log_msgraw(int lvl, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  if (s_log_level >= lvl) {
    if (s_log_fp != nullptr) {
      // fprintf it
      vfprintf(s_log_fp, fmt, ap);
      fflush(s_log_fp);
    }
    else {
      /* Just output to stdout */
      (void) vfprintf(stdout, fmt, ap);
    }
  }
  va_end(ap);
}

void
log_tmsg(int lvl, const char *fmt, ...)
{
	va_list ap;
	time_t now;
	struct tm *tm_now;

	va_start(ap, fmt);
	if (s_log_level >= lvl) {

		now = time(nullptr);
		tm_now = localtime(&now);

		if (s_log_fp != nullptr) {
			fprintf(s_log_fp, "%04d.%02d.%02d-%02d:%02d:%02d ",
			    tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
			    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);

			// fprintf it
			vfprintf(s_log_fp, fmt, ap);
      fprintf(s_log_fp, "\n");
			fflush(s_log_fp);
		}
		else {
			/* Just output to stdout */
			fprintf(stdout, "%04d.%02d.%02d-%02d:%02d:%02d ",
			    tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
			    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
			(void) vfprintf(stdout, fmt, ap);
      fprintf(stdout, "\n");
		}
	}
	va_end(ap);
}

void log_flush()
{
  if (s_log_fp != nullptr) {
    fflush(s_log_fp);
  } else {
    fflush(stdout);
  }
}

