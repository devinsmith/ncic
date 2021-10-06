/*
** Based on pork_conf.c
** ncic_conf.c - pork's configuration parser.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
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
#include <pwd.h>
#include <cerrno>
#include <climits>
#include <sys/stat.h>
#include <sys/types.h>

#include "ncic.h"
#include "ncic_util.h"
#include "ncic_list.h"
#include "ncic_imwindow.h"
#include "ncic_acct.h"
#include "ncic_misc.h"
#include "ncic_bind.h"
#include "ncic_alias.h"
#include "ncic_screen_io.h"
#include "ncic_screen.h"
#include "ncic_command.h"
#include "ncic_conf.h"

static int pork_mkdir(const char *path) {
	struct stat st;

	if (stat(path, &st) != 0) {
		if (mkdir(path, 0700) != 0) {
			screen_err_msg("Error: mkdir %s: %s", path, strerror(errno));
			return (-1);
		}
	} else {
		if (!S_ISDIR(st.st_mode)) {
			screen_err_msg("Error: %s is not a directory", path);
			return (-1);
		}
	}

	return (0);
}

int read_conf(const char *path) {
	FILE *fp;
	char buf[4096];
	u_int32_t line = 0;

	fp = fopen(path, "r");
	if (fp == nullptr) {
		if (errno != -ENOENT)
			debug("fopen: %s: %s", path, strerror(errno));
		return (-1);
	}

	while (fgets(buf, sizeof(buf), fp) != nullptr) {
		char *p;

		++line;

		p = strchr(buf, '\n');
		if (p == nullptr) {
			debug("line %u too long", line);
			fclose(fp);
			return (-1);
		}

		*p = '\0';

		while ((p = strchr(p, '\t')) != nullptr)
			*p++ = ' ';

		p = buf;
		if (*p == '#')
			continue;

		if (*p == opt_get_char(OPT_CMDCHARS))
			p++;

		if (!blank_str(p))
			run_command(p);
	}

	fclose(fp);
	return (0);
}

static int read_acct_conf(struct pork_acct *acct, const char *filename) {
	FILE *fp;
	char buf[2048];
	int line = 0;

	fp = fopen(filename, "r");
	if (fp == nullptr) {
		if (errno != ENOENT) {
			debug("fopen: %s: %s", filename, strerror(errno));
			return (-1);
		}

		return (0);
	}

	while (fgets(buf, sizeof(buf), fp) != nullptr) {
		char *p;

		++line;

		p = strchr(buf, '\n');
		if (p == nullptr) {
			debug("line %u too long", line);
			fclose(fp);
			return (-1);
		}

		*p = '\0';

		while ((p = strchr(p, '\t')) != nullptr)
			*p++ = ' ';

		p = buf;
		if (*p == '#')
			continue;

		p = strchr(buf, ':');
		if (p == nullptr)
			continue;
		*p++ = '\0';

		while (*p == ' ')
			p++;

		if (!strcasecmp(buf, "username")) {
			free(acct->username);
			acct->username = xstrdup(p);
		} else if (!strcasecmp(buf, "password")) {
			if (acct->passwd != nullptr) {
				memset(acct->passwd, 0, strlen(acct->passwd));
				free(acct->passwd);
			}

			acct->passwd = xstrdup(p);
		} else if (!strcasecmp(buf, "profile")) {
			free(acct->profile);
			acct->profile = xstrdup(p);
		} else {
			screen_err_msg("Error: account config line %d: bad setting: %s",
				line, buf);
		}
	}

	fclose(fp);
	return (0);
}

int read_user_config(struct pork_acct *acct) {
	char nname[NUSER_LEN];
	char buf[PATH_MAX];
	char *pork_dir = opt_get_str(OPT_NCIC_DIR);

	if (acct == nullptr || pork_dir == nullptr)
		return (-1);

	normalize(nname, acct->username, sizeof(nname));

	snprintf(buf, sizeof(buf), "%s/%s", pork_dir, nname);
	if (pork_mkdir(buf) != 0)
		return (-1);

	snprintf(buf, sizeof(buf), "%s/%s/logs", pork_dir, nname);
	if (pork_mkdir(buf) != 0)
		return (-1);

	snprintf(buf, sizeof(buf), "%s/%s/ncicrc", pork_dir, nname);
	if (read_conf(buf) != 0 && errno != ENOENT)
		screen_err_msg("There was an error reading your ncicrc file");

	snprintf(buf, sizeof(buf), "%s/%s/account", pork_dir, nname);
	if (read_acct_conf(acct, buf) != 0)
		screen_err_msg("Error: Can't read account config file, %s", buf);

	return (0);
}

static int save_acct_conf(struct pork_acct *acct, char *filename) {
	char *fn;
	size_t len;
	FILE *fp;

	len = strlen(filename) + sizeof("-TEMP");
	fn = (char *)xmalloc(len);
	snprintf(fn, len, "%s-TEMP", filename);

	fp = fopen(fn, "w");
	if (fp == nullptr) {
		debug("fopen: %s: %s", fn, strerror(errno));
		free(fn);
		return (-1);
	}

	if (acct->username != nullptr)
		fprintf(fp, "username: %s\n", acct->username);
	if (acct->profile != nullptr)
		fprintf(fp, "profile: %s\n", acct->profile);
	if (opt_get_bool(OPT_SAVE_PASSWD) && acct->passwd != nullptr)
		fprintf(fp, "password: %s\n", acct->passwd);

	fchmod(fileno(fp), 0600);
	fclose(fp);

	if (rename(fn, filename) != 0) {
		debug("rename: %s<=>%s: %s", fn, filename, strerror(errno));
		unlink(fn);
		free(fn);
		return (-1);
	}

	free(fn);
	return (0);
}

int save_user_config(struct pork_acct *acct) {
	char nname[NUSER_LEN];
	char buf[PATH_MAX];
	char *pork_dir = opt_get_str(OPT_NCIC_DIR);

	if (acct == nullptr || pork_dir == nullptr) {
		debug("acct=%p port_dir=%p", acct, pork_dir);
		return (-1);
	}

	normalize(nname, acct->username, sizeof(nname));

	snprintf(buf, sizeof(buf), "%s/%s/buddy_list", pork_dir, nname);

	snprintf(buf, sizeof(buf), "%s/%s/account", pork_dir, nname);
	if (save_acct_conf(acct, buf) != 0)
		screen_err_msg("Error: Can't write account config file, %s.", buf);

	return (0);
}

int read_global_config(void) {
	struct passwd *pw;
	char *pork_dir;
	char buf[PATH_MAX];

	if (read_conf(SYSTEM_NCICRC) != 0) {
    screen_err_msg("Error reading the system-wide ncicrc file from %s", SYSTEM_NCICRC);
  }

  pw = getpwuid(getuid());
	if (pw == nullptr) {
		debug("getpwuid: %s", strerror(errno));
		return (-1);
	}

	pork_dir = opt_get_str(OPT_NCIC_DIR);
	if (pork_dir == nullptr) {
		snprintf(buf, sizeof(buf), "%s/.ncic", pw->pw_dir);
		opt_set(OPT_NCIC_DIR, buf);

		pork_dir = opt_get_str(OPT_NCIC_DIR);
	} else
		xstrncpy(buf, pork_dir, sizeof(buf));

	if (pork_mkdir(buf) != 0)
		return (-1);

	pork_dir = opt_get_str(OPT_NCIC_DIR);

	snprintf(buf, sizeof(buf), "%s/ncicrc", pork_dir);
	if (read_conf(buf) != 0 && errno != ENOENT)
		return (-1);

	return (0);
}

static void write_alias_line(void *data, void *filep) {
	struct alias *alias = static_cast<struct alias *>(data);
	FILE *fp = static_cast<FILE *>(filep);

	fprintf(fp, "alias %s %s%s\n",
		alias->alias, alias->orig, (alias->args ? alias->args : ""));
}

static void write_bind_line(void *data, void *filep) {
	struct binding *binding = static_cast<struct binding *>(data);
  FILE *fp = static_cast<FILE *>(filep);
	char key_name[32];

	bind_get_keyname(binding->key, key_name, sizeof(key_name));
	fprintf(fp, "bind %s %s\n", key_name, binding->binding);
}

static void write_bind_blist_line(void *data, void *filep) {
  struct binding *binding = static_cast<struct binding *>(data);
  FILE *fp = static_cast<FILE *>(filep);
	char key_name[32];

	bind_get_keyname(binding->key, key_name, sizeof(key_name));
	fprintf(fp, "bind -buddy %s %s\n", key_name, binding->binding);
}

int save_global_config(void) {
	char porkrc[PATH_MAX];
	char *fn;
	size_t len;
	FILE *fp;
	char *pork_dir = opt_get_str(OPT_NCIC_DIR);

	if (pork_dir == nullptr)
		return (-1);

	snprintf(porkrc, sizeof(porkrc), "%s/ncicrc", pork_dir);

	len = strlen(porkrc) + sizeof("-TEMP");
	fn = (char *)xmalloc(len);
	snprintf(fn, len, "%s-TEMP", porkrc);

	fp = fopen(fn, "w");
	if (fp == nullptr) {
		debug("fopen: %s: %s", fn, strerror(errno));
		return (-1);
	}

	opt_write(fp);
	fprintf(fp, "\n");
	hash_iterate(&screen.alias_hash, write_alias_line, fp);
	fprintf(fp, "\n");
	hash_iterate(&screen.binds.main.hash, write_bind_line, fp);
	fprintf(fp, "\n");
	hash_iterate(&screen.binds.blist.hash, write_bind_blist_line, fp);

	fclose(fp);

	if (rename(fn, porkrc) != 0) {
		debug("rename: %s<=>%s: %s", fn, porkrc, strerror(errno));
		unlink(fn);
		free(fn);
		return (-1);
	}

	free(fn);
	return (0);
}
