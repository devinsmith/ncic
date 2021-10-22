/*
** Based on pork_command.c
** ncic_command.c - interface to commands typed by the user
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
** Copyright (C) 2002-2004 Amber Adams <amber@ojnk.net>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <ncurses.h>
#include <sys/types.h>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <cerrno>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_set.h"
#include "ncic_imsg.h"
#include "ncic_imwindow.h"
#include "ncic_proto.h"
#include "ncic_acct.h"
#include "ncic_input.h"
#include "ncic_bind.h"
#include "ncic_screen.h"
#include "ncic_screen_io.h"
#include "ncic_alias.h"
#include "ncic_chat.h"
#include "ncic_conf.h"
#include "ncic_msg.h"
#include "ncic_command.h"
#include "ncic_command_defs.h"
#include "ncic_help.h"

extern struct sockaddr_storage local_addr;
extern in_port_t local_port;

static void print_binding(void *data, void *nothing);
static void print_alias(void *data, void *nothing);
static int cmd_compare(const void *l, const void *r);
static void print_timer(void *data, void *nothing);
static int run_one_command(char *str, u_int32_t set);

enum {
  CMDSET_MAIN,
  CMDSET_WIN,
  CMDSET_HISTORY,
  CMDSET_INPUT,
  CMDSET_SCROLL,
  CMDSET_CHAT,
  CMDSET_ACCT,
  CMDSET_PROTO,
};

/*
** Note that the "struct command" arrays are arranged in alphabetical
** order. They have to be like that.
*/

/*
** The / command set.
*/

static struct command command[] = {
	{ "",			cmd_send			},
	{ "acct",		cmd_acct			},
	{ "alias",		cmd_alias			},
	{ "auto",		cmd_auto			},
	{ "away",		cmd_away			},
	{ "bind",		cmd_bind			},
	{ "chat",		cmd_chat			},
	{ "connect",	cmd_connect			},
	{ "ctcp",		cmd_ctcp			},
	{ "disconnect", cmd_disconnect		},
	{ "echo",		cmd_echo			},
	{ "help",		cmd_help			},
	{ "history",	cmd_history			},
	{ "idle",		cmd_idle			},
	{ "input",		cmd_input			},
	{ "laddr",		cmd_laddr			},
	{ "lastlog",	cmd_lastlog			},
	{ "load",		cmd_load			},
	{ "lport",		cmd_lport			},
	{ "me",			cmd_me				},
	{ "mode",		cmd_mode			},
	{ "msg",		cmd_msg				},
	{ "nick",		cmd_nick			},
	{ "notice",		cmd_notice			},
	{ "ping",		cmd_ping			},
	{ "profile",	cmd_profile,		},
	{ "query",		cmd_query			},
	{ "quit",		cmd_quit			},
	{ "refresh",	cmd_refresh			},
	{ "save",		cmd_save			},
	{ "scroll",		cmd_scroll			},
	{ "set",		cmd_set				},
	{ "unalias",	cmd_unalias			},
	{ "unbind",		cmd_unbind			},
	{ "whowas",		cmd_whowas			},
	{ "win",		cmd_win				},
};

/*
** /input commands
*/

static struct command input_command[] = {
	{ "backspace",				cmd_input_bkspace			},
	{ "clear",					cmd_input_clear				},
	{ "clear_next_word",		cmd_input_clear_next		},
	{ "clear_prev_word",		cmd_input_clear_prev		},
	{ "clear_to_end",			cmd_input_clear_to_end		},
	{ "clear_to_start",			cmd_input_clear_to_start	},
	{ "delete",					cmd_input_delete			},
	{ "end",					cmd_input_end				},
	{ "find_next_cmd",			cmd_input_find_next_cmd		},
	{ "focus_next",				cmd_input_focus_next		},
	{ "insert",					cmd_input_insert			},
	{ "left",					cmd_input_left				},
	{ "next_word",				cmd_input_next_word			},
	{ "prev_word",				cmd_input_prev_word			},
	{ "prompt",					cmd_input_prompt			},
	{ "right",					cmd_input_right				},
	{ "send",					cmd_input_send				},
	{ "start",					cmd_input_start				},
};

USER_COMMAND(cmd_input_bkspace) {
	input_bkspace(cur_window()->input);
}

USER_COMMAND(cmd_input_clear) {
	input_clear_line(cur_window()->input);
}

USER_COMMAND(cmd_input_clear_prev) {
	input_clear_prev_word(cur_window()->input);
}

USER_COMMAND(cmd_input_clear_next) {
	input_clear_next_word(cur_window()->input);
}

USER_COMMAND(cmd_input_clear_to_end) {
	input_clear_to_end(cur_window()->input);
}

USER_COMMAND(cmd_input_focus_next) {
	struct imwindow *win = cur_window();

	imwindow_switch_focus(win);
}

USER_COMMAND(cmd_input_clear_to_start) {
	input_clear_to_start(cur_window()->input);
}

USER_COMMAND(cmd_input_delete) {
	input_delete(cur_window()->input);
}

USER_COMMAND(cmd_input_end) {
	input_end(cur_window()->input);
}

USER_COMMAND(cmd_input_insert) {
	if (args != nullptr)
		input_insert_str(cur_window()->input, args);
}

USER_COMMAND(cmd_input_left) {
	input_move_left(cur_window()->input);
}

USER_COMMAND(cmd_input_prev_word) {
	input_prev_word(cur_window()->input);
}

USER_COMMAND(cmd_input_prompt) {
	if (args == nullptr) {
		input_set_prompt(cur_window()->input, nullptr);
		screen_cmd_output("Input prompt cleared");
	} else {
		if (input_set_prompt(cur_window()->input, args) == -1)
			screen_err_msg("The requested prompt is too long for the screen");
		else {
			screen_cmd_output("Input prompt set to %s", args);
		}
	}
}

USER_COMMAND(cmd_input_next_word) {
	input_next_word(cur_window()->input);
}

USER_COMMAND(cmd_input_right) {
	input_move_right(cur_window()->input);
}

