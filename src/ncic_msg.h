/*
** Based on pork_msg.h
** ncic_msg.h
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_MSG_H__
#define __NCIC_MSG_H__

#ifdef __cplusplus
extern "C" {
#endif

struct autoresp {
	char *name;
	time_t last;
};

int pork_recv_msg(	struct pork_acct *acct,
					char *dest,
					char *sender,
					char *userhost,
					char *msg,
					int arply);

int pork_recv_notice(	struct pork_acct *acct,
						char *dest,
						char *src,
						char *userhost,
						char *msg);

int pork_recv_action(	struct pork_acct *acct,
						char *dest,
						char *src,
						char *userhost,
						char *msg);

int ncic_recv_sys_alert(struct pork_acct *acct, char *msg);
int ncic_recv_highlight_msg(struct pork_acct *acct, char *msg);

int pork_msg_autoreply(struct pork_acct *acct, char *dest, char *msg);
int pork_msg_send_auto(struct pork_acct *acct, char *sender);
int pork_set_away(struct pork_acct *acct, char *msg);
int pork_set_back(struct pork_acct *acct);
int pork_msg_send(struct pork_acct *acct, char *dest, char *msg);
int pork_set_profile(struct pork_acct *acct, char *profile);
int pork_set_idle_time(struct pork_acct *acct, u_int32_t seconds);
int pork_send_warn(struct pork_acct *acct, char *user);
int pork_send_warn_anon(struct pork_acct *acct, char *user);
int pork_change_nick(struct pork_acct *acct, char *nick);
int pork_notice_send(struct pork_acct *acct, char *dest, char *msg);
int pork_action_send(struct pork_acct *acct, char *dest, char *msg);
int pork_signoff(struct pork_acct *acct, char *msg);

#ifdef __cplusplus
}
#endif

#endif /* __NCIC_MSG_H__ */
