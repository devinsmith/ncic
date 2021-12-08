/*
** Based on pork_acct.c
** ncic_acct.c - account management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_proto.h"
#include "ncic_acct.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_msg.h"
#include "ncic_chat.h"

struct pork_acct *pork_acct_find(u_int32_t refnum) {
  return screen.acct;
}

struct pork_acct *pork_acct_get_data(u_int32_t refnum) {
  return screen.acct;
}

int pork_acct_del_refnum(u_int32_t refnum, char *reason) {
	struct pork_acct *acct;

	acct = pork_acct_find(refnum);
	if (acct == nullptr)
		return (-1);

	pork_acct_del(acct, reason);
	return (0);
}

void pork_acct_del(struct pork_acct *acct, const char *reason) {
	dlist_t *cur;

  if (acct != nullptr) {
    pork_signoff(acct, reason);
    chat_leave_all(acct);
  }

	cur = screen.window_list;
	if (cur != nullptr) {
		do {
			struct imwindow *win = (imwindow *)cur->data;

			if (win->owner == acct) {

				win->owner->ref_count--;

				if (win->owner == screen.null_acct)
					win->owner = nullptr;
				else {
					win->owner = screen.null_acct;
					win->owner->ref_count++;
				}
			}

			cur = cur->next;
		} while (cur != screen.window_list);
	}

	/* This must always be the case. */
	if (acct != nullptr) {
		delete acct;
	}
}

int pork_acct_next_refnum(u_int32_t cur_refnum, u_int32_t *next) {
	struct pork_acct *acct = nullptr;

	acct = pork_acct_find(cur_refnum);
	if (acct == nullptr) {
		debug("current refnum %u doesn't exist", cur_refnum);
		return (-1);
	}

	*next = acct->refnum;
	return (0);
}

void pork_acct_del_all(const char *reason) {
		pork_acct_del(screen.acct, reason);
}

int pork_acct_connect(const char *user, char *args, int protocol) {
  struct pork_acct *acct;

  if (user == nullptr)
    return (-1);

  if (screen.acct != nullptr) {
    screen_err_msg("%s is already connected", user);
    return (-1);
  }

  acct = pork_acct_init(user, protocol);
  if (acct == nullptr) {
    screen_err_msg("Failed to initialize account for %s", user);
    return (-1);
  }

  screen.acct = acct;
  screen_bind_all_unbound(acct);

  if (acct->proto->connect(acct, args) == -1) {
    screen_err_msg("Unable to login as %s", acct->username);
    pork_acct_del_refnum(acct->refnum, nullptr);
    return (-1);
  }

  return (0);
}

void pork_acct_update(void) {
  struct pork_acct *acct = screen.acct;
  time_t time_now = time(nullptr);

  if (acct == nullptr)
    return;

  if (acct->proto->update != nullptr) {
    if (acct->proto->update(acct) == -1)
      return;
  }

  if (acct->proto->set_idle_time != nullptr && acct->report_idle &&
    !acct->marked_idle && opt_get_bool(OPT_REPORT_IDLE))
  {
    time_t time_diff = time_now - acct->last_input;
    int idle_after = opt_get_int(OPT_IDLE_AFTER);

    if (idle_after > 0 && time_diff >= 60 * idle_after) {
      screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_IDLE,
        "Setting %s idle after %d minutes of inactivity",
        acct->username, (int) time_diff / 60);
      acct->proto->set_idle_time(acct, time_diff);
    }
  }
}

static inline u_int32_t pork_acct_get_new_refnum() {
	u_int32_t i;

	for (i = 0 ; i < 0xffffffff ; i++) {
		if (pork_acct_find(i) == nullptr)
			return (i);
	}

	return (0);
}

/*
** Initialize an account for the specified user.
*/

struct pork_acct *pork_acct_init(const char *user, int protocol) {
  auto *acct = new pork_acct;
	acct->username = xstrdup(user);
	acct->state = STATE_DISCONNECTED;
	acct->proto = proto_get(protocol);

	if (protocol < 0)
		return (acct);

	if (acct->proto->init != nullptr && acct->proto->init(acct) == -1) {
    delete acct;
    return nullptr;
  }

