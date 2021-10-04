/*
** Heavily based on pork_util.h
** ncic_util.h - utility functions
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_UTIL_H__
#define __NCIC_UTIL_H__

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define array_elem(x) (sizeof((x)) / sizeof((x)[0]))

/* Needs special care */
#define POINTER_TO_UINT(p)	((uintptr_t) (p))
#define UINT_TO_POINTER(u)	((void *) (uintptr_t) (u))

#ifndef min
#	define min(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef max
#	define max(x,y) ((x) > (y) ? (x) : (y))
#endif

char *xstrdup(const char *str);
void *xmalloc(size_t len);
void *xcalloc(size_t nmemb, size_t len);
char *xstrndup(const char *str, size_t len);
void *xrealloc(void *ptr, size_t size);

void strtoupper(char *s);
void free_str_wipe(char *str);

int xstrncpy(char *dest, const char *src, size_t n);
int xstrncat(char *dest, const char *src, size_t n);

int expand_path(char *path, char *dest, size_t len);

int blank_str(const char *str);

uint32_t string_hash(const char *str, uint32_t order);
uint32_t int_hash(int num, uint32_t order);

int str_to_uint(const char *str, uint32_t *val);
int str_to_int(const char *str, int *val);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_UTIL_H__ */
