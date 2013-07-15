/*
** Based on pork_irc_dcc.h
** ncic_irc_dcc.h
** Copyright (C) 2004-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_IRC_DCC_H__
#define __NCIC_IRC_DCC_H__

#define IRC_DCC_REJECTED	0x01

int irc_file_accept(struct file_transfer *xfer);
int irc_file_abort(struct file_transfer *xfer);
int irc_recv_data(struct file_transfer *xfer, char *buf, size_t len);
int irc_file_send(struct file_transfer *xfer);

int irc_handler_dcc_send(struct pork_acct *acct, struct irc_input *in);
int irc_handler_dcc_reject(struct pork_acct *acct, struct irc_input *in);
int irc_handler_dcc_resume(struct pork_acct *acct, struct irc_input *in);
int irc_handler_dcc_accept(struct pork_acct *acct, struct irc_input *in);

struct dcc {
	time_t last_active;
	off_t ack;
};

#endif /* __NCIC_IRC_DCC_H__ */