USER_COMMAND(cmd_input_send) {
	struct imwindow *imwindow = cur_window();
	struct input *input = imwindow->input;
	static int recursion;

	/*
	** This is kind of a hack, but it's necessary if the client
	** isn't to crash when someone types "/input send"
	*/

	if (recursion == 1 && args == nullptr)
		return;

	recursion = 1;

	if (args != nullptr)
		input_set_buf(input, args);

	if (input->len > 0) {
		char *input_str = xstrdup(input_get_buf_str(input));

		if (args == nullptr)
			input_history_add(input);

		input_clear_line(input);

		if (input_str[0] == opt_get_char(OPT_CMDCHARS))
			run_command(&input_str[1]);
		else
			cmd_send(input_str);

		free(input_str);
	}

	recursion = 0;
}

USER_COMMAND(cmd_input_start) {
	input_home(cur_window()->input);
}

/*
** /scroll commands
*/

static struct command scroll_command[] = {
	{ "by",				cmd_scroll_by			},
	{ "down",			cmd_scroll_down			},
	{ "end",			cmd_scroll_end			},
	{ "page_down",		cmd_scroll_pgdown		},
	{ "page_up",		cmd_scroll_pgup			},
	{ "start",			cmd_scroll_start		},
	{ "up",				cmd_scroll_up			},
};

USER_COMMAND(cmd_scroll_by) {
	int lines;

	if (args == nullptr)
		return;

	if (str_to_int(args, &lines) != 0) {
		screen_err_msg("Invalid number of lines: %s", args);
		return;
	}

	imwindow_scroll_by(cur_window(), lines);
}

USER_COMMAND(cmd_scroll_down) {
	imwindow_scroll_down(cur_window());
}

USER_COMMAND(cmd_scroll_end) {
	imwindow_scroll_end(cur_window());
}

USER_COMMAND(cmd_scroll_pgdown) {
	imwindow_scroll_page_down(cur_window());
}

USER_COMMAND(cmd_scroll_pgup) {
	imwindow_scroll_page_up(cur_window());
}

USER_COMMAND(cmd_scroll_start) {
	imwindow_scroll_start(cur_window());
}

USER_COMMAND(cmd_scroll_up) {
	imwindow_scroll_up(cur_window());
}

/*
** /win commands.
*/

static struct command window_command[] = {
	{ "bind",				cmd_win_bind		},
	{ "bind_next",			cmd_win_bind_next	},
	{ "clear",				cmd_win_clear		},
	{ "close",				cmd_win_close		},
	{ "dump",				cmd_win_dump		},
	{ "erase",				cmd_win_erase		},
	{ "ignore",				cmd_win_ignore		},
	{ "list",				cmd_win_list		},
	{ "next",				cmd_win_next		},
	{ "prev",				cmd_win_prev		},
	{ "rename",				cmd_win_rename		},
	{ "renumber",			cmd_win_renumber	},
	{ "set",				cmd_win_set			},
	{ "skip",				cmd_win_skip		},
	{ "swap",				cmd_win_swap		},
	{ "unignore",			cmd_win_unignore	},
	{ "unskip",				cmd_win_unskip		},
};

USER_COMMAND(cmd_win_bind) {
	struct imwindow *imwindow = cur_window();
	u_int32_t refnum;
	int ret;

	if (args == nullptr || blank_str(args)) {
		if (imwindow->owner != nullptr && imwindow->owner->username != nullptr) {
			screen_cmd_output("This window is bound to account %s [refnum %u]",
				imwindow->owner->username, imwindow->owner->refnum);
		} else
			screen_cmd_output("This window is bound to no account");

		return;
	}

	if (str_to_uint(args, &refnum) == -1) {
		screen_err_msg("Bad account refnum: %s", args);
		return;
	}

	ret = imwindow_bind_acct(imwindow, refnum);
	if (ret == -1) {
		if (imwindow->type == WIN_TYPE_CHAT)
			screen_err_msg("You can't rebind chat windows");
		else
			screen_err_msg("Account %s isn't signed on", args);
	} else {
		screen_cmd_output("This window is now bound to account %s [refnum %u]",
			imwindow->owner->username, imwindow->owner->refnum);
	}
}

USER_COMMAND(cmd_win_bind_next) {
	if (imwindow_bind_next_acct(cur_window()) != -1)
		screen_refresh();
}

USER_COMMAND(cmd_win_clear) {
	imwindow_clear(cur_window());
}

USER_COMMAND(cmd_win_close) {
	screen_close_window(cur_window());
}

USER_COMMAND(cmd_win_dump) {
	if (args == nullptr || blank_str(args)) {
		screen_err_msg("No output file specified");
	} else {
		char buf[4096];

		expand_path(args, buf, sizeof(buf));
		imwindow_dump_buffer(cur_window(), buf);
	}
}

USER_COMMAND(cmd_win_erase) {
	imwindow_erase(cur_window());
}

USER_COMMAND(cmd_win_ignore) {
	struct imwindow *win;

	if (args != nullptr && !blank_str(args)) {
		u_int32_t refnum;

		if (str_to_uint(args, &refnum) != 0) {
			screen_err_msg("Bad window refnum: %s", args);
			return;
		}

		win = imwindow_find_refnum(refnum);
		if (win == nullptr) {
			screen_err_msg("No window with refnum %u", refnum);
			return;
		}
	} else
		win = cur_window();

	imwindow_ignore(win);
}

USER_COMMAND(cmd_win_list) {
	dlist_t *cur;
	static const char *win_types[] = { "Status", "Conv", "Chat" };

	screen_cmd_output("Window List:");

	screen_cmd_output("REFNUM\t\tNAME\t\tTYPE\t\tTARGET");
	cur = screen.window_list;
	do {
		struct imwindow *imwindow = (struct imwindow *)cur->data;

		screen_cmd_output("%u\t\t\t%s\t\t%s\t\t%s",
			imwindow->refnum, imwindow->name,
			win_types[imwindow->type], imwindow->target);

		cur = cur->next;
	} while (cur != screen.window_list);
}

USER_COMMAND(cmd_win_next) {
	screen_cycle_fwd();
}

USER_COMMAND(cmd_win_prev) {
	screen_cycle_bak();
}

