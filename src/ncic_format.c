/*
** Based on pork_format.c
** ncic_format.c - helper functions for formatting text.
** Copyright (C) 2002-2004 Amber Adams <amber@ojnk.net>
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_buddy.h"
#include "ncic_imwindow.h"
#include "ncic_acct.h"
#include "ncic_proto.h"
#include "ncic_cstr.h"
#include "ncic_misc.h"
#include "ncic_screen.h"
#include "ncic_chat.h"
#include "ncic_format.h"

static int format_status_timestamp(	char opt,
									char *buf,
									size_t len,
									va_list ap __notused)
{
	time_t cur_time;
	struct tm *tm;
	int ret = 0;

	cur_time = time(NULL);
	tm = localtime(&cur_time);

	if (tm == NULL) {
		debug("localtime: %s", strerror(errno));
		return (-1);
	}

	switch (opt) {
		/* Hours, 24-hour format */
		case 'H':
			ret = snprintf(buf, len, "%02d", tm->tm_hour);
			break;

		/* Hours, 12-hour format */
		case 'h': {
			u_int32_t time_12hour;

			if (tm->tm_hour == 0)
				time_12hour = 12;
			else {
				time_12hour = tm->tm_hour;

				if (time_12hour > 12)
					time_12hour -= 12;
			}

			ret = snprintf(buf, len, "%02d", time_12hour);
			break;
		}

		/* Minutes */
		case 'M':
		case 'm':
			ret = snprintf(buf, len, "%02d", tm->tm_min);
			break;

		/* Seconds */
		case 'S':
		case 's':
			ret = snprintf(buf, len, "%02d", tm->tm_sec);
			break;

		/* AM or PM */
		case 'Z':
		case 'z':
			ret = xstrncpy(buf, (tm->tm_hour >= 12 ? "pm" : "am"), len);
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_status_activity(	char opt,
									char *buf,
									size_t len,
									va_list ap __notused)
{
	switch (opt) {
		/* Activity display */
		case 'A':
		case 'a': {
			dlist_t *cur = screen.window_list;
			size_t n = 0;

			do {
				struct imwindow *imwindow = cur->data;

				if (imwindow->swindow.activity &&
					!imwindow->ignore_activity &&
					!imwindow->swindow.visible)
				{
					int ret = snprintf(&buf[n], len - n, "%u,",
								imwindow->refnum);

					if (ret < 0 || (size_t) ret >= len - n)
						return (-1);

					n += ret;
				}

				cur = cur->next;
			} while (cur != screen.window_list);

			if (n > 0)
				buf[n - 1] = '\0';
			else
				return (1);

			break;
		}

		default:
			return (-1);
	}

	return (0);
}

static int format_status_typing(char opt, char *buf, size_t len, va_list ap) {
	int ret = 0;
	struct imwindow *imwindow = va_arg(ap, struct imwindow *);

	switch (opt) {
		/* Typing status */
		case 'Y':
			if (imwindow->typing) {
				char *str;

				if (imwindow->typing == 1)
					str = opt_get_str(OPT_TEXT_TYPING_PAUSED);
				else
					str = opt_get_str(OPT_TEXT_TYPING);

				if (xstrncpy(buf, str, len) == -1)
					ret = -1;
			} else
				return (1);

			break;

		default:
			return (-1);
	}

	return (ret);
}

