/*
** Based on pork_help.c
** ncic_help.c - /help command implementation.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include "config.h"

#include <unistd.h>
#include <ncurses.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>

#include "ncic_util.h"
#include "ncic_screen.h"
#include "ncic_imsg.h"
#include "ncic_screen_io.h"
#include "ncic_help.h"

static int pork_help_is_section(char *string) {
	char path[4096];
	struct stat st;

	snprintf(path, sizeof(path), "%s/%s", NCIC_HELP_PATH, string);

	if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
		return (0);

	return (1);
}

int pork_help_get_cmds(const char *section, char *buf, size_t len) {
	DIR *dir;
	struct dirent *de;
	char path[4096];
	char cwd[4096];
	int i = 0;
	int ret;

	if (section == nullptr)
		section = "main";

	ret = snprintf(path, sizeof(path), "%s/%s", NCIC_HELP_PATH, section);
	if (ret < 0 || (size_t) ret >= sizeof(path))
		return (-1);

	dir = opendir(path);
	if (dir == nullptr)
		return (-1);

	if (getcwd(cwd, sizeof(cwd)) == nullptr)
		goto out_fail;

	if (chdir(path) != 0)
		goto out_fail;

	while (len > 0 && (de = readdir(dir)) != nullptr) {
		struct stat st;

		ret = stat(de->d_name, &st);
		if (ret != 0 || !S_ISREG(st.st_mode))
			continue;

		ret = snprintf(&buf[i], len, "%s%s%%x ",
				(pork_help_is_section(de->d_name) ? HELP_SECTION_STYLE : ""),
				de->d_name);

		if (ret < 0 || (size_t) ret >= len)
			goto out_fail2;

		len -= ret;
		i += ret;
	}

	if (i > 0)
		buf[i - 1] = '\0';

	closedir(dir);
	return (0);

out_fail2:
	chdir(cwd);
out_fail:
	closedir(dir);
	return (-1);
}

int pork_help_print(const char *section, char *command) {
	FILE *fp;
	char buf[8192];
	int ret;

	if (command == nullptr)
		return (-1);

	if (section == nullptr)
		section = "main";

	ret = snprintf(buf, sizeof(buf), "%s/%s/%s", NCIC_HELP_PATH, section, command);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	fp = fopen(buf, "r");
	if (fp == nullptr)
		return (-1);

	while (fgets(buf, sizeof(buf), fp) != nullptr) {
		char *p;
		char outbuf[8192];
		size_t len = sizeof(outbuf) - 1;
		char *out = outbuf;

		*out = '\0';

		p = strchr(buf, '\n');
		if (p == nullptr)
			goto out_fail;
		*p = '\0';

		p = buf;
		while (len > 0 && *p != '\0') {
			if (p == buf && isupper(*p) && isupper(*(p + 1))) {
				/*
				** This is a section heading.
				*/
				int ret;

				ret = xstrncpy(out, HELP_HEADER_STYLE, len);
				if (ret == -1)
					goto out_fail;

				out += ret;
				len -= ret;
			}

			if (*p == '\t') {
				int i;

				for (i = 0 ; i < HELP_TABSTOP ; i++) {
					*out++ = ' ';
					len--;

					if (len <= 0)
						goto out_fail;
				}
			} else if (isprint(*p)) {
				*out++ = *p;
				len--;
			}

			p++;
		}

		if (len <= 0)
			goto out_fail;

		*out = '\0';
		screen_win_msg(cur_window(), 0, 0, 1,
			MSG_TYPE_CMD_OUTPUT, "%s", outbuf);
	}

	fclose(fp);
	return (0);

out_fail:
	fclose(fp);
	return (-1);
}