	acct->can_connect = true;
	acct->refnum = pork_acct_get_new_refnum();

	time(&acct->last_input);
	return (acct);
}

static int pork_acct_reconnect(struct pork_acct *acct) {
	if (acct->proto->reconnect == nullptr || acct->connected ||
		!acct->disconnected || acct->reconnecting)
	{
		debug("%p %u %u %u", acct->proto->reconnect, acct->connected,
			!acct->disconnected, acct->reconnecting);
		return (-1);
	}

	acct->reconnect_tries++;
	acct->reconnecting = true;

	screen_err_msg("Automatically reconnecting account %s (attempt %u)",
		acct->username, acct->reconnect_tries);

	return (acct->proto->reconnect(acct, nullptr));
}

static int pork_acct_connect_fail(struct pork_acct *acct) {
	u_int32_t max_reconnect_tries;
	u_int32_t connect_interval;
	u_int32_t connect_interval_max;

	max_reconnect_tries = opt_get_int(OPT_RECONNECT_TRIES);
	connect_interval = opt_get_int(OPT_RECONNECT_INTERVAL);
	connect_interval_max = opt_get_int(OPT_RECONNECT_MAX_INTERVAL);

	acct->connected = false;
	acct->reconnecting = false;
	acct->disconnected = true;

	if (acct->reconnect_tries >= max_reconnect_tries) {
		screen_err_msg("Failed to reconnect %s after %u tries. Giving up.",
			acct->username, max_reconnect_tries);

		pork_acct_del_refnum(acct->refnum, nullptr);
		return (-1);
	}

	acct->reconnect_next_try = time(nullptr) +
		min(acct->reconnect_tries * connect_interval, connect_interval_max);
	return (0);
}

int pork_acct_disconnected(struct pork_acct *acct) {
	if (!acct->successful_connect || !opt_get_bool(OPT_AUTO_RECONNECT)) {
		pork_acct_del_refnum(acct->refnum, nullptr);
		return (0);
	}

	if (!acct->connected)
		return (pork_acct_connect_fail(acct));

	screen_win_msg(cur_window(), 1, 1, 0, MSG_TYPE_SIGNOFF,
		"%s has been disconnected", acct->username);

	acct->connected = false;
	acct->reconnecting = false;
	acct->reconnect_tries = 0;
	acct->disconnected = true;
	acct->reconnect_next_try = time(nullptr);

	if (acct->proto->disconnected != nullptr)
		acct->proto->disconnected(acct);

	return (pork_acct_reconnect(acct));
}

void pork_acct_reconnect_all(void) {
	time_t now = time(nullptr);
	int timeout = opt_get_int(OPT_CONNECT_TIMEOUT);

	if (screen.acct != nullptr) {
		struct pork_acct *acct = screen.acct;

		if (acct->disconnected && !acct->reconnecting &&
			acct->reconnect_next_try < now)
		{
			pork_acct_reconnect(acct);
		} else if (	acct->reconnecting &&
					acct->reconnect_next_try + timeout < now)
		{
			screen_err_msg(
				"Attempt %u to reconnect %s timed out after %u seconds",
				acct->reconnect_tries, acct->username, timeout);

			acct->proto->connect_abort(acct);
			pork_acct_connect_fail(acct);
		}
	}
}

pork_acct::pork_acct() : username{nullptr}, passwd{nullptr},
  userhost{nullptr}, away_msg{nullptr}, last_input{0}, id{0}, state{0},
  idle_time{0}, report_idle{false}, marked_idle{false}, can_connect{false},
  connected{false}, successful_connect{false}, disconnected{false},
  reconnecting{false}
{

}

pork_acct::~pork_acct()
{
  /* XXX - really? */
  if (proto->free != nullptr)
    proto->free(this);

  hash_destroy(&autoreply);

  free_str_wipe(passwd);
  free(away_msg);
  free(username);
  free(server);
  free(fport);
}

void pork_acct::set_connected()
{
  successful_connect = true;
  connected = true;

  if (reconnecting)
    chat_rejoin_all(this);

  disconnected = false;
  reconnecting = false;
  reconnect_tries = 0;
}
