/*
** Based on pork_transfer.h
** ncic_transfer.h
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_TRANSFER_H__
#define __NCIC_TRANSFER_H__

#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* XXX: Why is this needed?? */
#include "ncic_transfer.h"

struct pork_acct;

enum {
	TRANSFER_DIR_SEND,
	TRANSFER_DIR_RECV,
	TRANSFER_DIR_ANY,
};

enum {
	TRANSFER_STATUS_WAITING,
	TRANSFER_STATUS_ACTIVE,
	TRANSFER_STATUS_COMPLETE,
};

struct file_transfer {
	u_int32_t refnum;
	u_int16_t type;
	u_int16_t status;
	u_int32_t protocol_flags:24;
	u_int32_t quote_fname:1;
	in_port_t lport;
	in_port_t fport;
	char *peer_username;
	char *fname_local;
	char *fname_base;
	char *fname_orig;
	int sock;
	off_t bytes_sent;
	off_t file_len;
	off_t start_offset;
	struct pork_acct *acct;
	FILE *fp;
	void *data;
	struct timeval time_started;
	char faddr_ip[MAX_IPLEN];
	char laddr_ip[MAX_IPLEN];
	char faddr_host[MAX_HOSTLEN];
	char laddr_host[MAX_HOSTLEN];
	struct sockaddr_storage faddr;
	struct sockaddr_storage laddr;
};

struct file_transfer *transfer_find_refnum(	struct pork_acct *acct,
											u_int32_t refnum);

int transfer_send_accepted(struct file_transfer *xfer);
int transfer_recv_accepted(struct file_transfer *xfer);

int transfer_request_send(struct file_transfer *xfer);
int transfer_request_recv(struct file_transfer *xfer);

int transfer_bind_listen_sock(struct file_transfer *xfer, int sock);
int transfer_resume(struct file_transfer *xfer, off_t offset);
int transfer_get(struct file_transfer *xfer, char *filename);
int transfer_abort(struct file_transfer *xfer);
int transfer_send_complete(struct file_transfer *xfer);
int transfer_recv_complete(struct file_transfer *xfer);
int transfer_destroy(struct file_transfer *xfer);
int transfer_send(struct pork_acct *acct, char *dest, char *filename);
char *transfer_status_str(struct file_transfer *xfer);
void transfer_recv_data(int fd, u_int32_t cond, void *data);
void transfer_send_data(int fd, u_int32_t cond, void *data);
double transfer_time_elapsed(struct file_transfer *xfer);
inline double transfer_avg_speed(struct file_transfer *xfer);
inline char *transfer_get_local_hostname(struct file_transfer *xfer);
inline char *transfer_get_remote_hostname(struct file_transfer *xfer);
int transfer_cancel_local(struct file_transfer *xfer);
int transfer_cancel_remote(struct file_transfer *xfer);
int transfer_lost(struct file_transfer *xfer);
u_int32_t transfer_get_all(struct pork_acct *acct);
u_int32_t transfer_abort_all(struct pork_acct *acct, u_int32_t type);

struct file_transfer *transfer_new(	struct pork_acct *acct,
									char *peer,
									int direction,
									char *filename,
									size_t size);


struct file_transfer *transfer_find(struct pork_acct *acct,
									char *peer,
									char *file);

#endif /* __NCIC_TRANSFER_H__ */
