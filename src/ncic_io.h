/*
** Based on pork_io.h
** ncic_io.h - I/O handler.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_IO_H__
#define __NCIC_IO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define IO_COND_READ		0x01
#define IO_COND_WRITE		0x02
#define IO_COND_EXCEPTION	0x04
#define IO_COND_DEAD		0x08
#define IO_COND_ALWAYS		0x10
#define IO_COND_RW			(IO_COND_READ | IO_COND_WRITE)

struct io_source {
	int fd;
	u_int32_t cond;
	void *data;
	void *key;
	void (*callback)(int fd, u_int32_t condition, void *data);
};

int pork_io_init(void);
void pork_io_destroy(void);
int pork_io_del(void *key);
int pork_io_run(void);
int pork_io_dead(void *key);
int pork_io_add_cond(void *key, u_int32_t new_cond);
int pork_io_del_cond(void *key, u_int32_t new_cond);
int pork_io_set_cond(void *key, u_int32_t new_cond);

int pork_io_add(int fd,
				u_int32_t cond,
				void *data,
				void *key,
				void (*callback)(int fd, u_int32_t condition, void *data));

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_IO_H__ */

