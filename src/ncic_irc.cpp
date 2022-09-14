/*
** Based on pork_irc.c
** ncic_irc.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <unistd.h>
#include <ncurses.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <ctime>
#include <sys/time.h>
#include <sys/types.h>
#include <cerrno>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_queue.h"
#include "ncic_inet.h"
#include "ncic_acct.h"
#include "ncic_proto.h"
#include "ncic_io.h"
#include "ncic_imwindow.h"
#include "ncic_screen_io.h"
#include "ncic_chat.h"
#include "ncic_log.h"

#include "ncic_irc.h"
#include "ncic_naken.h"

static void irc_event(int sock, u_int32_t cond, void *data) {
    irc_session_t *session = static_cast<irc_session_t *>(data);

	if (cond & IO_COND_READ) {
		if (naken_input_dispatch(session) == -1) {
			struct pork_acct *acct = static_cast<struct pork_acct *>(session->data);

			pork_sock_err(acct, sock);
      IoManager::instance().delete_key(data);
			int ret = pork_acct_disconnected(acct);
      log_tmsg(0, "Acct disconnected: %d", ret);
      session->data = nullptr;

			return;
		}
	}

	irc_flush_outq(session);
}

static void irc_connected(int sock, u_int32_t cond, void *data) {
	int ret;
    irc_session_t *session = static_cast<irc_session_t *>(data);

  IoManager::instance().delete_key(data);

	ret = sock_is_error(sock);
	if (ret != 0) {
		struct pork_acct *acct = (struct pork_acct *)session->data;
		char *errstr = strerror(ret);

		screen_err_msg("network error: %s: %s", acct->username, errstr);

		close(sock);
		pork_acct_disconnected(acct);
	} else {
		session->sock = sock;

    if (session->use_ssl) {

      /* Register the error strings for libcrypto & libssl */
      SSL_load_error_strings();
      /* Register the available ciphers and digests */
      SSL_library_init();
      OpenSSL_add_all_algorithms();

      // New context saying we are a client, and using SSL 2 or 3
      session->sslContext = SSL_CTX_new(TLS_client_method());
      if (session->sslContext == nullptr) // Dumps to stderr, yuck
        ERR_print_errors_fp(stderr);

      SSL_CTX_set_verify(session->sslContext, SSL_VERIFY_NONE, nullptr);
      SSL_CTX_set_verify_depth(session->sslContext, 0);
      SSL_CTX_set_mode(session->sslContext, SSL_MODE_AUTO_RETRY);
      SSL_CTX_set_session_cache_mode(session->sslContext, SSL_SESS_CACHE_CLIENT);

      // Create an SSL struct for the connection
      session->sslHandle = SSL_new(session->sslContext);
      if (session->sslHandle == nullptr)
        ERR_print_errors_fp(stderr);

      // Connect the SSL struct to our connection
      if (!SSL_set_fd(session->sslHandle, session->sock))
        ERR_print_errors_fp(stderr);

      while ((ret = SSL_connect(session->sslHandle)) == -1) {
        fd_set fds;
        int ssl_err;
        struct pork_acct *acct = (struct pork_acct *) session->data;

        FD_ZERO(&fds);
        FD_SET(session->sock, &fds);

        switch (ssl_err = SSL_get_error(session->sslHandle, ret)) {
          case SSL_ERROR_WANT_READ:
            select(session->sock + 1, &fds, nullptr, nullptr, nullptr);
            break;
          case SSL_ERROR_WANT_WRITE:
            select(session->sock + 1, nullptr, &fds, nullptr, nullptr);
            break;
          default:
            screen_err_msg("network error: %s: could not connect %d",
                           acct->username, ssl_err);
            return;
            break;
        }
      }
    }

		sock_setflags(sock, 0);

		/* enable keep alive */
		sock_setkeepalive(sock);
    IoManager::instance().add(sock, IO_COND_READ, data, data, irc_event);
		irc_send_login(session);
	}
}

static int irc_init(struct pork_acct *acct) {
	irc_session_t *session = (irc_session_t *)xcalloc(1, sizeof(*session));

	session->outq = queue_new(0);
	session->inq = queue_new(0);
	session->sock = -1;
	session->sslHandle = nullptr;
	session->sslContext = nullptr;
  session->use_ssl = true;

	session->data = acct;
	acct->data = session;
	return (0);
}

