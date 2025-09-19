/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util-macros.h"
#include "util-mem.h"

struct stringbuf {
	char *data;
	size_t len; /* len of log */
	size_t sz;  /* allocated size */
};

static inline void
stringbuf_init(struct stringbuf *b)
{
	b->len = 0;
	b->sz = 64;
	b->data = calloc(1, b->sz);
}

static inline bool
stringbuf_is_empty(struct stringbuf *b)
{
	return b->len == 0;
}

static inline void
stringbuf_reset(struct stringbuf *b)
{
	free(b->data);
	b->data = NULL;
	b->sz = 0;
	b->len = 0;
}

static inline struct stringbuf *
stringbuf_new(void)
{
	struct stringbuf *b = calloc(1, sizeof(*b));
	stringbuf_init(b);
	return b;
}

static inline void
stringbuf_destroy(struct stringbuf *b)
{
	stringbuf_reset(b);
	free(b);
}

DEFINE_DESTROY_CLEANUP_FUNC(stringbuf);

static inline char *
stringbuf_steal_destroy(struct stringbuf *b)
{
	char *str = b->data;
	b->data = NULL;
	stringbuf_destroy(b);
	return str;
}

static inline char *
stringbuf_steal(struct stringbuf *b)
{
	char *str = b->data;
	b->data = NULL;
	stringbuf_reset(b);
	return str;
}

/**
 * Ensure the stringbuf has space for at least sz bytes
 */
static inline int
stringbuf_ensure_size(struct stringbuf *b, size_t sz)
{
	if (b->sz < sz) {
		char *tmp = realloc(b->data, sz);
		if (!tmp)
			return -errno;
		b->data = tmp;
		memset(&b->data[b->len], 0, sz - b->len);
		b->sz = sz;
	}
	return 0;
}

/**
 * Ensure the stringbuf has space for at least sz *more* bytes
 */
static inline int
stringbuf_ensure_space(struct stringbuf *b, size_t sz)
{
	return stringbuf_ensure_size(b, b->len + sz);
}

/**
 * Append the the data from the fd to the string buffer.
 */
static inline int
stringbuf_append_from_fd(struct stringbuf *b, int fd, size_t maxlen)
{
	while (1) {
		size_t inc = maxlen ? maxlen : 1024;
		do {
			int r = stringbuf_ensure_space(b, inc);
			if (r < 0)
				return r;

			r = read(fd, &b->data[b->len], min(b->sz - b->len, inc));
			if (r <= 0) {
				if (r < 0 && errno != EAGAIN)
					return -errno;
				return 0;
			}
			b->len += r;
		} while (maxlen > 0);
	}

	return 0;
}

static inline int
stringbuf_append_string(struct stringbuf *b, const char *msg)
{
	size_t slen = strlen(msg);

	int r = stringbuf_ensure_space(b, slen + 1);
	if (r < 0)
		return r;

	if (!b->data) /* cannot happen, but let's make scan-build happy */
		abort();

	memcpy(b->data + b->len, msg, slen);
	b->len += slen;

	return 0;
}
