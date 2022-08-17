/*
** Based on pork_io.h
** ncic_io.h - I/O handler.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef NCIC_IO_H
#define NCIC_IO_H

#include <list>

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

// Singleton, for now.
struct IoManager {
  IoManager(const IoManager&) = delete;
  IoManager& operator=(const IoManager &) = delete;
  IoManager(IoManager &&) = delete;
  IoManager & operator=(IoManager &&) = delete;

  static IoManager& instance()
  {
    static IoManager manager;
    return manager;
  }

  void destroy();
  int delete_key(void *key);
  int add_cond(void *key, u_int32_t new_cond);
  int del_cond(void *key, u_int32_t new_cond);

  int add(int fd,
      u_int32_t cond,
      void *data,
      void *key,
      void (*callback)(int fd, u_int32_t condition, void *data));

  int run();

private:
  IoManager() = default;

  int process_dead_fds();

  std::list<io_source *> io_list;
};

#endif // NCIC_IO_H

