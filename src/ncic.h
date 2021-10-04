/*
** Based on pork.h
** ncic.h - ncic main include file.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_H__
#define __NCIC_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define __format(x) __attribute__((format x ))
#else
#define __format(x)
#endif

#ifdef HAVE___ATTRIBUTE__
#define __notused	__attribute__((unused))
#else
#define __notused
#endif

#ifdef ENABLE_DEBUGGING
#	define debug(format, args...) do { fprintf(stderr, "[%s:%u:%s] DEBUG: " format "\n", __FILE__, __LINE__, __FUNCTION__, ##args); } while (0)
#else
#	define debug(format, args...) do { } while (0)
#endif

#include <sys/types.h>
#include <stdint.h>

void pork_exit(int status, const char *msg, const char *fmt, ...) __format((printf, 3, 4));
void keyboard_input(int fd, uint32_t condition, void *data);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_H__ */

