/*
** Based on pork_command_defs.h
** ncic_command_defs.h - static declarations needed in ncic_command.c
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** I moved this stuff here so that the ncic_command.c file might
** be slightly less cluttered.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_COMMAND_DEFS_H__
#define __NCIC_COMMAND_DEFS_H__

USER_COMMAND(cmd_acct);
USER_COMMAND(cmd_alias);
USER_COMMAND(cmd_auto);
USER_COMMAND(cmd_away);
USER_COMMAND(cmd_bind);
USER_COMMAND(cmd_compose);
USER_COMMAND(cmd_connect);
USER_COMMAND(cmd_ctcp);
USER_COMMAND(cmd_disconnect);
USER_COMMAND(cmd_echo);
USER_COMMAND(cmd_eval);
USER_COMMAND(cmd_help);
USER_COMMAND(cmd_idle);
USER_COMMAND(cmd_laddr);
USER_COMMAND(cmd_lastlog);
USER_COMMAND(cmd_load);
USER_COMMAND(cmd_lport);
USER_COMMAND(cmd_me);
USER_COMMAND(cmd_mode);
USER_COMMAND(cmd_msg);
USER_COMMAND(cmd_nick);
USER_COMMAND(cmd_notice);
USER_COMMAND(cmd_perl);
USER_COMMAND(cmd_perl_dump);
USER_COMMAND(cmd_perl_load);
USER_COMMAND(cmd_ping);
USER_COMMAND(cmd_profile);
USER_COMMAND(cmd_query);
USER_COMMAND(cmd_quit);
USER_COMMAND(cmd_refresh);
USER_COMMAND(cmd_save);
USER_COMMAND(cmd_send);
USER_COMMAND(cmd_set);
USER_COMMAND(cmd_unbind);
USER_COMMAND(cmd_unalias);
USER_COMMAND(cmd_whowas);

USER_COMMAND(cmd_acct_save);
USER_COMMAND(cmd_acct_set);

USER_COMMAND(cmd_file);
USER_COMMAND(cmd_file_cancel);
USER_COMMAND(cmd_file_list);
USER_COMMAND(cmd_file_get);
USER_COMMAND(cmd_file_resume);
USER_COMMAND(cmd_file_send);

USER_COMMAND(cmd_win);
USER_COMMAND(cmd_win_bind);
USER_COMMAND(cmd_win_bind_next);
USER_COMMAND(cmd_win_clear);
USER_COMMAND(cmd_win_close);
USER_COMMAND(cmd_win_dump);
USER_COMMAND(cmd_win_erase);
USER_COMMAND(cmd_win_ignore);
USER_COMMAND(cmd_win_list);
USER_COMMAND(cmd_win_next);
USER_COMMAND(cmd_win_prev);
USER_COMMAND(cmd_win_rename);
USER_COMMAND(cmd_win_renumber);
USER_COMMAND(cmd_win_set);
USER_COMMAND(cmd_win_skip);
USER_COMMAND(cmd_win_swap);
USER_COMMAND(cmd_win_unignore);
USER_COMMAND(cmd_win_unskip);

USER_COMMAND(cmd_scroll);
USER_COMMAND(cmd_scroll_by);
USER_COMMAND(cmd_scroll_down);
USER_COMMAND(cmd_scroll_end);
USER_COMMAND(cmd_scroll_pgdown);
USER_COMMAND(cmd_scroll_pgup);
USER_COMMAND(cmd_scroll_start);
USER_COMMAND(cmd_scroll_up);

USER_COMMAND(cmd_input);
USER_COMMAND(cmd_input_bkspace);
USER_COMMAND(cmd_input_clear);
USER_COMMAND(cmd_input_clear_prev);
USER_COMMAND(cmd_input_clear_next);
USER_COMMAND(cmd_input_clear_to_end);
USER_COMMAND(cmd_input_clear_to_start);
USER_COMMAND(cmd_input_delete);
USER_COMMAND(cmd_input_end);
USER_COMMAND(cmd_input_find_next_cmd);
USER_COMMAND(cmd_input_focus_next);
USER_COMMAND(cmd_input_insert);
USER_COMMAND(cmd_input_left);
USER_COMMAND(cmd_input_prev_word);
USER_COMMAND(cmd_input_prompt);
USER_COMMAND(cmd_input_next_word);
USER_COMMAND(cmd_input_right);
USER_COMMAND(cmd_input_send);
USER_COMMAND(cmd_input_start);

USER_COMMAND(cmd_history);
USER_COMMAND(cmd_history_clear);
USER_COMMAND(cmd_history_list);
USER_COMMAND(cmd_history_next);
USER_COMMAND(cmd_history_prev);

USER_COMMAND(cmd_timer);
USER_COMMAND(cmd_timer_add);
USER_COMMAND(cmd_timer_del);
USER_COMMAND(cmd_timer_del_refnum);
USER_COMMAND(cmd_timer_list);
USER_COMMAND(cmd_timer_purge);

USER_COMMAND(cmd_chat);
USER_COMMAND(cmd_chat_ban);
USER_COMMAND(cmd_chat_ignore);
USER_COMMAND(cmd_chat_invite);
USER_COMMAND(cmd_chat_join);
USER_COMMAND(cmd_chat_leave);
USER_COMMAND(cmd_chat_list);
USER_COMMAND(cmd_chat_send);
USER_COMMAND(cmd_chat_topic);
USER_COMMAND(cmd_chat_unignore);

USER_COMMAND(cmd_protocol);

#endif /* __NCIC_COMMAND_DEFS_H__ */