USER_COMMAND(cmd_win_rename) {
	struct imwindow *win = cur_window();

	if (args == nullptr)
		screen_cmd_output("Window %u has name \"%s\"", win->refnum, win->name);
	else
		imwindow_rename(win, args);
}

USER_COMMAND(cmd_win_renumber) {
	u_int32_t num;

	if (args == nullptr || blank_str(args)) {
		screen_cmd_output("This is window %u", cur_window()->refnum);
		return;
	}

	if (str_to_uint(args, &num) != 0) {
		screen_err_msg("Bad window number: %s", args);
		return;
	}

	screen_renumber(cur_window(), num);
}

USER_COMMAND(cmd_win_set) {
	char *variable;
	char *value;
	int opt;

	variable = strsep(&args, " ");
	if (variable == nullptr || blank_str(variable)) {
		wopt_print(cur_window());
		return;
	}

	opt = wopt_find(variable);
	strtoupper(variable);
	if (opt == -1) {
		screen_err_msg("Unknown variable: %s", variable);
		return;
	}

	value = args;
	if (value == nullptr || blank_str(value)) {
		wopt_print_var(cur_window(), opt, "is set to");
		return;
	}

	if (wopt_set(cur_window(), opt, value) == -1)
		screen_nocolor_msg("Bad argument for %s: %s", variable, value);
	else
		wopt_print_var(cur_window(), opt, "set to");
}

USER_COMMAND(cmd_win_skip) {
	struct imwindow *win;

	if (args != nullptr && !blank_str(args)) {
		u_int32_t refnum;

		if (str_to_uint(args, &refnum) != 0) {
			screen_err_msg("Bad window refnum: %s", args);
			return;
		}

		win = imwindow_find_refnum(refnum);
		if (win == nullptr) {
			screen_err_msg("No window with refnum %u", refnum);
			return;
		}
	} else
		win = cur_window();

	imwindow_skip(win);
}

USER_COMMAND(cmd_win_swap) {
	u_int32_t num;

	if (args == nullptr || blank_str(args))
		return;

	if (str_to_uint(args, &num) != 0) {
		screen_err_msg("Invalid window refnum: %s", args);
		return;
	}

	if (screen_goto_window(num) != 0)
		screen_err_msg("No such window: %s", args);
}

USER_COMMAND(cmd_win_unignore) {
	struct imwindow *win;

	if (args != nullptr && !blank_str(args)) {
		u_int32_t refnum;

		if (str_to_uint(args, &refnum) != 0) {
			screen_err_msg("Bad window refnum: %s", args);
			return;
		}

		win = imwindow_find_refnum(refnum);
		if (win == nullptr) {
			screen_err_msg("No window with refnum %u", refnum);
			return;
		}
	} else
		win = cur_window();

	imwindow_unignore(win);
}

USER_COMMAND(cmd_win_unskip) {
	struct imwindow *win;

	if (args != nullptr && !blank_str(args)) {
		u_int32_t refnum;

		if (str_to_uint(args, &refnum) != 0) {
			screen_err_msg("Bad window refnum: %s", args);
			return;
		}

		win = imwindow_find_refnum(refnum);
		if (win == nullptr) {
			screen_err_msg("No window with refnum %u", refnum);
			return;
		}
	} else
		win = cur_window();

	imwindow_unskip(win);
}

/*
** History Manipulation.
*/

static struct command history_command[] = {
	{ "clear",		cmd_history_clear	},
	{ "list",		cmd_history_list	},
	{ "next",		cmd_history_next	},
	{ "prev",		cmd_history_prev	},
};

USER_COMMAND(cmd_history_clear) {
	input_history_clear(cur_window()->input);
}

USER_COMMAND(cmd_history_list) {
	struct imwindow *win = cur_window();
	struct input *input = win->input;
	dlist_t *cur = input->history_end;
	u_int32_t i = 0;

	if (cur == nullptr)
		return;

	screen_win_msg(win, 0, 0, 0, MSG_TYPE_CMD_OUTPUT, "Command history:");

	do {
		screen_win_msg(win, 0, 0, 0, MSG_TYPE_CMD_OUTPUT, "%u: %s", i++,
			(char *) cur->data);
		cur = cur->prev;
	} while (cur != nullptr);
}

USER_COMMAND(cmd_history_next) {
	input_history_next(cur_window()->input);
}

USER_COMMAND(cmd_history_prev) {
	input_history_prev(cur_window()->input);
}

/*
** /buddy commands
*/


USER_COMMAND(cmd_buddy_awaymsg) {
	struct imwindow *win = cur_window();
	struct pork_acct *acct = win->owner;

	if (acct->proto->get_away_msg == nullptr)
		return;

	if (args == nullptr || blank_str(args)) {
		if (win->type == WIN_TYPE_PRIVMSG)
			args = win->target;
		else
			args = acct->username;
	}

	acct->proto->get_away_msg(acct, args);
}

USER_COMMAND(cmd_buddy_privacy_mode) {
	struct pork_acct *acct = cur_window()->owner;
	int mode = -1;

	if (acct->proto->set_privacy_mode == nullptr)
		return;

	if (args != nullptr)
		str_to_int(args, &mode);

	mode = acct->proto->set_privacy_mode(acct, mode);
	screen_cmd_output("Privacy mode for %s is %d", acct->username, mode);
}



USER_COMMAND(cmd_buddy_profile) {
	struct imwindow *win = cur_window();
	struct pork_acct *acct = win->owner;

	if (acct->proto->get_profile == nullptr)
		return;

	if (args == nullptr || blank_str(args)) {
		if (win->type == WIN_TYPE_PRIVMSG)
			args = win->target;
		else
			args = acct->username;
	}

	acct->proto->get_profile(acct, args);
}

USER_COMMAND(cmd_buddy_report_idle) {
	struct pork_acct *acct = cur_window()->owner;

	if (acct->proto->set_report_idle == nullptr)
		return;

	if (args != nullptr && !blank_str(args)) {
		u_int32_t mode;

		if (str_to_uint(args, &mode) != 0) {
			screen_err_msg("Invalid number: %s", args);
			return;
		}

		acct->proto->set_report_idle(acct, mode);
	}

	screen_cmd_output("The reporting of idle time for %s is %s",
		acct->username, (acct->report_idle ? "enabled" : "disabled"));
}

