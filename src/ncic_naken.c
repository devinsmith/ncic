/*
 * Copyright (c) 2009 Devin Smith <devin@devinsmith.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_queue.h"
#include "ncic_inet.h"
#include "ncic_io.h"
#include "ncic_acct.h"
#include "ncic_opt.h"
#include "ncic_imwindow.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_chat.h"
//#include "ncic_missing.h>

#include "ncic_irc.h"
#include "ncic_naken.h"

int
naken_set_back(irc_session_t *session, char *msg)
{
	char buf[IRC_OUT_BUFLEN];
	int ret;

	ret = snprintf(buf, sizeof(buf), "%% is back\r\n");

	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (irc_send(session, buf, ret));
}

