/*
 * procio.c -- Replace stdio for read and write on files below
 * proc to be able to read and write large buffers as well.
 *
 * Copyright (C) 2017 Werner Fink
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct pcookie {
	char	*buf;
	size_t	count;
	size_t	length;
	off_t	offset;
	int	fd;
	int	delim;
	int	final:1;
} pcookie_t;

static ssize_t proc_read(void *, char *, size_t);
static ssize_t proc_write(void *, const char *, size_t);
static int proc_close(void *);

__extension__
static cookie_io_functions_t procio = {
    .read  = proc_read,
    .write = proc_write,
    .seek  = NULL,
    .close = proc_close,
};

FILE *fprocopen(const char *path, const char *mode)
{
	pcookie_t *cookie = NULL;
	FILE *handle = NULL;
	mode_t flags = 0;
	size_t len = 0;
	int c, delim;

	if (!mode || !(len = strlen(mode))) {
		errno = EINVAL;
		goto out;
	}

	/* No append mode possible */
	switch (mode[0]) {
	case 'r':
		flags |= O_RDONLY;
		break;
	case 'w':
		flags |= O_WRONLY|O_TRUNC;
		break;
	default:
		errno = EINVAL;
		goto out;
	}

	delim = ',';				/* default delimeter is the comma */
	for (c = 1; c < len; c++) {
		switch (mode[c]) {
		case '\0':
			break;
		case '+':
			errno = EINVAL;
			goto out;
		case 'e':
			flags |= O_CLOEXEC;
			continue;
		case 'b':
		case 'm':
		case 'x':
			/* ignore this */
			continue;
		default:
			if (mode[c] == ' ' || (mode[c] >= ',' && mode[c] <= '.') || mode[c] == ':')
				delim = mode[c];
			else {
				errno = EINVAL;
				goto out;
			}
			break;
		}
		break;
	}

	cookie = (pcookie_t *)malloc(sizeof(pcookie_t));
	if (!cookie)
		goto out;
	cookie->count = BUFSIZ;
	cookie->buf = (char *)malloc(cookie->count);
	if (!cookie->buf) {
		int errsv = errno;
		free(cookie);
		errno = errsv;
		goto out;
	}
	cookie->final = 0;
	cookie->offset = 0;
	cookie->length = 0;
	cookie->delim = delim;

	cookie->fd = openat(AT_FDCWD, path, flags);
	if (cookie->fd < 0) {
		int errsv = errno;
		free(cookie->buf);
		free(cookie);
		errno = errsv;
		goto out;
	}

	handle = fopencookie(cookie, mode, procio);
	if (!handle) {
		int errsv = errno;
		close(cookie->fd);
		free(cookie->buf);
		free(cookie);
		errno = errsv;
		goto out;
	}
out:
	return handle;
}

static
ssize_t proc_read(void *c, char *buf, size_t count)
{
	pcookie_t *cookie = c;
	ssize_t len = -1;
	void *ptr;

	if (cookie->count < count) {
		ptr = realloc(cookie->buf, count);
		if (!ptr)
			goto out;
		cookie->buf = ptr;
		cookie->count = count;
	}

	while (!cookie->final) {
		len = read(cookie->fd, cookie->buf, cookie->count);

		if (len <= 0) {
			if (len == 0) {
				/* EOF */
				cookie->final = 1;
				cookie->buf[cookie->length] = '\0';
				break;
			}
			goto out;		/* error or done */
		}

		cookie->length = len;

		if (cookie->length < cookie->count)
			continue;

		/* Likly to small buffer here */

		lseek(cookie->fd, 0, SEEK_SET);	/* reset for a retry */

		ptr = realloc(cookie->buf, cookie->count += BUFSIZ);
		if (!ptr)
			goto out;
		cookie->buf = ptr;
	}

	len = count;
	if (cookie->length - cookie->offset < len)
		len = cookie->length - cookie->offset;

	if (len < 0)
		len = 0;

	if (len) {
		(void)memcpy(buf, cookie->buf+cookie->offset, len);
		cookie->offset += len;
	} else
		len = EOF;
out:
	return len;
}

#define LINELEN	4096

static
ssize_t proc_write(void *c, const char *buf, size_t count)
{
	pcookie_t *cookie = c;
	ssize_t len = -1;
	void *ptr;

	if (!count) {
		len = 0;
		goto out;
	}

						    /* NL is the final input */
	cookie->final = memrchr(buf, '\n', count) ? 1 : 0;

	while (cookie->count < cookie->offset + count) {
		ptr = realloc(cookie->buf, cookie->count += count);
		if (!ptr)
			goto out;
		cookie->buf = ptr;
	}

	len = count;
	(void)memcpy(cookie->buf+cookie->offset, buf, count);
	cookie->offset += count;

	if (cookie->final) {
		len = write(cookie->fd, cookie->buf, cookie->offset);
		if (len < 0 && errno == EINVAL) {
			size_t offset;
			off_t amount;
			char *token;
			/*
			 * Oops buffer might be to large, split buffer into
			 * pieces at delimeter if provided
			 */
			if (!cookie->delim)
				goto out;		/* Hey dude?! */
			offset = 0;
			do {
				token = NULL;
				if (cookie->offset > LINELEN)
					token = (char*)memrchr(cookie->buf+offset, cookie->delim, LINELEN);
				else
					token = (char*)memrchr(cookie->buf+offset, '\n', cookie->offset);
				if (token)
					*token = '\n';
				else {
					errno = EINVAL;
					len = -1;
					goto out;	/* Wrong/Missing delimeter? */
				}
				if (offset > 0)
					lseek(cookie->fd, 1, SEEK_CUR);

				amount = token-(cookie->buf+offset)+1;
				ptr = cookie->buf+offset;

				len = write(cookie->fd, ptr, amount);
				if (len < 1  || len >= cookie->offset)
					break;

				offset += len;
				cookie->offset -= len;

			} while (cookie->offset > 0);
		}
		if (len > 0)
			len = count;
	}
out:
	return len;
}

static
int proc_close(void *c)
{
	pcookie_t *cookie = c;
	close(cookie->fd);
	free(cookie->buf);
	free(cookie);
	return 0;
}
