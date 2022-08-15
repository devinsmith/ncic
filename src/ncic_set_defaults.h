/*
** Based on pork_set_defaults.h
** ncic_set_defaults.h - /SET command implementation.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __NCIC_SET_DEFAULTS_H__
#define __NCIC_SET_DEFAULTS_H__

/*
** Default values for the global options.
*/

#define DEFAULT_ACTIVITY_TYPES				0xffffffff
#define DEFAULT_AUTO_RECONNECT				0
#define DEFAULT_BANNER						"*** "
#define DEFAULT_BEEP						0
#define DEFAULT_BEEP_MAX					3
#define DEFAULT_BEEP_ON_OUTPUT				0
#define DEFAULT_CMDCHARS					'/'
#define DEFAULT_CONNECT_TIMEOUT				180
#define DEFAULT_DOWNLOAD_DIR				""
#define DEFAULT_DUMP_MSGS_TO_STATUS			0
#define DEFAULT_FORMAT_ACTION_RECV			"[$T] %c* %W$N%x $M"
#define DEFAULT_FORMAT_ACTION_RECV_STATUS	"[$T] %C>%c* %W$N%D(%C$h%D)%x $M"
#define DEFAULT_FORMAT_ACTION_SEND			"[$T] %c* %W$N%x $M"
#define DEFAULT_FORMAT_ACTION_SEND_STATUS	"[$T] %D-> %m$R %c* %W$N%x $M"
#define DEFAULT_FORMAT_CHAT_CREATE			"[$T] %W$N%x has %Gcreated %c$R %D(%x$U%D)"
#define DEFAULT_FORMAT_CHAT_IGNORE			"[$T] %W$N%x has %Gignored%x $D in %c$R"
#define DEFAULT_FORMAT_CHAT_INVITE			"[$T] %W$N%x has %Ginvited%x $D to %c$R%x %D(%x$M%D)"
#define DEFAULT_FORMAT_CHAT_JOIN			"[%YJ%D/%x$T%D/%C$N%D/%c$H%x]"
#define DEFAULT_FORMAT_CHAT_KICK			"[%RL%D/%r$D%D/%c$N%D/%c$M%x]"
#define DEFAULT_FORMAT_CHAT_LEAVE			"[%rL%D/%x$T%D/%c$N%D/%c$H%x]"
#define DEFAULT_FORMAT_CHAT_MODE			"[%MM%D/%m$T%D/%W$N%D/%C$M%x]"
#define DEFAULT_FORMAT_CHAT_NICK			"[%YN%D/%x$T%D/%m$N%W ->%M $D%x]"
#define DEFAULT_FORMAT_CHAT_QUIT			"[%BQ%D/%x$T%D/%B$N%D/%b$H%D/%x$M%x]"
#define DEFAULT_FORMAT_CHAT_RECV			"[$T] %b<%x$N%b>%x $M"
#define DEFAULT_FORMAT_CHAT_RECV_ACTION		"[$T] %c* %W$N%x $M"
#define DEFAULT_FORMAT_CHAT_RECV_NOTICE		"[$T] %D-%B$N%Y:%b$C%D-%x $M"
#define DEFAULT_FORMAT_CHAT_SEND			"[$T] $C%m>%x $M"
#define DEFAULT_FORMAT_CHAT_SEND_ACTION		"[$T] %c* %W$N%x $M"
#define DEFAULT_FORMAT_CHAT_SEND_NOTICE		"[$T] %D-> -%c$N%Y:%b$C%D-%x $M"
#define DEFAULT_FORMAT_CHAT_TOPIC			"[%GT%D/%g$T%D/%W$N%D/%x$M%x]"
#define DEFAULT_FORMAT_CHAT_UNIGNORE		"%W$N%x has %Gunignored%x $D in %c$R"
#define DEFAULT_FORMAT_HIGHLIGHT      "[$I]%Y$N%x: $M"
#define DEFAULT_FORMAT_IM_RECV				"[$T] %m<%x$N%m>%x $M"
#define DEFAULT_FORMAT_IM_RECV_AUTO			"[$T] %Ca%cuto%D-%Cr%cesponse%x from %W$N%D:%x $M"
#define DEFAULT_FORMAT_IM_RECV_STATUS		"[$T] %D*%M$N%D(%m$h%D)*%x $M"
#define DEFAULT_FORMAT_IM_SEND				"[$T] %c>%x $M"
#define DEFAULT_FORMAT_IM_SEND_AUTO			"[$T] %Ca%cuto%D-%Cr%cesponse%x to %W$R%D:%x $M"
#define DEFAULT_FORMAT_IM_SEND_STATUS		"[$T] %D->*%m$R%D*%x $M"
#define DEFAULT_FORMAT_NOTICE_RECV			"[$T] %D-%B$N%D-%x $M"
#define DEFAULT_FORMAT_NOTICE_RECV_STATUS	"[$T] %D-%B$N%D(%c$h%D)-%x $M"
#define DEFAULT_FORMAT_NOTICE_SEND			"[$T] %D-> -%c$R%D-%x $M"
#define DEFAULT_FORMAT_NOTICE_SEND_STATUS	"[$T] %D-> -%c$R%D-%x $M"
#define DEFAULT_FORMAT_STATUS				"%d,w$T$n [$z$c]$A$H $>$I%d,w$S [$!]"
#define DEFAULT_FORMAT_STATUS_ACTIVITY		" %w,d{$A}%d,w"
#define DEFAULT_FORMAT_STATUS_CHAT			"%d,w$T$@$n (+$u) [$z$c (+$M)]$A$H $>$I$W%d,w$S [$!]"
#define DEFAULT_FORMAT_STATUS_HELD			" <%g,w$H%d,w>"
#define DEFAULT_FORMAT_STATUS_IDLE			"%d,w (%D,widle: $i%d,w)"
#define DEFAULT_FORMAT_STATUS_TIMESTAMP		"[$H:$M] "
#define DEFAULT_FORMAT_STATUS_TYPING		" (%b,w$Y%d,w)"
#define DEFAULT_FORMAT_SYSTEM_ALERT		"%R$M"
#define DEFAULT_FORMAT_TIMESTAMP			"$H:$M"
#define DEFAULT_FORMAT_WARN					"[$T] %R$N%r has been warned by %R$U%r.%x Warning level is now %W$W%%"
#define DEFAULT_FORMAT_WHOIS_AWAY			"%D-%Ca%cway message%W:%x $A"
#define DEFAULT_FORMAT_WHOIS_IDLE			"%D-%p-%P--%Ci%cdle time%W:%x $i"
#define DEFAULT_FORMAT_WHOIS_MEMBER			"%D-%Cm%cember since%W:%x $d"
#define DEFAULT_FORMAT_WHOIS_NAME			"%D-%p-%P---%Cu%csername%W:%x $N"
#define DEFAULT_FORMAT_WHOIS_SIGNON			"%D-%Co%cnline since%W:%x $s"
#define DEFAULT_FORMAT_WHOIS_USERINFO		"%D-%p-%P--%Cu%cser info%W:%x $P"
#define DEFAULT_FORMAT_WHOIS_WARNLEVEL		"%D-%p-%P-%Cw%carn level%W:%x $W%%"
#define DEFAULT_HISTORY_LEN					400
#define DEFAULT_IDLE_AFTER					10
#define DEFAULT_LOG							0
#define DEFAULT_LOG_TYPES					0xffffffff
#define DEFAULT_LOGIN_ON_STARTUP			1
#define DEFAULT_OUTGOING_MSG_FONT			""
#define DEFAULT_OUTGOING_MSG_FONT_BGCOLOR	"#ffffff"
#define DEFAULT_OUTGOING_MSG_FONT_FGCOLOR	"#000000"
#define DEFAULT_OUTGOING_MSG_FONT_SIZE		""
#define DEFAULT_NCIC_DIR					NULL
#define DEFAULT_PRIVATE_INPUT				0
#define DEFAULT_PROMPT						"%Mn%mcic%w>%x "
#define DEFAULT_RECONNECT_INTERVAL			15
#define DEFAULT_RECONNECT_MAX_INTERVAL		600
#define DEFAULT_RECONNECT_TRIES				10
#define DEFAULT_RECURSIVE_EVENTS			0
#define DEFAULT_REPORT_IDLE					1
#define DEFAULT_SAVE_PASSWD					0
#define DEFAULT_SCROLL_ON_INPUT				1
#define DEFAULT_SCROLL_ON_OUTPUT			0
#define DEFAULT_SCROLLBUF_LEN				5000
#define DEFAULT_SEND_REMOVES_AWAY			1
#define DEFAULT_SHOW_BUDDY_AWAY				1
#define DEFAULT_SHOW_BUDDY_IDLE				0
#define DEFAULT_SHOW_BUDDY_SIGNOFF			1
#define DEFAULT_TEXT_NO_NAME				"<not specified>"
#define DEFAULT_TEXT_NO_ROOM				":(not joined)"
#define DEFAULT_TEXT_TYPING					"Typing"
#define DEFAULT_TEXT_TYPING_PAUSED			"Typing [paused]"
#define DEFAULT_TEXT_WARN_ANONYMOUS			"%D<%ranonymous%D>%x"
#define DEFAULT_TIMESTAMP					1
#define DEFAULT_TRANSFER_PORT_MAX			15000
#define DEFAULT_TRANSFER_PORT_MIN			10000
#define DEFAULT_WORDWRAP					1
#define DEFAULT_WORDWRAP_CHAR				' '

#endif /* __NCIC_SET_DEFAULTS_H__ */
