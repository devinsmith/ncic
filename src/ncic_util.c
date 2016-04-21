/*
** Heavily based on pork_util.c
** ncic_util.c - utility functions
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

//#include <config.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ncic.h"
//#include <pork_missing.h>
#include "ncic_util.h"

/*
** Convert all the alphabetic characters in the string to uppercase. This
** modifies the string.
*/

void strtoupper(char *s) {
	if (s == NULL)
		return;

	for (; *s != '\0' ; s++)
		*s = toupper(*s);
}

void *xrealloc(void *ptr, size_t size) {
	void *ret = realloc(ptr, size);

	if (ret == NULL) {
		debug("out of memory: %u", size);
		exit(-1);
	}

	return (ret);
}

void *xmalloc(size_t len) {
	void *ret = malloc(len);

	if (ret == NULL) {
		debug("out of memory: %u", len);
		exit(-1);
	}

	return (ret);
}

void *xcalloc(size_t nmemb, size_t len) {
	void *ret = calloc(nmemb, len);

	if (ret == NULL) {
		debug("out of memory: %u", len);
		exit(-1);
	}

	return (ret);
}

char *xstrdup(const char *str) {
	char *ret = strdup(str);

	if (ret == NULL) {
		debug("out of memory: %p", str);
		exit(-1);
	}

	return (ret);
}

void free_str_wipe(char *str) {
	if (str == NULL)
		return;

	memset(str, 0, strlen(str));
	free(str);
}

/*
** Copy at most n-1 characters from src to dest and nul-terminate dest.
** Returns the number of characters copied (excluding the '\0') if the whole
** string was copied, -1 otherwise.
*/

int xstrncpy(char *dest, const char *src, size_t n) {
	size_t len;
	int ret;

	if (n == 0)
		return (0);

	len = strlen(src);
	if (len >= n) {
		len = n - 1;
		ret = -1;
		debug("string too long src=%s", src);
	} else
		ret = (int) len;

	memcpy(dest, src, len);
	dest[len] = '\0';
	return (ret);
}

/*
** Append at most n-1 characters from src to dest and nul-terminate dest.
** Returns the number of characters copied (excluding the '\0') if the whole
** string was copied, -1 otherwise.
*/

int xstrncat(char *dest, const char *src, size_t n) {
	if (n == 0)
		return (0);

	for (; *dest != '\0' && n > 0 ; dest++, n--)
		;

	if (n == 0) {
		debug("src too long: %s", src);
		return (-1);
	}

	return (xstrncpy(dest, src, n));
}

char *xstrndup(const char *str, size_t len) {
	char *dst;
	char *ret;

	if (len == 0)
		return (NULL);

	dst = xmalloc(len + 1);
	ret = dst;

	while (*str != '\0' && len > 0) {
		*dst++ = *str++;
		len--;
	}

	if (dst[-1] != '\0')
		*dst = '\0';

	return (ret);
}

/*
** Returns 1 if the string is blank (i.e. contains
** nothing but space characters), 0 if it's not blank.
*/

int blank_str(const char *str) {
	const char *p = str;

	while (*p != '\0') {
		if (*p != ' ')
			return (0);
		p++;
	}

	return (1);
}

/*
** Returns the portion of the string starting with token number "tok_num".
** Returns NULL if there are less tokens than "tok_num" in the string.
*/

char *str_from_tok(char *str, uint32_t tok_num) {
	char *p;
	uint32_t i;

	if (tok_num <= 1)
		return (str);

	p = str;
	for (i = 0 ; i < tok_num - 1 && (p = strchr(p, ' ')) != NULL ; i++)
		p++;

	if (i != tok_num - 1 || p == NULL)
		return (NULL);

	return (p);
}

/*
** Trims any trailing whitespace characters from the string.
*/

void str_trim(char *str) {
	char *p;
	size_t len;

	len = strlen(str);
	p = &str[len - 1];

	while (*p == ' ' || *p == '\t')
		*p-- = '\0';
}

/*
** Hash by Karl Nelson <kenelson@ece.ucdavis.edu>
**
** See http://mail.gnome.org/archives/gtk-devel-list/2000-February/msg00057.html
** for details.
*/

uint32_t string_hash(const char *str, uint32_t order) {
	uint32_t hash = 0;

	while (*str != '\0')
		hash = (hash << 5) - hash + *str++;

	return (hash & ((1 << order) - 1));
}

uint32_t int_hash(int num, uint32_t order) {
	return (num & ((1 << order) - 1));
}

int str_to_uint(const char *str, uint32_t *val) {
	char *end;
	uint32_t temp;

	temp = strtoul(str, &end, 0);
	if (*end != '\0')
		return (-1);

	*val = temp;
	return (0);
}

int str_to_int(const char *str, int *val) {
	char *end;
	int temp;

	temp = strtol(str, &end, 0);
	if (*end != '\0')
		return (-1);

	*val = temp;
	return (0);
}

char *terminate_quote(char *buf) {
	char *p = buf;

	if (*p == '\"')
		p++;

	while ((p = strchr(p, '\"')) != NULL) {
		if (p[-1] != '\\') {
			*p++ = '\0';
			return (p);
		}
		memmove(&p[-1], p, strlen(p) + 1);
	}

	return (NULL);
}

int expand_path(char *path, char *dest, size_t len) {
#ifdef WIN32
	return (0);
#else
	int ret;

	if (*path == '~') {
		struct passwd *pw;

		path++;

		if (*path == '/') {
			pw = getpwuid(getuid());
			path++;
		} else {
			char *p = strchr(path, '/');

			if (p == NULL) {
				if (*path == '\0')
					pw = getpwuid(getuid());
				else
					pw = getpwnam(path);

				if (pw != NULL)
					path = NULL;
			} else {
				char *user = xstrndup(path, p - path);
				pw = getpwnam(user);
				free(user);

				if (pw != NULL)
					path = ++p;
			}
		}

		if (pw != NULL) {
			if (path != NULL) {
				ret = snprintf(dest, len, "%s/%s", pw->pw_dir, path);
				if (ret < 1 || (size_t) ret >= len)
					ret = -1;
			} else
				ret = xstrncpy(dest, pw->pw_dir, len);
		} else {
			path--;
			ret = xstrncpy(dest, path, len);
		}
	} else
		ret = xstrncpy(dest, path, len);

	return (ret);
#endif
}

int file_get_size(FILE *fp, size_t *result) {
	struct stat st;

	if (fstat(fileno(fp), &st) != 0) {
		debug("fstat: %s", strerror(errno));
		return (-1);
	}

	*result = st.st_size;
	return (0);
}
