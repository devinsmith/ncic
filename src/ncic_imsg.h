/*
** Heavily based on pork_imsg.h
** ncic_imsg.h
** Copyright (C) 2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_IMSG_H__
#define __NCIC_IMSG_H__

#define IMSG(x) ((struct imsg *) (x))

struct swindow;

enum {
	MSG_TYPE_PRIVMSG_RECV				= (1 << 0),
	MSG_TYPE_PRIVMSG_SEND				= (1 << 1),
	MSG_TYPE_NOTICE_RECV				= (1 << 2),
	MSG_TYPE_NOTICE_SEND				= (1 << 3),
	MSG_TYPE_NOTICE_BROADCAST_RECV		= (1 << 4),
	MSG_TYPE_NOTICE_BROADCAST_SEND		= (1 << 5),
	MSG_TYPE_CHAT_ACTION_SEND			= (1 << 6),
	MSG_TYPE_CHAT_ACTION_RECV			= (1 << 7),
	MSG_TYPE_CHAT_MSG_RECV				= (1 << 8),
	MSG_TYPE_CHAT_MSG_SEND				= (1 << 9),
	MSG_TYPE_CHAT_MSG_RESTRICT_RECV		= (1 << 10),
	MSG_TYPE_CHAT_MSG_RESTRICT_SEND		= (1 << 11),
	MSG_TYPE_CHAT_NOTICE_RECV			= (1 << 12),
	MSG_TYPE_CHAT_NOTICE_RESTRICT_RECV	= (1 << 13),
	MSG_TYPE_CHAT_NOTICE_SEND			= (1 << 14),
	MSG_TYPE_CHAT_NOTICE_RESTRICT_SEND	= (1 << 15),
	MSG_TYPE_CHAT_STATUS				= (1 << 16),
	MSG_TYPE_BACK						= (1 << 17),
	MSG_TYPE_AWAY						= (1 << 18),
	MSG_TYPE_IDLE						= (1 << 19),
	MSG_TYPE_UNIDLE						= (1 << 20),
	MSG_TYPE_SIGNON						= (1 << 21),
	MSG_TYPE_SIGNOFF					= (1 << 22),
	MSG_TYPE_FILE_XFER_STATUS			= (1 << 23),
	MSG_TYPE_FILE_XFER_START			= (1 << 24),
	MSG_TYPE_FILE_XFER_FIN				= (1 << 25),
	MSG_TYPE_CMD_OUTPUT					= (1 << 26),
	MSG_TYPE_STATUS						= (1 << 27),
	MSG_TYPE_ERROR						= (1 << 28),
	MSG_TYPE_LASTLOG					= (1 << 29),
	MSG_TYPE_ALL						= ~0,
};

enum {
	MSG_OPT_NONE						= 0,
	MSG_OPT_TIMESTAMP					= (1 << 0),
	MSG_OPT_BANNER						= (1 << 1),
	MSG_OPT_COLOR						= (1 << 2),
	MSG_OPT_ALL							= ~0,
};

struct imsg {
	chtype *text;
	uint32_t serial;
	uint32_t len;
	uint32_t lines;
};

uint32_t imsg_lines(struct swindow *swindow, struct imsg *imsg);
struct imsg *imsg_new(struct swindow *swindow, chtype *msg, size_t len);
struct imsg *imsg_copy(struct swindow *swindow, struct imsg *imsg);
chtype *imsg_partial(struct swindow *swindow, struct imsg *imsg, uint32_t n);

#endif /* __NCIC_IMSG_H__ */
