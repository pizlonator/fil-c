/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
 * Copyright (C) 2010-2022 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * General memory allocation wrappers for malloc, realloc, calloc and strdup
 */

#ifndef UTIL_LINUX_XALLOC_H
#define UTIL_LINUX_XALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c.h"
#include "strutils.h"

#ifndef XALLOC_EXIT_CODE
# define XALLOC_EXIT_CODE EXIT_FAILURE
#endif

static inline
__ul_alloc_size(1)
__ul_returns_nonnull
void *xmalloc(const size_t size)
{
	void *ret = malloc(size);

	if (!ret && size)
		err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
	return ret;
}

static inline
__ul_alloc_size(2)
__ul_returns_nonnull
void *xrealloc(void *ptr, const size_t size)
{
	void *ret = realloc(ptr, size);

	if (!ret && size)
		err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
	return ret;
}

static inline
__ul_calloc_size(2, 3)
__ul_returns_nonnull
void *xreallocarray(void *ptr, const size_t nelems, const size_t size)
{
	void *ret = reallocarray(ptr, nelems, size);

	if (!ret && size && nelems)
		err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
	return ret;
}

static inline
__ul_calloc_size(1, 2)
__ul_returns_nonnull
void *xcalloc(const size_t nelems, const size_t size)
{
	void *ret = calloc(nelems, size);

	if (!ret && size && nelems)
		err(XALLOC_EXIT_CODE, "cannot allocate %zu bytes", size);
	return ret;
}

static inline
__attribute__((warn_unused_result))
__ul_returns_nonnull
char *xstrdup(const char *str)
{
	char *ret;

	assert(str);
	ret = strdup(str);
	if (!ret)
		err(XALLOC_EXIT_CODE, "cannot duplicate string");
	return ret;
}

static inline
__attribute__((warn_unused_result))
__ul_returns_nonnull
char *xstrndup(const char *str, size_t size)
{
	char *ret;

	assert(str);
	ret = strndup(str, size);
	if (!ret)
		err(XALLOC_EXIT_CODE, "cannot duplicate string");
	return ret;
}


static inline
__attribute__((__format__(printf, 2, 3)))
int xasprintf(char **strp, const char *fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vasprintf(&(*strp), fmt, args);
	va_end(args);
	if (ret < 0)
		err(XALLOC_EXIT_CODE, "cannot allocate string");
	return ret;
}

static inline
__attribute__((__format__(printf, 2, 0)))
int xvasprintf(char **strp, const char *fmt, va_list ap)
{
	int ret = vasprintf(&(*strp), fmt, ap);

	if (ret < 0)
		err(XALLOC_EXIT_CODE, "cannot allocate string");
	return ret;
}

static inline void xstrappend(char **a, const char *b)
{
	if (strappend(a, b) < 0)
		err(XALLOC_EXIT_CODE, "cannot allocate string");
}

static inline void xstrputc(char **a, char c)
{
	char b[] = {c, '\0'};
	xstrappend(a, b);
}

static inline
__attribute__((__format__(printf, 2, 0)))
int xstrvfappend(char **a, const char *format, va_list ap)
{
	int ret = strvfappend(a, format, ap);

	if (ret < 0)
		err(XALLOC_EXIT_CODE, "cannot allocate string");
	return ret;

}

static inline
__attribute__ ((__format__ (__printf__, 2, 3)))
int xstrfappend(char **a, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = xstrvfappend(a, format, ap);
	va_end(ap);

	return ret;
}

static inline
__attribute__((warn_unused_result))
char *xgethostname(void)
{
	char *name;
	size_t sz = get_hostname_max() + 1;

	name = xmalloc(sizeof(char) * sz);
	if (gethostname(name, sz) != 0) {
		free(name);
		return NULL;
	}
	name[sz - 1] = '\0';
	return name;
}

#endif