static int format_status_held(char opt, char *buf, size_t len, va_list ap) {
	struct imwindow *imwindow = va_arg(ap, struct imwindow *);
	int ret = 0;

	if (imwindow->swindow.held == 0)
		return (1);

	switch (opt) {
		/* Held indicator */
		case 'H':
			ret = snprintf(buf, len, "%02d", imwindow->swindow.held);
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_status_idle(char opt, char *buf, size_t len, va_list ap) {
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	int hide_if_zero = va_arg(ap, int);
	int ret = 0;

	switch (opt) {
		/* Idle time */
		case 'I':
		own:
			if (acct->idle_time == 0 && hide_if_zero)
				return (1);

			ret = time_to_str(acct->idle_time, buf, len);
			break;

		/*
		** If the current window is a chat window, $i will display the
		** idle time of the user we're talking to.
		*/
		case 'i': {
			struct imwindow *win = cur_window();

			if (win->type == WIN_TYPE_PRIVMSG) {
				struct buddy *buddy;

				buddy = NULL;
				if (buddy == NULL || (buddy->idle_time == 0 && hide_if_zero))
					return (1);

				ret = time_to_str(buddy->idle_time, buf, len);
				break;
			}

			goto own;
		}

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_status(char opt, char *buf, size_t len, va_list ap) {
	struct imwindow *imwindow = va_arg(ap, struct imwindow *);
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	int ret = 0;

	switch (opt) {
		/* Disconnect / reconnect status */
		case 'D':
			if (acct->reconnecting)
				ret = xstrncpy(buf, "reconnecting", len);
			else if (acct->disconnected)
				ret = xstrncpy(buf, "disconnected", len);
			else
				ret = xstrncpy(buf, "connected", len);
			break;

		/* Screen name */
		case 'n':
		case 'N':
			ret = xstrncpy(buf, acct->username, len);
			break;

		/* Remote server */
		case 'S':
		case 's':
			if (acct->server != NULL)
				ret = xstrncpy(buf, acct->server, len);
			break;

		/* Remote port */
		case 'P':
		case 'p':
			if (acct->fport != NULL)
				ret = xstrncpy(buf, acct->fport, len);
			break;

		/* Window name */
		case 'z':
		case 'Z':
			ret = xstrncpy(buf, imwindow->name, len);
			break;

		/* Chat room, if inactive */
		case 'c':
		case 'C':
			if (imwindow->type == WIN_TYPE_CHAT &&
				(imwindow->data == NULL || !acct->connected))
			{
				ret = xstrncpy(buf, opt_get_str(OPT_TEXT_NO_ROOM), len);
			}
			break;

		/* Chat mode, if applicable; M includes arguments, m doesn't. */
		case 'M':
		case 'm':
			if (imwindow->type == WIN_TYPE_CHAT && imwindow->data != NULL) {
				struct chatroom *chat = imwindow->data;

				ret = xstrncpy(buf, chat->mode, len);
				if (opt == 'm') {
					char *p;

					p = strchr(buf, ' ');
					if (p != NULL)
						*p = '\0';
				}
			}
			break;

		/* Chat status, if applicable */
		case '@':
			if (imwindow->type == WIN_TYPE_CHAT && imwindow->data != NULL) {
				struct chatroom *chat = imwindow->data;
				struct chat_user *user;

				user = chat_find_user(acct, chat, acct->username);
				if (user == NULL)
					break;

				if (user->status & CHAT_STATUS_OP)
					ret = xstrncpy(buf, "@", len);
				else if (user->status & CHAT_STATUS_HALFOP)
					ret = xstrncpy(buf, "%%", len);
				else if (user->status & CHAT_STATUS_VOICE)
					ret = xstrncpy(buf, "+", len);
			}
			break;

		/* User status */
		case '!':
			if (acct->disconnected) {
				if (acct->reconnecting)
					ret = xstrncpy(buf, "reconnecting", len);
				else
					ret = xstrncpy(buf, "disconnected", len);
			} else if (acct->connected) {
				if (acct->away_msg != NULL)
					ret = xstrncpy(buf, "away", len);
				else
					ret = xstrncpy(buf, "online", len);
			} else
				ret = xstrncpy(buf, "not connected", len);
			break;

		/* Protocol */
		case '?':
			ret = xstrncpy(buf, acct->proto->name, len);
			break;

		/* User mode */
		case 'u':
		case 'U':
			ret = xstrncpy(buf, acct->umode, len);
			break;

		/* Timestamp */
		case 't':
		case 'T':
			ret = fill_format_str(OPT_FORMAT_STATUS_TIMESTAMP, buf, len);
			break;

		/* Activity */
		case 'a':
		case 'A':
			ret = fill_format_str(OPT_FORMAT_STATUS_ACTIVITY, buf, len);
			break;

		/* Typing */
		case 'y':
		case 'Y':
			ret = fill_format_str(OPT_FORMAT_STATUS_TYPING, buf, len, imwindow);
			break;

		/* Held Messages */
		case 'h':
		case 'H':
			ret = fill_format_str(OPT_FORMAT_STATUS_HELD, buf, len, imwindow);
			break;

		/* Idle Time */
		case 'i':
		case 'I':
			ret = fill_format_str(OPT_FORMAT_STATUS_IDLE, buf, len, acct,
					isupper(opt));
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int
format_system_alert(char opt, char *buf, size_t len, va_list ap)
{
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	char *msg = va_arg(ap, char *);
	int ret = 0;

	switch (opt) {
	case 'M':
	case 'm':
		if (msg != NULL)
			ret = xstrncpy(buf, msg, len);
		break;
	default:
		return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_msg_highlight(char opt, char *buf, size_t len, va_list ap) {
  struct pork_acct *acct = va_arg(ap, struct pork_acct *);
  char *msg = va_arg(ap, char *);
  int ret = 0;

  switch (opt) {
    /* User name */
    case 'n':
    case 'N':
      if (acct->username != NULL)
        ret = xstrncpy(buf, acct->username, len);
      break;

    /* Message text */
    case 'm':
    case 'M':
      if (msg != NULL)
        ret = xstrncpy(buf, msg, len);
      break;

    // id number
    case 'i':
    case 'I':
      ret = snprintf(buf, len, "%u", acct->id);
      break;

    default:
      return (-1);
  }

  if (ret < 0 || (size_t) ret >= len)
    return (-1);

  return (0);
}


static int format_msg_send(char opt, char *buf, size_t len, va_list ap) {
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	char *dest = va_arg(ap, char *);
 	char *msg = va_arg(ap, char *);
	int ret = 0;

	switch (opt) {
		/* Timestamp */
		case 'T':
			ret = fill_format_str(OPT_FORMAT_TIMESTAMP, buf, len);
			break;

		/* Screen name of sender */
		case 'n':
		case 'N':
			ret = xstrncpy(buf, acct->username, len);
			break;

		/* Screen name / alias  of the receiver */
		case 'R':
			if (dest != NULL)
				ret = xstrncpy(buf, buddy_name(acct, dest), len);
			break;

		case 'r':
			if (dest != NULL)
				ret = xstrncpy(buf, dest, len);
			break;

		/* Message text */
		case 'M':
			if (msg != NULL) {
				msg = acct->proto->filter_text(msg);
				ret = xstrncpy(buf, msg, len);
				free(msg);
			}
			break;

		case 'm':
			if (msg != NULL)
				ret = xstrncpy(buf, msg, len);
			break;

		case 'H':
			if (acct->userhost != NULL) {
				char *host = acct->proto->filter_text(acct->userhost);
				ret = xstrncpy(buf, host, len);
				free(host);
			}
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);


	return (0);
}

static int format_msg_recv(char opt, char *buf, size_t len, va_list ap) {
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	char *dest = va_arg(ap, char *);
 	char *sender = va_arg(ap, char *);
	char *sender_userhost = va_arg(ap, char *);
	char *msg = va_arg(ap, char *);
	int ret = 0;

	switch (opt) {
		/* Timestamp */
		case 'T':
			ret = fill_format_str(OPT_FORMAT_TIMESTAMP, buf, len);
			break;

		/* Screen name of sender */
		case 'n':
			if (sender != NULL)
				ret = xstrncpy(buf, sender, len);
			break;

		/* Screen name / alias of sender */
		case 'N':
			if (sender != NULL)
				ret = xstrncpy(buf, buddy_name(acct, sender), len);
			break;

		/* Screen name / alias  of the receiver */
		case 'R':
			if (dest != NULL)
				ret = xstrncpy(buf, buddy_name(acct, dest), len);
			break;

		case 'r':
			if (dest != NULL)
				ret = xstrncpy(buf, dest, len);
			break;

		/* Message text */
		case 'm':
		case 'M':
			if (msg != NULL) {
				msg = acct->proto->filter_text(msg);
				ret = xstrncpy(buf, msg, len);
				free(msg);
			}
			break;

		case 'H':
			if (acct->userhost != NULL) {
				char *host = acct->proto->filter_text(acct->userhost);
				ret = xstrncpy(buf, host, len);
				free(host);
			}
			break;

		case 'h':
			if (sender_userhost != NULL) {
				char *host = acct->proto->filter_text(sender_userhost);
				ret = xstrncpy(buf, host, len);
				free(host);
			}
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_chat_send(char opt, char *buf, size_t len, va_list ap) {
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	struct chatroom *chat = va_arg(ap, struct chatroom *);
	char *dest = va_arg(ap, char *);
	char *msg = va_arg(ap, char *);

	int ret = 0;

	switch (opt) {
		/* Timestamp */
		case 'T':
			ret = fill_format_str(OPT_FORMAT_TIMESTAMP, buf, len);
			break;

		/* Message source */
		case 'n':
		case 'N':
			ret = xstrncpy(buf, acct->username, len);
			break;

		/* Message destination */
		case 'C':
			if (chat != NULL)
				ret = xstrncpy(buf, chat->title_quoted, len);
			break;

		case 'c':
			if (dest != NULL) {
				dest = acct->proto->filter_text(dest);
				ret = xstrncpy(buf, dest, len);
				free(dest);
			}
			break;

		/* Message text */
		case 'm':
		case 'M':
			if (msg != NULL) {
				msg = acct->proto->filter_text(msg);
				ret = xstrncpy(buf, msg, len);
				free(msg);
			}
			break;

		case 'H':
			if (acct->userhost != NULL) {
				char *host = acct->proto->filter_text(acct->userhost);
				ret = xstrncpy(buf, host, len);
				free(host);
			}
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_chat_recv(char opt, char *buf, size_t len, va_list ap) {
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	struct chatroom *chat = va_arg(ap, struct chatroom *);
	char *dest = va_arg(ap, char *);
	char *src = va_arg(ap, char *);
	char *src_uhost = va_arg(ap, char *);
	char *msg = va_arg(ap, char *);

	int ret = 0;

	switch (opt) {
		/* Timestamp */
		case 'T':
			ret = fill_format_str(OPT_FORMAT_TIMESTAMP, buf, len);
			break;

		/* Message source */
		case 'N':
			if (src != NULL)
				ret = xstrncpy(buf, buddy_name(acct, src), len);
			break;

		case 'n':
			if (src != NULL)
				ret = xstrncpy(buf, src, len);
			break;

		/* Message destination */
		case 'C':
			if (chat != NULL)
				ret = xstrncpy(buf, chat->title_quoted, len);
			break;

		case 'c':
			if (dest != NULL) {
				dest = acct->proto->filter_text(dest);
				ret = xstrncpy(buf, dest, len);
				free(dest);
			}
			break;

		/* Message text */
		case 'M':
		case 'm':
			if (msg != NULL) {
				msg = acct->proto->filter_text(msg);
				ret = xstrncpy(buf, msg, len);
				free(msg);
			}
			break;

		case 'H':
			if (acct->userhost != NULL) {
				char *host = acct->proto->filter_text(acct->userhost);
				ret = xstrncpy(buf, host, len);
				free(host);
			}
			break;

		case 'h':
			if (src_uhost != NULL) {
				src_uhost = acct->proto->filter_text(src_uhost);
				ret = xstrncpy(buf, src_uhost, len);
				free(src_uhost);
			}
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_chat_info(char opt, char *buf, size_t len, va_list ap) {
	struct pork_acct *acct = va_arg(ap, struct pork_acct *);
	struct chatroom *chat = va_arg(ap, struct chatroom *);
	char *chat_nameq = va_arg(ap, char *);
	char *src = va_arg(ap, char *);
	char *dst = va_arg(ap, char *);
	char *msg = va_arg(ap, char *);
	int ret = 0;

	switch (opt) {
		/* Timestamp */
		case 'T':
			ret = fill_format_str(OPT_FORMAT_TIMESTAMP, buf, len);
			break;

		/* Screen name of user who the action originated from */
		case 'n':
		case 'N':
			ret = xstrncpy(buf, buddy_name(acct, src), len);
			break;

		/* Destination, if applicable */
		case 'D':
		case 'd':
			if (dst != NULL) {
				dst = acct->proto->filter_text(dst);
				ret = xstrncpy(buf, dst, len);
				free(dst);
			}
			break;

		/* Chat name (quoted) */
		case 'r':
		case 'R':
			ret = xstrncpy(buf, chat_nameq, len);
			break;

		/* Chat name (full, quoted) */
		case 'U':
		case 'u':
			if (chat != NULL)
				ret = xstrncpy(buf, chat->title_full_quoted, len);

		/* Source's userhost (if available) */
		case 'H':
			if (acct != NULL && chat != NULL && src != NULL) {
				struct chat_user *chat_user;

				chat_user = chat_find_user(acct, chat, src);
				if (chat_user != NULL && chat_user->host != NULL) {
					char *host = acct->proto->filter_text(chat_user->host);
					ret = xstrncpy(buf, host, len);
					free(host);
				}
			}
			break;

		/* Dest's userhost (if available) */
		case 'h':
			if (acct != NULL && chat != NULL && dst != NULL) {
				struct chat_user *chat_user;

				chat_user = chat_find_user(acct, chat, dst);
				if (chat_user != NULL && chat_user->host != NULL) {
					char *host = acct->proto->filter_text(chat_user->host);
					ret = xstrncpy(buf, host, len);
					free(host);
				}
			}
			break;

		case 'm':
		case 'M':
			if (msg != NULL) {
				msg = acct->proto->filter_text(msg);
				ret = xstrncpy(buf, msg, len);
				free(msg);
			}
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_warning(char opt, char *buf, size_t len, va_list ap) {
	char *you = va_arg(ap, char *);
	char *warner = va_arg(ap, char *);
	u_int16_t warn_level = va_arg(ap, unsigned int);
	int ret = 0;

	switch (opt) {
		/* Timestamp */
		case 'T':
			ret = fill_format_str(OPT_FORMAT_TIMESTAMP, buf, len);
			break;

		case 'n':
		case 'N':
			ret = xstrncpy(buf, you, len);
			break;

		case 'u':
		case 'U':
			ret = xstrncpy(buf, warner, len);
			break;

		case 'w':
		case 'W':
			ret = snprintf(buf, len, "%u", warn_level);
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int format_whois(char opt, char *buf, size_t len, va_list ap) {
	char *user = va_arg(ap, char *);
	u_int32_t warn_level = va_arg(ap, unsigned long int);
	u_int32_t idle_time = va_arg(ap, unsigned long int);
	u_int32_t online_since = va_arg(ap, unsigned long int);
	u_int32_t member_since = va_arg(ap, unsigned long int);
	char *info = va_arg(ap, char *);
	int ret = 0;

	switch (opt) {
		/* Timestamp */
		case 'T':
			ret = fill_format_str(OPT_FORMAT_TIMESTAMP, buf, len);
			break;

		/* Name */
		case 'N':
		case 'n':
			ret = xstrncpy(buf, user, len);
			break;

		/* Online since */
		case 'S':
		case 's':
			ret = date_to_str(online_since, buf, len);
			break;

		/* Idle time */
		case 'I':
		case 'i':
			ret = time_to_str(idle_time, buf, len);
			break;

		/* Member since */
		case 'D':
		case 'd':
			ret = date_to_str(member_since, buf, len);
			break;

		/* Warn level */
		case 'W':
		case 'w':
			ret = snprintf(buf, len, "%u", warn_level);
			break;

		/* Profile or away message */
		case 'P':
		case 'A':
			if (info != NULL) {
				char *p = strchr(info, '\n');
				if (p != NULL)
					ret = snprintf(buf, len, "\n%s", info);
				else
					ret = xstrncpy(buf, info, len);
			}
			break;

		/*
		** Same as above, but don't prepend a newline if the profile or away
		** msg contains one.
		*/
		case 'p':
		case 'a':
			ret = xstrncpy(buf, info, len);
			break;

		default:
			return (-1);
	}

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int (*const format_handler[])(char, char *, size_t, va_list) = {
	format_msg_recv,			/* OPT_FORMAT_ACTION_RECV			*/
	format_msg_recv,			/* OPT_FORMAT_ACTION_RECV_STATUS	*/
	format_msg_send,			/* OPT_FORMAT_ACTION_SEND			*/
	format_msg_send,			/* OPT_FORMAT_ACTION_SEND_STATUS	*/
	format_chat_info,			/* OPT_FORMAT_CHAT_CREATE			*/
	format_chat_info,			/* OPT_FORMAT_CHAT_IGNORE			*/
	format_chat_info,			/* OPT_FORMAT_CHAT_INVITE			*/
	format_chat_info,			/* OPT_FORMAT_CHAT_JOIN				*/
	format_chat_info,			/* OPT_FORMAT_CHAT_KICK				*/
	format_chat_info,			/* OPT_FORMAT_CHAT_LEAVE			*/
	format_chat_info,			/* OPT_FORMAT_CHAT_MODE				*/
	format_chat_info,			/* OPT_FORMAT_CHAT_NICK				*/
	format_chat_info,			/* OPT_FORMAT_CHAT_QUIT				*/
	format_chat_recv,			/* OPT_FORMAT_CHAT_RECV				*/
	format_chat_recv,			/* OPT_FORMAT_CHAT_RECV_ACTION		*/
	format_chat_recv,			/* OPT_FORMAT_CHAT_RECV_NOTICE		*/
	format_chat_send,			/* OPT_FORMAT_CHAT_SEND				*/
	format_chat_send,			/* OPT_FORMAT_CHAT_SEND_ACTION		*/
	format_chat_send,			/* OPT_FORMAT_CHAT_SEND_NOTICE		*/
	format_chat_info,			/* OPT_FORMAT_CHAT_TOPIC			*/
	format_chat_info,			/* OPT_FORMAT_CHAT_UNIGNORE			*/
  format_msg_highlight,		/* OPT_FORMAT_HIGHLIGHT			*/
	format_msg_recv,			/* OPT_FORMAT_IM_RECV				*/
	format_msg_recv,			/* OPT_FORMAT_IM_RECV_AUTO			*/
	format_msg_recv,			/* OPT_FORMAT_IM_RECV_STATUS		*/
	format_msg_send,			/* OPT_FORMAT_IM_SEND				*/
	format_msg_send,			/* OPT_FORMAT_IM_SEND_AUTO			*/
	format_msg_send,			/* OPT_FORMAT_IM_SEND_STATUS		*/
	format_msg_recv,			/* OPT_FORMAT_NOTICE_RECV			*/
	format_msg_recv,			/* OPT_FORMAT_NOTICE_RECV_STATUS	*/
	format_msg_send,			/* OPT_FORMAT_NOTICE_SEND			*/
	format_msg_send,			/* OPT_FORMAT_NOTICE_SEND_STATUS	*/
	format_status,				/* OPT_FORMAT_STATUS				*/
	format_status_activity,		/* OPT_FORMAT_STATUS_ACTIVITY		*/
	format_status,				/* OPT_FORMAT_STATUS_CHAT			*/
	format_status_held,			/* OPT_FORMAT_STATUS_HELD			*/
	format_status_idle,			/* OPT_FORMAT_STATUS_IDLE			*/
	format_status_timestamp,	/* OPT_FORMAT_STATUS_TIMESTAMP		*/
	format_status_typing,		/* OPT_FORMAT_STATUS_TYPING			*/
	format_system_alert,		/* OPT_FORMAT_SYSTEM_ALERT			*/
	format_status_timestamp,	/* OPT_FORMAT_TIMESTAMP				*/
	format_warning,				/* OPT_FORMAT_WARN					*/
	format_whois,				/* OPT_FORMAT_WHOIS_AWAY			*/
	format_whois,				/* OPT_FORMAT_WHOIS_IDLE			*/
	format_whois,				/* OPT_FORMAT_WHOIS_MEMBER			*/
	format_whois,				/* OPT_FORMAT_WHOIS_NAME			*/
	format_whois,				/* OPT_FORMAT_WHOIS_SIGNON			*/
	format_whois,				/* OPT_FORMAT_WHOIS_USERINFO		*/
	format_whois,				/* OPT_FORMAT_WHOIS_WARNLEVEL		*/
};

/*
** Do all appropriate '$' substitutions in format strings, except for
** $>, which must be handled after everything else.
*/

int fill_format_str(int type, char *buf, size_t len, ...) {
	va_list ap;
	char *format;
	size_t i = 0;
	int (*handler)(char, char *, size_t, va_list);

	format = opt_get_str(type);
	if (format == NULL) {
		debug("unknown format str: %d", type);
		return (-1);
	}

	--len;
	handler = format_handler[type - OPT_FORMAT_OFFSET];

	while (*format != '\0' && i < len) {
		if (*format == FORMAT_VARIABLE) {
			char result[len + 1];
			int ret_code;

			format++;

			result[0] = '\0';

			va_start(ap, len);
			ret_code = handler(*format, result, sizeof(result), ap);
			va_end(ap);

			if (ret_code == 0) {
				/*
				** The subsitution was successful.
				*/
				int ret;

				ret = xstrncpy(&buf[i], result, len - i);
				if (ret == -1)
					break;

				i += ret;
			} else if (ret_code == 1) {
				/*
				** We should suppress the whole string.
				*/

				*buf = '\0';
				break;
			} else {
				/*
				** Unknown variable -- treat
				** their FORMAT_VARIABLE as a literal.
				*/

				if (i > len - 2)
					break;

				buf[i++] = FORMAT_VARIABLE;
				buf[i++] = *format;
			}
		} else
			buf[i++] = *format;

		format++;
	}

	buf[i] = '\0';
	return (i);
}

void format_apply_justification(char *buf, chtype *ch, size_t len) {
	char *p = buf;
	char *left = NULL;
	char *right = NULL;
	size_t len_left;
	chtype fill_char;

	while ((p = strchr(p, '$')) != NULL) {
		if (p[1] == '>') {
			left = buf;

			*p = '\0';
			right = &p[2];
			break;
		}
		p++;
	}

	len_left = plaintext_to_cstr(ch, len, buf, NULL);
	fill_char = ch[len_left - 1];
	chtype_set(fill_char, ' ');

	/*
	** If there's right-justified text, paste it on.
	*/
	if (right != NULL) {
		chtype ch_right[len];
		size_t len_right;
		size_t diff;
		size_t i;
		size_t j;

		plaintext_to_cstr(ch_right, len, right, NULL);
		len_right = cstrlen(ch_right);

		/*
		** If the right side won't fit, paste as much
		** as possible, leaving at least one cell of whitespace.
		*/

		if (len_left + len_right >= len - 1) {
			int new_rlen;

			new_rlen = len_right - ((len_left + 1 + len_right) - (len - 1));
			if (new_rlen < 0)
				len_right = 0;
			else
				len_right = new_rlen;
		}

		diff = len_left + (--len - (len_left + len_right));

		i = len_left;
		while (i < diff)
			ch[i++] = fill_char;

		j = 0;
		while (i < len && j < len_right && ch_right[j] != 0)
			ch[i++] = ch_right[j++];

		ch[i] = 0;
	} else if (len_left < len - 1) {
		/*
		** If there's no right-justified text, pad out the
		** string to its maximum length with blank characters (spaces)
		** having the color/highlighting attributes of the last character
		** in the string.
		*/
		size_t i = len_left;

		--len;
		while (i < len)
			ch[i++] = fill_char;

		ch[i] = 0;
	}
}