USER_COMMAND(cmd_buddy_warn) {
	struct imwindow *win = cur_window();
	struct pork_acct *acct = win->owner;

	if (acct->proto->warn == nullptr)
		return;

	if (args == nullptr || blank_str(args)) {
		if (win->type == WIN_TYPE_PRIVMSG)
			args = win->target;
		else
			return;
	}

	pork_send_warn(acct, args);
}

USER_COMMAND(cmd_buddy_warn_anon) {
	struct imwindow *win = cur_window();
	struct pork_acct *acct = win->owner;

	if (acct->proto->warn_anon == nullptr)
		return;

	if (args == nullptr || blank_str(args)) {
		if (win->type == WIN_TYPE_PRIVMSG)
			args = win->target;
		else
			return;
	}

	pork_send_warn_anon(acct, args);
}

/*
** acct commands
*/

static struct command acct_command[] = {
	{ "save",	cmd_acct_save		},
	{ "set",	cmd_acct_set		},
};

USER_COMMAND(cmd_acct_save) {
	pork_acct_save(cur_window()->owner);
}

USER_COMMAND(cmd_acct_set) {
}

/*
** /chat commands
*/

static struct command chat_command[] = {
	{ "ban",				cmd_chat_ban			},
	{ "ignore",				cmd_chat_ignore			},
	{ "invite",				cmd_chat_invite			},
	{ "join",				cmd_chat_join			},
	{ "leave",				cmd_chat_leave			},
	{ "list",				cmd_chat_list			},
	{ "send",				cmd_chat_send			},
	{ "topic",				cmd_chat_topic			},
	{ "unignore",			cmd_chat_unignore		},
};

USER_COMMAND(cmd_chat_ban) {
	struct imwindow *win = cur_window();
	struct pork_acct *acct = win->owner;
	struct chatroom *chat;
	char *arg1;
	char *arg2;

	if (args == nullptr)
		return;

	arg1 = strsep(&args, " ");

	chat = chat_find(acct, arg1);
	if (chat == nullptr) {
		if (win->type == WIN_TYPE_CHAT && win->data != nullptr)
			chat_ban(acct, (struct chatroom *)win->data, arg1);
		else
			screen_err_msg("%s is not a member of %s", acct->username, arg1);

		return;
	}

	arg2 = strsep(&args, " ");
	if (arg2 != nullptr)
		chat_ban(acct, chat, arg2);
}

USER_COMMAND(cmd_chat_ignore) {
	struct imwindow *imwindow = cur_window();
	struct pork_acct *acct = imwindow->owner;
	char *chat_name;
	char *user_name;

	if (args == nullptr)
		return;

	chat_name = strsep(&args, " ");
	user_name = args;

	if (user_name == nullptr) {
		struct chatroom *chat = (struct chatroom *)imwindow->data;

		if (imwindow->type != WIN_TYPE_CHAT || chat == nullptr) {
			screen_err_msg("You must specify a chat room if the current window is not a chat window");
			return;
		}

		user_name = chat_name;
		chat_name = chat->title;
	}

	chat_ignore(acct, chat_name, user_name);
}

USER_COMMAND(cmd_chat_invite) {
	struct imwindow *imwindow = cur_window();
	struct pork_acct *acct = imwindow->owner;
	char *chat_name;
	char *user_name;
	char *invite_msg;

	if (args == nullptr)
		return;

	chat_name = strsep(&args, " ");
	user_name = strsep(&args, " ");
	invite_msg = args;

	if (user_name == nullptr) {
		struct chatroom *chat = (struct chatroom *)imwindow->data;

		if (imwindow->type != WIN_TYPE_CHAT || chat == nullptr) {
			screen_err_msg("You must specify a chat room if the current window is not a chat window");
			return;
		}

		user_name = chat_name;
		chat_name = chat->title;
	}

	chat_invite(acct, chat_name, user_name, invite_msg);
}

USER_COMMAND(cmd_chat_join) {
	screen_err_msg("cmd_chat_join");
	chat_join(cur_window()->owner, args);
}

USER_COMMAND(cmd_chat_leave) {
	struct imwindow *win = cur_window();
	char *name = args;

	if (name == nullptr || blank_str(name)) {
		struct chatroom *chat;

		if (win->type != WIN_TYPE_CHAT) {
			screen_err_msg("You must specify a chat room if the current window is not a chat window");
			return;
		}

		if (win->data == nullptr)
			return;

		chat = (chatroom *)win->data;
		name = chat->title;
	}

	chat_leave(win->owner, name, 1);
}

USER_COMMAND(cmd_chat_list) {
	chat_list(cur_window()->owner);
}

USER_COMMAND(cmd_chat_send) {
	struct pork_acct *acct = cur_window()->owner;
	struct imwindow *win;
	char *chat_name;

	if (args == nullptr)
		return;

	chat_name = strsep(&args, " ");
	if (chat_name == nullptr || args == nullptr) {
		screen_err_msg("You must specify a chatroom and a message");
		return;
	}

	win = imwindow_find_chat_target(acct, chat_name);
	if (win == nullptr || win->data == nullptr) {
		screen_err_msg("%s is not joined to %s", acct->username, chat_name);
		return;
	}

	chat_send_msg(acct, (chatroom *)win->data, chat_name, args);
}

USER_COMMAND(cmd_chat_topic) {
	struct imwindow *win = cur_window();
	struct pork_acct *acct = win->owner;
	char *topic = nullptr;
	struct chatroom *chat = nullptr;

	if (acct->proto->chat_set_topic == nullptr)
		return;

	if (args != nullptr) {
		topic = strchr(args, ' ');
		if (topic != nullptr)
			*topic++ = '\0';

		chat = chat_find(acct, args);
	}

	if (chat == nullptr) {
		if (topic != nullptr)
			topic[-1] = ' ';

		topic = args;

		if (win->type == WIN_TYPE_CHAT)
			chat = (chatroom *)win->data;
		else {
			screen_err_msg("You must specify a chat room if the current window isn't a chat window");
			return;
		}
	}

	acct->proto->chat_set_topic(acct, chat, topic);
}

