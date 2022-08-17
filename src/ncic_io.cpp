/*
** Based on pork_io.c
** ncic_io.c - I/O handler.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <cstdlib>
#include <cerrno>
#include <sys/time.h>
#include <sys/types.h>
#include <algorithm>

#include "ncic.h"
#include "ncic_io.h"
#include "ncic_inet.h"
#include "ncic_log.h"

void IoManager::destroy()
{
  for (auto io_source : io_list) {
    delete io_source;
  }
}

int IoManager::run() {
  fd_set rfds;
  fd_set wfds;
  fd_set xfds;
  int max_fd = -1;
  int ret;
  struct timeval tv = { 0, 600000 };

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);
  FD_ZERO(&xfds);

  for (auto it = io_list.begin(); it != io_list.end(); ++it) {
    io_source *io = *it;

    if (io->fd >= 0) {
      if (io->cond & IO_COND_ALWAYS && io->callback != nullptr)
        io->callback(io->fd, IO_COND_ALWAYS, io->data);

      if (io->cond & IO_COND_READ)
        FD_SET(io->fd, &rfds);

      if (io->cond & IO_COND_WRITE)
        FD_SET(io->fd, &wfds);

      if (io->cond & IO_COND_EXCEPTION)
        FD_SET(io->fd, &xfds);

      if (io->fd > max_fd)
        max_fd = io->fd;
    } else {
      if (io->callback != nullptr)
        io->callback(io->fd, IO_COND_DEAD, io->data);

      delete io;
      io_list.erase(it);
    }
  }

  if (max_fd < 0)
    return (-1);

  /*
  ** If there's a bad fd in the set better find it, otherwise
  ** we're going to get into an infinite loop.
  */
  ret = select(max_fd + 1, &rfds, &wfds, &xfds, &tv);
  if (ret < 1) {
    if (ret == -1 && errno == EBADF) {
      process_dead_fds();
    }

    return (ret);
  }

  for (auto io : io_list) {
    if (io->fd >= 0) {
      u_int32_t cond = 0;

      if ((io->cond & IO_COND_READ) && FD_ISSET(io->fd, &rfds))
        cond |= IO_COND_READ;

      if ((io->cond & IO_COND_WRITE) && FD_ISSET(io->fd, &wfds))
        cond |= IO_COND_WRITE;

      if ((io->cond & IO_COND_EXCEPTION) && FD_ISSET(io->fd, &xfds))
        cond |= IO_COND_EXCEPTION;

      if (cond != 0 && io->callback != nullptr)
        io->callback(io->fd, cond, io->data);
    }
  }

  return (ret);
}

int IoManager::process_dead_fds()
{
  int bad_fd = 0;

  for (auto it = io_list.begin(); it != io_list.end(); ++it) {
    io_source *io = *it;

    if (io->fd < 0 || sock_is_error(io->fd)) {
      debug("fd %d is dead", io->fd);
      if (io->callback != nullptr)
        io->callback(io->fd, IO_COND_DEAD, io->data);

      delete io;
      io_list.erase(it);

      bad_fd++;
    }
  }

  return bad_fd;
}

int IoManager::delete_key(void *key)
{
  struct io_source *io;

  log_tmsg(0, "Removing io for key: %p", key);

  auto result = std::find_if(io_list.begin(), io_list.end(),
                             [key](const io_source* x) { return x->key == key;});

  if (result == io_list.end())
    return (-1);

  io = *result;
  io->fd = -2;
  io->callback = nullptr;

  return 0;
}

int IoManager::add_cond(void *key, u_int32_t new_cond)
{
  auto result = std::find_if(io_list.begin(), io_list.end(),
                             [key](const io_source* x) { return x->key == key;});

  if (result == io_list.end())
    return (-1);

  (*result)->cond |= new_cond;

  return 0;
}

int IoManager::del_cond(void *key, u_int32_t new_cond)
{
  auto result = std::find_if(io_list.begin(), io_list.end(),
                             [key](const io_source* x) { return x->key == key;});

  if (result == io_list.end())
    return (-1);

  (*result)->cond &= ~new_cond;

  return 0;
}

int IoManager::add(int fd, u_int32_t cond, void *data, void *key, void (*callback)(int, u_int32_t, void *))
{
  struct io_source *io;

  log_tmsg(0, "Adding new condition on fd: %d, key: %p, cond: %d", fd, key, cond);

  /*
  ** If there's already an entry for this key, delete it
  ** and replace it with the new one.
  */

  auto result = std::find_if(io_list.begin(), io_list.end(),
                             [key](const io_source* x) { return x->key == key;});

  if (result != io_list.end()) {
    free(*result);
    io_list.erase(result);
  }

  io = new io_source;
  io->fd = fd;
  io->cond = cond;
  io->data = data;
  io->key = key;
  io->callback = callback;

  io_list.push_back(io);

  return 0;
}