static int irc_free(struct pork_acct *acct) {
	irc_session_t *session = acct->data;
	u_int32_t i;

	if (session->sslHandle != nullptr) {
		SSL_shutdown(session->sslHandle);
		SSL_free(session->sslHandle);
	}

	if (session->sslContext != nullptr) {
		SSL_CTX_free(session->sslContext);
  }

  if (session->server != nullptr) {
    free_str_wipe(session->server);
  }

	queue_destroy(session->inq, free);
	queue_destroy(session->outq, free);

  IoManager::instance().delete_key(session);
	free(session);
	return (0);
}

static int irc_update(struct pork_acct *acct) {
	irc_session_t *session = (irc_session_t *)acct->data;
	time_t time_now;

	if (session == nullptr)
		return (-1);

	time(&time_now);
	if (session->last_update + 300 <= time_now && acct->connected) {
		irc_send_pong(session, acct->server);
		session->last_update = time_now;
	}

	return (0);
}

static u_int32_t irc_add_servers(struct pork_acct *acct, char *str) {
	irc_session_t *session = acct->data;

  if (blank_str(str)) {
    return 0;
  }

  if (strstr(str, "--no-ssl") != nullptr) {
    session->use_ssl = false;
  }

  session->server = xstrdup(str);
	return 1;
}

static int irc_do_connect(struct pork_acct *acct, char *args) {
  irc_session_t *session = acct->data;
	int sock;
	int ret;

	if (args == nullptr) {
		screen_err_msg("Error: Syntax is /connect <nick>[=passwd] <server>[:<port>]");
		return (-1);
	}

	if (irc_add_servers(acct, args) < 1) {
		screen_err_msg("Error: %s: No server specified", acct->username);
		return (-1);
	}

	screen_err_msg("Server is %s", session->server);
	ret = irc_connect(acct, session->server, &sock);
	if (ret == 0)
		irc_connected(sock, 0, session);
	else if (ret == -EINPROGRESS)
    IoManager::instance().add(sock, IO_COND_WRITE, session, session, irc_connected);
	else
		return (-1);

	return (0);
}

static int irc_connect_abort(struct pork_acct *acct) {
    auto *session = acct->data;

	close(session->sock);
  IoManager::instance().delete_key(session);
	return (0);
}

static int irc_reconnect(struct pork_acct *acct, char *args __notused) {
	int sock;
	int ret;
  irc_session_t *session = acct->data;

	ret = irc_connect(acct, session->server, &sock);
	if (ret == 0) {
		irc_connected(sock, 0, session);
	} else if (ret == -EINPROGRESS)
    IoManager::instance().add(sock, IO_COND_WRITE, session, session, irc_connected);
	else
		return (-1);

	return (0);
}

static int irc_chan_send(struct pork_acct *acct,
			struct chatroom *chat,
			const char *target,
			char *msg)
{
    irc_session_t *session = static_cast<irc_session_t *>(acct->data);

    return (naken_send(session, msg));
}

static int irc_quit(struct pork_acct *acct, const char *reason) {
	if (acct->connected) {
        irc_session_t *session = static_cast<irc_session_t *>(acct->data);
        return (irc_send_quit(session, reason));
    }

	return (-1);
}

static int irc_away(struct pork_acct *acct, char *msg) {
    irc_session_t *session = static_cast<irc_session_t *>(acct->data);
	return (irc_set_away(session, msg));
}

static int irc_back(struct pork_acct *acct) {
    irc_session_t *session = static_cast<irc_session_t *>(acct->data);
	return (naken_set_back(session, nullptr));
}

char *irc_text_filter(char *str) {
  if (str == nullptr)
    return (xstrdup(""));

  return xstrdup(str);

}

int irc_chan_free(struct pork_acct *acct, void *data) {
	free(data);

	return (0);
}

int irc_proto_init(struct pork_proto *proto) {
	proto->chat_send = irc_chan_send;
	proto->chat_free = irc_chan_free;

	proto->connect = irc_do_connect;
	proto->connect_abort = irc_connect_abort;
	proto->reconnect = irc_reconnect;
	proto->free = irc_free;
	proto->init = irc_init;
	proto->signoff = irc_quit;
	proto->update = irc_update;
	proto->user_compare = strcasecmp;
	proto->change_nick = nullptr;
	proto->filter_text = irc_text_filter;
	proto->set_away = irc_away;
	proto->set_back = irc_back;
	return (0);
}