USER_COMMAND(cmd_chat_unignore) {
	struct imwindow *imwindow = cur_window();
	struct pork_acct *acct = imwindow->owner;
	char *chat_name;
	char *user_name;

	if (args == nullptr)
		return;

	chat_name = strsep(&args, " ");
	user_name = args;

	if (user_name == nullptr) {
		struct chatroom *chat = (chatroom *)imwindow->data;

		if (imwindow->type != WIN_TYPE_CHAT || chat == nullptr) {
			screen_err_msg("You must specify a chat room if the current window is not a chat window");
			return;
		}

		user_name = chat_name;
		chat_name = chat->title;
	}

	chat_unignore(acct, chat_name, user_name);
}

static struct command_set {
	struct command *set;
	size_t elem;
	const char *type;
} command_set[] = {
	{	command,			array_elem(command),			"" 			},
	{	window_command,		array_elem(window_command),		"win "		},
	{	history_command,	array_elem(history_command),	"history "	},
	{	input_command,		array_elem(input_command),		"input "	},
	{	scroll_command,		array_elem(scroll_command),		"scroll "	},
	{	chat_command,		array_elem(chat_command),		"chat "		},
	{	acct_command,		array_elem(acct_command),		"acct "		},
};

/*
** Main command set.
*/

USER_COMMAND(cmd_alias) {
	char *alias;
	char *str;

	alias = strsep(&args, " ");
	if (alias == nullptr || blank_str(alias)) {
		hash_iterate(&screen.alias_hash, print_alias, nullptr);
		return;
	}

	str = args;
	if (str == nullptr || blank_str(str)) {
		struct alias *lalias = alias_find(&screen.alias_hash, alias);

		if (lalias != nullptr) {
			screen_cmd_output("%s is aliased to %s%s",
				lalias->alias, lalias->orig,
				(lalias->args != nullptr ? lalias->args : ""));
		} else
			screen_err_msg("There is no alias for %s", alias);

		return;
	}

	if (alias_add(&screen.alias_hash, alias, str) == 0) {
		struct alias *lalias = alias_find(&screen.alias_hash, alias);

		if (lalias != nullptr) {
			screen_cmd_output("%s is aliased to %s%s",
				lalias->alias, lalias->orig,
				(lalias->args != nullptr ? lalias->args : ""));

			return;
		}
	}

	screen_err_msg("Error adding alias for %s", alias);
}

USER_COMMAND(cmd_auto) {
	struct pork_acct *acct = cur_window()->owner;
	char *target;

	if (args == nullptr || !acct->connected)
		return;

	target = strsep(&args, " ");
	if (target == nullptr || args == nullptr)
		return;

	pork_msg_autoreply(acct, target, args);
}

USER_COMMAND(cmd_away) {
	struct pork_acct *acct = cur_window()->owner;

	if (args == nullptr)
		pork_set_back(acct);
	else
		pork_set_away(acct, args);
}

USER_COMMAND(cmd_bind) {
	int key;
	char *func;
	char *key_str;
	struct key_binds *target_binds = cur_window()->active_binds;
	struct binding *binding;

	key_str = strsep(&args, " ");
	if (key_str == nullptr || blank_str(key_str)) {
		hash_iterate(&target_binds->hash, print_binding, nullptr);
		return;
	}

	if (key_str[0] == '-' && key_str[1] != '\0') {
		if (!strcasecmp(key_str, "-b") || !strcasecmp(key_str, "-buddy"))
			target_binds = &screen.binds.blist;
		else if (!strcasecmp(key_str, "-m") || !strcasecmp(key_str, "-main"))
			target_binds = &screen.binds.main;
		else {
			screen_err_msg("Bad bind flag: %s", key_str);
			return;
		}

		key_str = strsep(&args, " ");

		if (key_str == nullptr || blank_str(key_str)) {
			hash_iterate(&target_binds->hash, print_binding, nullptr);
			return;
		}
	}

	key = bind_get_keycode(key_str);
	if (key == -1) {
		screen_err_msg("Bad keycode: %s", key_str);
		return;
	}

	func = args;
	if (func != nullptr) {
		if (*func == opt_get_char(OPT_CMDCHARS) && *(func + 1) != '\0')
			func++;
		if (blank_str(func))
			func = nullptr;
	}

	if (func == nullptr) {
		binding = bind_find(target_binds, key);
		if (binding != nullptr)
			screen_cmd_output("%s is bound to %s", key_str, binding->binding);
		else
			screen_cmd_output("%s is not bound", key_str);

		return;
	}

	bind_add(target_binds, key, func);
	binding = bind_find(target_binds, key);
	if (binding != nullptr) {
		screen_cmd_output("%s is bound to %s", key_str, binding->binding);
		return;
	}

	screen_err_msg("Error binding %s", key_str);
}

USER_COMMAND(cmd_connect) {
	int protocol = PROTO_IRC;
	char *user;

	if (args == nullptr || blank_str(args))
		return;

	if (*args == '-') {
		char *p = strchr(++args, ' ');

		if (p != nullptr)
			*p++ = '\0';

		protocol = proto_get_num(args);
		if (protocol == -1) {
			screen_err_msg("Invalid protocol: %s", args);
			return;
		}

		args = p;
	}

	user = strsep(&args, " ");
	pork_acct_connect(user, args, protocol);
}

USER_COMMAND(cmd_ctcp) {
	struct pork_acct *acct = cur_window()->owner;
	char *dest;

	if (acct->proto->ctcp == nullptr)
		return;

	dest = strsep(&args, " ");
	if (dest == nullptr || args == nullptr)
		return;

	acct->proto->ctcp(acct, dest, args);
}

USER_COMMAND(cmd_echo) {
	if (args != nullptr)
		screen_win_msg(cur_window(), 0, 0, 1, MSG_TYPE_CMD_OUTPUT, args);
}

