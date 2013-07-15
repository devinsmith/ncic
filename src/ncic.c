/*
 * Copyright (c) 2007-2009 Devin Smith <devin@devinsmith.net>
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

#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#ifndef WIN32
#include <pwd.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#endif
#include <time.h>
#include <sys/time.h>

#ifdef HAVE_TERMIOS_H
#	include <termios.h>
#elif defined HAVE_SYS_TERMIOS_H
#	include <sys/termios.h>
#endif

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_set.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_acct.h"
#include "ncic_misc.h"
#include "ncic_input.h"
#include "ncic_bind.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_status.h"
#include "ncic_alias.h"
#include "ncic_opt.h"
#include "ncic_conf.h"
#include "ncic_color.h"
#include "ncic_command.h"
#include "ncic_timer.h"
#include "ncic_io.h"
#include "ncic_proto.h"
#include "ncic_screen.h"
#include "ncic_queue.h"
#include "ncic_inet.h"

struct screen screen;

/*
** The fallback for when no binding for a key exists.
** Insert the key into the input buffer.
*/

static void binding_insert(int key) {
	struct imwindow *imwindow = cur_window();

	if (key > 0 && key <= 0xff)
		input_insert(imwindow->input, key);
}

static inline void binding_run(struct binding *binding) {
	/*
	** Yeah, this is kind of a hack, but it makes things
	** more pleasant (i.e. you don't see status messages
	** about new settings and things like that if the command
	** was run from a key binding (other than hitting enter on
	** the command line).
	*/

	if (binding->key != '\n') {
		int quiet = screen_set_quiet(1);

		run_mcommand(binding->binding);
		screen_set_quiet(quiet);
	} else
		run_mcommand(binding->binding);
}

#ifndef WIN32
/* No resize support under Windows yet. */
static void
resize_display(void) {
	struct winsize size;

	if (ioctl(1, TIOCGWINSZ, &size) != 0) {
		debug("ioctl: %s", strerror(errno));
		pork_exit(-1, NULL, "Fatal error getting screen size\n");
	}

	screen_resize(size.ws_row, size.ws_col);
	screen_refresh();
}
#endif

#ifndef WIN32
static void sigwinch_handler(int sig __notused) {
	pork_io_add_cond(&screen, IO_COND_ALWAYS);
}

static void generic_signal_handler(int sig) {
	pork_exit(sig, NULL, "Caught signal %d. Exiting\n", sig);
}
#endif

void
keyboard_input(int fd, uint32_t cond, void *data)
{
	struct imwindow *imwindow = cur_window();
	struct pork_acct *acct = imwindow->owner;
	int key;

	/*
	** This will be the case only after the program receives SIGWINCH.
	** The screen can't be resized from inside a signal handler..
	*/
	if (cond == IO_COND_ALWAYS) {
		pork_io_del_cond(&screen, IO_COND_ALWAYS);
#ifndef WIN32
		resize_display();
#endif
		return;
	}

	key = wgetinput(screen.status_bar);
	if (key == -1)
		return;

	time(&acct->last_input);
	bind_exec(imwindow->active_binds, key);

	if (acct->connected && acct->marked_idle && opt_get_bool(OPT_REPORT_IDLE)) {
		if (acct->proto->set_idle_time != NULL)
			acct->proto->set_idle_time(acct, 0);
		acct->marked_idle = 0;
		screen_win_msg(cur_window(), 1, 1, 0,
			MSG_TYPE_UNIDLE, "%s is no longer marked idle", acct->username);
	}
}

int
main(int argc, char *argv[])
{
#ifndef WIN32
	struct passwd *pw;
#endif
	char buf[PATH_MAX];
	struct imwindow *imwindow;
	int ret;
	time_t timer_last_run;
	time_t status_last_update = 0;

#ifdef WIN32
	snprintf(buf, sizeof(buf), "ncic");
#else
	pw = getpwuid(getuid());
	if (pw == NULL) {
		fprintf(stderr, "Fatal: Can't get your user info.\n");
		exit(-1);
	}

	snprintf(buf, sizeof(buf), "%s/.ncic", pw->pw_dir);
#endif
	opt_set(OPT_NCIC_DIR, buf);

	if (get_options(argc, argv) != 0) {
		fprintf(stderr, "Fatal: Error getting options.\n");
		exit(-1);
	}

	if (initialize_environment() != 0) {
		fprintf(stderr, "Fatal: Error initializing the terminal.\n");
		exit(-1);
	}

	proto_init();
	color_init();
	pork_io_init();

	if (screen_init(LINES, COLS) == -1)
		pork_exit(-1, NULL, "Fatal: Error initializing the terminal.\n");

#ifndef WIN32
	signal(SIGWINCH, sigwinch_handler);
	signal(SIGTERM, generic_signal_handler);
	signal(SIGQUIT, generic_signal_handler);
	signal(SIGHUP, generic_signal_handler);
	signal(SIGPIPE, SIG_IGN);
#endif

	wmove(screen.status_bar, STATUS_ROWS - 1, 0);
	imwindow = cur_window();

	bind_init(&screen.binds);
	bind_set_handlers(&screen.binds.main, binding_run, binding_insert);
	bind_set_handlers(&screen.binds.blist, binding_run, NULL);

	alias_init(&screen.alias_hash);

	screen_set_quiet(1);
	ret = read_global_config();
	screen_set_quiet(0);

	if (ret != 0)
		screen_err_msg("Error reading the global configuration.");

	status_draw(screen.null_acct);
	screen_draw_input();
	screen_doupdate();

	time(&timer_last_run);
	while (1) {
		time_t time_now;
		int dirty = 0;

		pork_io_run();
		pork_acct_update();

		/*
		** Run the timers at most once per second.
		*/

		time(&time_now);
		if (timer_last_run < time_now) {
			timer_last_run = time_now;
			timer_run(&screen.timer_list);
			pork_acct_reconnect_all();
		}

		imwindow = cur_window();
		dirty = imwindow_refresh(imwindow);

		/*
		** Draw the status bar at most once per second.
		*/

		time(&time_now);
		if (status_last_update < time_now) {
			status_last_update = time_now;
			status_draw(imwindow->owner);
			dirty++;
		}

		dirty += screen_draw_input();

		if (dirty)
			screen_doupdate();
	}

	pork_exit(0, NULL, NULL);
	return 0;
}

/*
** Sign off all acounts and exit. If a message
** is given, print it to the screen.
*/

void pork_exit(int status, char *msg, char *fmt, ...) {
	pork_acct_del_all(msg);
	screen_destroy();
	pork_io_destroy();
	proto_destroy();

	wclear(stdscr);
	wrefresh(stdscr);
	delwin(stdscr);
	endwin();

	if (fmt != NULL) {
		va_list ap;

		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}

	exit(status);
}
