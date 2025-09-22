/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/statvfs.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "dhcpcd.h"
#include "if-options.h"

const char *
hwaddr_ntoa(const void *hwaddr, size_t hwlen, char *buf, size_t buflen)
{
	const unsigned char *hp, *ep;
	char *p;

	/* Allow a hwlen of 0 to be an empty string. */
	if (buf == NULL || buflen == 0) {
		errno = ENOBUFS;
		return NULL;
	}

	if (hwlen * 3 > buflen) {
		/* We should still terminate the string just in case. */
		buf[0] = '\0';
		errno = ENOBUFS;
		return NULL;
	}

	hp = hwaddr;
	ep = hp + hwlen;
	p = buf;
	while (hp < ep) {
		if (hp != hwaddr)
			*p ++= ':';
		p += snprintf(p, 3, "%.2x", *hp++);
	}
	*p ++= '\0';
	return buf;
}

size_t
hwaddr_aton(uint8_t *buffer, const char *addr)
{
	char c[3];
	const char *p = addr;
	uint8_t *bp = buffer;
	size_t len = 0;

	c[2] = '\0';
	while (*p != '\0') {
		/* Skip separators */
		c[0] = *p++;
		switch (c[0]) {
		case '\n':	/* long duid split on lines */
		case ':':	/* typical mac address */
		case '-':	/* uuid */
			continue;
		}
		c[1] = *p++;
		/* Ensure that digits are hex */
		if (isxdigit((unsigned char)c[0]) == 0 ||
		    isxdigit((unsigned char)c[1]) == 0)
		{
			errno = EINVAL;
			return 0;
		}
		/* We should have at least two entries 00:01 */
		if (len == 0 && *p == '\0') {
			errno = EINVAL;
			return 0;
		}
		if (bp)
			*bp++ = (uint8_t)strtol(c, NULL, 16);
		len++;
	}
	return len;
}

ssize_t
readfile(const char *file, void *data, size_t len)
{
	int fd;
	ssize_t bytes;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;
	bytes = read(fd, data, len);
	close(fd);
	if ((size_t)bytes == len) {
		errno = ENOBUFS;
		return -1;
	}
	return bytes;
}

ssize_t
writefile(const char *file, mode_t mode, const void *data, size_t len)
{
	int fd;
	ssize_t bytes;

	fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (fd == -1)
		return -1;
	bytes = write(fd, data, len);
	close(fd);
	return bytes;
}

int
filemtime(const char *file, time_t *time)
{
	struct stat st;

	if (stat(file, &st) == -1)
		return -1;
	*time = st.st_mtime;
	return 0;
}

/* Handy routine to read very long lines in text files.
 * This means we read the whole line and avoid any nasty buffer overflows.
 * We strip leading space and avoid comment lines, making the code that calls
 * us smaller. */
char *
get_line(char ** __restrict buf, ssize_t * __restrict buflen)
{
	char *p, *c;
	bool quoted;

	do {
		p = *buf;
		if (*buf == NULL)
			return NULL;
		c = memchr(*buf, '\n', (size_t)*buflen);
		if (c == NULL) {
			c = memchr(*buf, '\0', (size_t)*buflen);
			if (c == NULL)
				return NULL;
			*buflen = c - *buf;
			*buf = NULL;
		} else {
			*c++ = '\0';
			*buflen -= c - *buf;
			*buf = c;
		}
		for (; *p == ' ' || *p == '\t'; p++)
			;
	} while (*p == '\0' || *p == '\n' || *p == '#' || *p == ';');

	/* Strip embedded comments unless in a quoted string or escaped */
	quoted = false;
	for (c = p; *c != '\0'; c++) {
		if (*c == '\\') {
			c++; /* escaped */
			continue;
		}
		if (*c == '"')
			quoted = !quoted;
		else if (*c == '#' && !quoted) {
			*c = '\0';
			break;
		}
	}
	return p;
}


int
is_root_local(void)
{
#ifdef ST_LOCAL
	struct statvfs vfs;

	if (statvfs("/", &vfs) == -1)
		return -1;
	return vfs.f_flag & ST_LOCAL ? 1 : 0;
#else
	errno = ENOTSUP;
	return -1;
#endif
}