USER_COMMAND(cmd_disconnect) {
	struct pork_acct *acct = cur_window()->owner;
	u_int32_t dest;
	dlist_t *node;

	if (!acct->can_connect)
		return;

	if (args == nullptr || blank_str(args))
		dest = acct->refnum;
	else {
		char *refnum = strsep(&args, " ");

		if (str_to_uint(refnum, &dest) == -1) {
			screen_err_msg("Bad account refnum: %s", refnum);
			return;
		}

		if (args != nullptr && blank_str(args))
			args = nullptr;
	}

	acct = pork_acct_find(dest);
	if (acct == nullptr) {
		screen_err_msg("Account refnum %u is not logged in", dest);
		return;
	}

	if (!acct->can_connect) {
		screen_err_msg("You cannot sign %s off", acct->username);
		return;
	}

	pork_acct_del(acct, args);

	if (screen.status_win->owner == screen.null_acct)
		imwindow_bind_next_acct(screen.status_win);
}

USER_COMMAND(cmd_help) {
	char *section;

	if (args == nullptr) {
		char buf[8192];

		if (pork_help_get_cmds("main", buf, sizeof(buf)) != -1) {
			screen_cmd_output("Help for the following commands is available:");
			screen_win_msg(cur_window(), 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
				"\t%s", buf);
		} else
			screen_err_msg("Error: Can't find the help files");

		return;
	}

	section = strsep(&args, " ");
	if (section == nullptr) {
		screen_err_msg("Error: Can't find the help files");
		return;
	}

	if (args == nullptr) {
		char buf[8192];

		if (pork_help_print("main", section) == -1) {
			screen_err_msg("Help: Error: No such command or section: %s",
				section);
		} else {
			struct imwindow *win = cur_window();

			if (pork_help_get_cmds(section, buf, sizeof(buf)) != -1) {
				screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT, " ");
				strtoupper(section);
				screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
					"%%W%s COMMANDS", section);
				screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT, "\t%s", buf);
				screen_cmd_output("Type /help %s <command> for the help text for a particular %s command.", section, section);
			}
		}
	} else {
		if (pork_help_print(section, args) == -1) {
			screen_err_msg("Help: Error: No such command in section %s",
				section);
		}
	}
}

USER_COMMAND(cmd_idle) {
	u_int32_t idle_secs = 0;

	if (args != nullptr && !blank_str(args)) {
		if (str_to_uint(args, &idle_secs) != 0) {
			screen_err_msg("Invalid time specification: %s", args);
			return;
		}
	}

	pork_set_idle_time(cur_window()->owner, idle_secs);
}

USER_COMMAND(cmd_laddr) {
	if (args == nullptr) {
		char buf[2048];

		if (get_hostname(&local_addr, buf, sizeof(buf)) != 0)
			xstrncpy(buf, "0.0.0.0", sizeof(buf));

		screen_cmd_output("New connections will use the local address %s", buf);
		return;
	}

	if (get_addr(args, &local_addr) != 0) {
		screen_err_msg("Invalid local address: %s", args);
		return;
	}

	screen_cmd_output("New connections will use the local address %s", args);
}

USER_COMMAND(cmd_lastlog) {
	int opts = 0;

	if (args == nullptr)
		return;

	if (*args == '-') {
		if (args[1] == ' ' || args[1] == '\0')
			goto done;

		args++;
		if (*args == '-' && args[1] == ' ') {
			args += 2;
			goto done;
		}

		do {
			switch (*args) {
				case 'b':
					opts |= SWINDOW_FIND_BASIC;
					break;

				case 'i':
					opts |= SWINDOW_FIND_ICASE;
					break;
			}

			if (*++args == ' ') {
				args++;
				break;
			}
		} while (*args != '\0');
	}
done:

	if (*args != '\0')
		imwindow_buffer_find(cur_window(), args, opts);
}

USER_COMMAND(cmd_load) {
	int quiet;
	char buf[PATH_MAX];

	if (args == nullptr)
		return;

	quiet = screen_set_quiet(1);

	expand_path(args, buf, sizeof(buf));
	if (read_conf(buf) != 0)
		screen_err_msg("Error reading %s: %s", buf, strerror(errno));

	screen_set_quiet(quiet);
}

USER_COMMAND(cmd_lport) {
	if (args == nullptr) {
		screen_cmd_output("New connections will use local port %u",
			ntohs(local_port));
		return;
	}

	if (get_port(args, &local_port) != 0) {
		screen_err_msg("Error: Invalid local port: %s", args);
		return;
	}

	local_port = htons(local_port);
	screen_cmd_output("New connections will use local port %s", args);
}

USER_COMMAND(cmd_me) {
	struct imwindow *win = cur_window();

	if (args == nullptr)
		return;

	if (win->type == WIN_TYPE_PRIVMSG)
		pork_action_send(win->owner, cur_window()->target, args);
	else if (win->type == WIN_TYPE_CHAT) {
		struct chatroom *chat;

		chat = (chatroom *)win->data;
		if (chat != nullptr)
			chat_send_action(win->owner, chat, chat->title, args);
	}
}

USER_COMMAND(cmd_msg) {
	struct pork_acct *acct = cur_window()->owner;
	char *target;
	struct chatroom *chat;

	if (args == nullptr || !acct->connected)
		return;

	target = strsep(&args, " ");
	if (target == nullptr || args == nullptr)
		return;

	chat = chat_find(acct, target);
	if (chat != nullptr)
		chat_send_msg(acct, chat, target, args);
	else
		pork_msg_send(acct, target, args);
}

USER_COMMAND(cmd_mode) {
	struct pork_acct *acct = cur_window()->owner;

	if (args == nullptr || !acct->connected)
		return;

	if (acct->proto->mode != nullptr)
		acct->proto->mode(acct, args);
}

USER_COMMAND(cmd_query) {
	struct imwindow *imwindow = cur_window();

	if (args != nullptr && !blank_str(args)) {
		struct imwindow *conv_window;

		screen_make_query_window(imwindow->owner, args, &conv_window);
		screen_goto_window(conv_window->refnum);
	} else
		screen_close_window(imwindow);
}

USER_COMMAND(cmd_quit) {
	pork_exit(0, args, nullptr);
}

USER_COMMAND(cmd_refresh) {
	screen_refresh();
}

USER_COMMAND(cmd_save) {
	if (save_global_config() == 0)
		screen_cmd_output("Your configuration has been saved");
	else
		screen_err_msg("There was an error saving your configuration");
}

USER_COMMAND(cmd_send) {
	struct imwindow *imwindow = cur_window();
	struct pork_acct *acct = imwindow->owner;

	if (args == nullptr || !acct->connected)
		return;

	if (imwindow->type == WIN_TYPE_PRIVMSG)
		pork_msg_send(acct, imwindow->target, args);
	else if (imwindow->type == WIN_TYPE_CHAT) {
		struct chatroom *chat = (chatroom *)imwindow->data;

		if (chat == nullptr) {
			screen_err_msg("%s is not a member of %s",
				acct->username, imwindow->target);
		} else
			chat_send_msg(acct, chat, chat->title, args);
	} else if (imwindow->type == WIN_TYPE_STATUS) {
		chat_send_msg(acct, (chatroom *)imwindow->data, "main", args);
	}
}

USER_COMMAND(cmd_unbind) {
	char *binding;
	struct imwindow *imwindow = cur_window();
	struct key_binds *target_binds = imwindow->active_binds;
	int c;

	binding = strsep(&args, " ");
	if (binding == nullptr || blank_str(binding))
		return;

	if (binding[0] == '-' && binding[1] != '\0') {
		if (!strcasecmp(binding, "-b") || !strcasecmp(binding, "-buddy"))
			target_binds = &screen.binds.blist;
		else if (!strcasecmp(binding, "-m") || !strcasecmp(binding, "-main"))
			target_binds = &screen.binds.main;
		else {
			screen_err_msg("Bad unbind flag: %s", binding);
			return;
		}

		binding = strsep(&args, " ");

		if (binding == nullptr || blank_str(binding))
			return;
	}

	c = bind_get_keycode(binding);
	if (c == -1) {
		screen_err_msg("Bad keycode: %s", binding);
		return;
	}

	if (bind_remove(target_binds, c) == -1)
		screen_cmd_output("There is no binding for %s", binding);
	else
		screen_cmd_output("Binding for %s removed", binding);
}

USER_COMMAND(cmd_unalias) {
	if (args == nullptr)
		return;

	if (alias_remove(&screen.alias_hash, args) == -1)
		screen_cmd_output("No such alias: %s", args);
	else
		screen_cmd_output("Alias %s removed", args);
}

USER_COMMAND(cmd_nick) {
	if (args == nullptr || blank_str(args))
		return;

	pork_change_nick(cur_window()->owner, args);
}

USER_COMMAND(cmd_notice) {
	struct pork_acct *acct = cur_window()->owner;
	char *target;
	struct chatroom *chat;

	if (args == nullptr || !acct->connected)
		return;

	target = strsep(&args, " ");
	if (target == nullptr || args == nullptr)
		return;

	chat = chat_find(acct, target);
	if (chat != nullptr)
		chat_send_notice(acct, chat, target, args);
	else
		pork_notice_send(acct, target, args);
}

USER_COMMAND(cmd_whowas) {
	struct pork_acct *acct = cur_window()->owner;

	if (acct->proto->whowas != nullptr && args != nullptr)
		acct->proto->whowas(acct, args);
}

USER_COMMAND(cmd_ping) {
	struct imwindow *win = cur_window();
	struct pork_acct *acct = win->owner;

	if (acct->proto->ping != nullptr)
		acct->proto->ping(acct, args);
}

USER_COMMAND(cmd_profile) {
	pork_set_profile(cur_window()->owner, args);
}

USER_COMMAND(cmd_acct) {
	if (args != nullptr)
		run_one_command(args, CMDSET_ACCT);
	else
		run_one_command("list", CMDSET_ACCT);
}

USER_COMMAND(cmd_chat) {
	if (!cur_window()->owner->connected)
		return;

	if (args != nullptr)
		run_one_command(args, CMDSET_CHAT);
	else
		run_one_command("list", CMDSET_CHAT);
}

USER_COMMAND(cmd_win) {
	if (args != nullptr)
		run_one_command(args, CMDSET_WIN);
	else
		run_one_command("list", CMDSET_WIN);
}

USER_COMMAND(cmd_input) {
	if (args != nullptr)
		run_one_command(args, CMDSET_INPUT);
}

USER_COMMAND(cmd_history) {
	if (args != nullptr)
		run_one_command(args, CMDSET_HISTORY);
	else
		run_one_command("list", CMDSET_HISTORY);
}

USER_COMMAND(cmd_scroll) {
	if (args != nullptr)
		run_one_command(args, CMDSET_SCROLL);
}

USER_COMMAND(cmd_set) {
	char *variable;
	char *value;
	int opt;

	variable = strsep(&args, " ");
	if (variable == nullptr || blank_str(variable)) {
		opt_print();
		return;
	}

	strtoupper(variable);
	opt = opt_find(variable);
	if (opt == -1) {
		screen_err_msg("Unknown variable: %s", variable);
		return;
	}

	value = args;
	if (value == nullptr || blank_str(value)) {
		opt_print_var(opt, "is set to");
		return;
	}

	if (opt_set(opt, value) == -1) {
		screen_nocolor_msg("Bad argument for %s: %s", variable, value);
	} else {
		opt_print_var(opt, "set to");
	}
}

int run_command(char *str) {
	return (run_one_command(str, CMDSET_MAIN));
}

int run_mcommand(char *str) {
	int i = 0;
	char *copystr = xstrdup(str);
	char *cmdstr = copystr;
	char *curcmd;

	curcmd = strsep(&cmdstr, ";");
	if (curcmd == nullptr)
		i = run_one_command(cmdstr, CMDSET_MAIN);
	else {
		while (curcmd != nullptr && i != -1) {
			char cmdchars = opt_get_char(OPT_CMDCHARS);

			while (*curcmd == ' ')
				curcmd++;

			while (*curcmd == cmdchars)
				curcmd++;

			i = run_one_command(curcmd, CMDSET_MAIN);
			curcmd = strsep(&cmdstr, ";");
		}
	}

	free(copystr);
	return (i);
}

static int run_one_command(char *str, u_int32_t set) {
	char *cmd_str;
	struct command *cmd;

	if (set == CMDSET_MAIN) {
		int ret;
		char *alias_str;

		ret = alias_resolve(&screen.alias_hash, str, &alias_str);
		if (ret == 0)
			str = alias_str;
		else if (ret == 1) {
			screen_err_msg("The alias chain is too long");
			return (-1);
		}
	}

	cmd_str = strsep(&str, " \t");

	cmd = (struct command *)bsearch(cmd_str, command_set[set].set, command_set[set].elem,
			sizeof(struct command), cmd_compare);

	if (cmd == nullptr) {
		struct pork_proto *proto;

		if (set == CMDSET_MAIN && (proto = proto_get_name(cmd_str)) != nullptr) {
			cmd_str = strsep(&str, " \t");

			cmd = (struct command *)bsearch(cmd_str, proto->cmd, proto->num_cmds,
					sizeof(struct command), cmd_compare);

			if (cmd == nullptr)
				screen_err_msg("Unknown %s command: %s", proto->name, cmd_str);
		} else {
			screen_err_msg("Unknown %scommand: %s",
				command_set[set].type, cmd_str);
		}

		return (-1);
	}

	cmd->cmd(str);
	return (0);
}

static void print_binding(void *data, void *nothing __notused) {
	struct binding *binding = static_cast<struct binding *>(data);
	char key_name[32];

	bind_get_keyname(binding->key, key_name, sizeof(key_name));
	screen_cmd_output("%s is bound to %s", key_name, binding->binding);
}

static void print_alias(void *data, void *nothing __notused) {
	struct alias *alias = static_cast<struct alias *>(data);

	screen_cmd_output("%s is aliased to %s%s",
		alias->alias, alias->orig, (alias->args != nullptr ? alias->args : ""));
}

static int cmd_compare(const void *l, const void *r) {
	char *key = (char *) l;
	struct command *cmd = (struct command *) r;

	return (strcasecmp(key, cmd->name));
}

USER_COMMAND(cmd_input_find_next_cmd) {
	u_int32_t cur_pos;
	u_int32_t begin_completion_pos;
	u_int32_t word_begin = 0;
	u_int32_t end_word;
	size_t elements = 0;
	char *input_buf;
	struct input *input;
	struct command *cmd = nullptr;

	input = cur_window()->input;
	cur_pos = input->cur - input->prompt_len;
	input_buf = input_get_buf_str(input);

	if (input->begin_completion <= input->prompt_len ||
		input->begin_completion >= input->cur)
	{
		if (cur_pos == 0) {
			/*
			** If you complete from the very beginning of the line,
			** insert a '/' because we only complete commands.
			*/

			input_insert(input, '/');
			input_buf = input_get_buf_str(input);
			cur_pos++;
		} else
			input->begin_completion = input->cur;
	}

	begin_completion_pos = input->begin_completion - input->prompt_len;

	/* Only complete if the line is a command. */
	if (input_buf[0] != '/')
		return;

	end_word = strcspn(&input_buf[1], " \t") + 1;

	if (end_word < begin_completion_pos) {
		size_t i;

		for (i = 1 ; i < array_elem(command_set) ; i++) {
			if (!strncasecmp(command_set[i].type, &input_buf[1], end_word)) {
				elements = command_set[i].elem;
				cmd = command_set[i].set;
				word_begin = end_word;

				break;
			}
		}

		if (word_begin == 0) {
			struct pork_proto *proto = cur_window()->owner->proto;

			if (!strncasecmp(proto->name, &input_buf[1], end_word)) {
				elements = proto->num_cmds;
				cmd = proto->cmd;
				word_begin = end_word;
			} else
				return;
		}

		while (	input_buf[word_begin] == ' ' ||
				input_buf[word_begin] == '\t')
		{
			word_begin++;
		}
	} else {
		word_begin = 1;
		elements = command_set[CMDSET_MAIN].elem;
		cmd = command_set[CMDSET_MAIN].set;
	}

	if (word_begin > 0) {
		int chosen = -1;
		size_t i;

		for (i = 0 ; i < elements ; i++) {
			const struct command *cmd_ptr;

			cmd_ptr = &cmd[i];
			if (!strcmp(cmd_ptr->name, ""))
				continue;

			if (input->cur == input->begin_completion) {
				if (!strncasecmp(cmd_ptr->name,
						&input_buf[word_begin], cur_pos - word_begin))
				{
					/* Don't choose this one if it's already an exact match. */
					if (strlen(cmd_ptr->name) == cur_pos - word_begin)
						continue;

					/* Match */
					chosen = i;
					i = elements;
					continue;
				}
			} else {
				/*
				** We've _already_ matched one. Now get
				** the next in the list, if possible.
				*/

				if (!strncasecmp(cmd_ptr->name,
						&input_buf[word_begin], cur_pos - word_begin))
				{
					/*
					** We've found the one we've matched already. If there
					** will be another possible, it will be the very next
					** one in the list. Let's check that. If it doesn't
					** match, none will.
					*/

					i++;
					if (i < elements) {
						cmd_ptr = &cmd[i];

						if (!strncasecmp(cmd_ptr->name,
								&input_buf[word_begin],
								begin_completion_pos - word_begin))
						{
							chosen = i;
						}

						i = elements;
						continue;
					}
				}
			}
		}

		if (input->begin_completion != input->cur) {
			/*
			** Remove characters added by last match. In
			** other words, remove characters from input->cur
			** until input->begin_completion == input->cur.
			*/
			size_t num_to_remove;

			num_to_remove = input->cur - input->begin_completion;

			for (i = 0 ; i < num_to_remove ; i++)
				input_bkspace(input);
		}

		if (chosen != -1) {
			/* We've found a match. */
			const struct command *cmd_ptr;
			u_int16_t remember_begin = input->begin_completion;

			cmd_ptr = &cmd[chosen];

			input_insert_str(input,
				&cmd_ptr->name[begin_completion_pos - word_begin]);
			input->begin_completion = remember_begin;
		}
	}
}
