/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2020 Red Hat, Inc.
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

#pragma once

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static inline void *
zalloc(size_t size)
{
	void *p;

	/* We never need to alloc anything more than 1,5 MB so we can assume
	 * if we ever get above that something's going wrong */
	if (size > 1536 * 1024)
		assert(!"bug: internal malloc size limit exceeded");

	p = calloc(1, size);
	if (!p)
		abort();

	return p;
}

/**
 * Use: _cleanup_(somefunction) struct foo *bar;
 */
#define _cleanup_(_x) __attribute__((cleanup(_x)))

/**
 * Use: _unref_(foo) struct foo *bar;
 *
 * This requires foo_unrefp() to be present, use DEFINE_UNREF_CLEANUP_FUNC.
 */
#define _unref_(_type) __attribute__((cleanup(_type##_unrefp))) struct _type

/**
 * Use: _destroy_(foo) struct foo *bar;
 *
 * This requires foo_destroyp() to be present, use DEFINE_UNREF_CLEANUP_FUNC.
 */
#define _destroy_(_type) __attribute__((cleanup(_type##_destroyp))) struct _type

/**
 * Use: _free_(foo) struct foo *bar;
 *
 * This requires foo_freep() to be present, use DEFINE_FREE_CLEANUP_FUNC.
 */
#define _free_(_type) __attribute__((cleanup(_type##_freep))) struct _type

static inline void
_free_ptr_(void *p)
{
	free(*(void **)p);
}
/**
 * Use: _autofree_ char *data;
 */
#define _autofree_ _cleanup_(_free_ptr_)

static inline void
_close_fd_(int *fd)
{
	if (*fd != -1)
		close(*fd);
}

/**
 * Use: _autoclose_ int fd = open(...);
 */
#define _autoclose_ _cleanup_(_close_fd_)

static inline void
_close_file_(FILE **fp)
{
	if (*fp)
		fclose(*fp);
}

/**
 * Use: _autofclose_ FILE *fp = fopen(...);
 */
#define _autofclose_ _cleanup_(_close_file_)

/**
 * Use:
 * DEFINE_TRIVIAL_CLEANUP_FUNC(struct foo *, foo_unref)
 * _cleanup_(foo_unrefp) struct foo *bar;
 */
#define DEFINE_TRIVIAL_CLEANUP_FUNC(_type, _func)		\
	static inline void _func##p(_type *_p) {		\
		if (*_p)					\
			_func(*_p);				\
	}							\
	struct __useless_struct_to_allow_trailing_semicolon__

/**
 * Define a cleanup function for the struct type foo with a matching
 * foo_unref(). Use:
 * DEFINE_UNREF_CLEANUP_FUNC(foo)
 * _unref_(foo) struct foo *bar;
 */
#define DEFINE_UNREF_CLEANUP_FUNC(_type)		\
	static inline void _type##_unrefp(struct _type **_p) {	\
		if (*_p)					\
			_type##_unref(*_p);			\
	}							\
	struct __useless_struct_to_allow_trailing_semicolon__

/**
 * Define a cleanup function for the struct type foo with a matching
 * foo_destroy(). Use:
 * DEFINE_DESTROY_CLEANUP_FUNC(foo)
 * _destroy_(foo) struct foo *bar;
 */
#define DEFINE_DESTROY_CLEANUP_FUNC(_type)		\
	static inline void _type##_destroyp(struct _type **_p) {\
		if (*_p)					\
			_type##_destroy(*_p);			\
	}							\
	struct __useless_struct_to_allow_trailing_semicolon__

/**
 * Define a cleanup function for the struct type foo with a matching
 * foo_free(). Use:
 * DEFINE_FREE_CLEANUP_FUNC(foo)
 * _free_(foo) struct foo *bar;
 */
#define DEFINE_FREE_CLEANUP_FUNC(_type)		\
	static inline void _type##_freep(struct _type **_p) {\
		if (*_p)					\
			_type##_free(*_p);			\
	}							\
	struct __useless_struct_to_allow_trailing_semicolon__

static inline void *
_steal(void *ptr)
{
	void **original = (void **)ptr;
	void *swapped = *original;
	*original = NULL;
	return swapped;
}

/**
 * Resets the pointer content and resets the data to NULL.
 * This circumvents _cleanup_ handling for that pointer.
 * Use:
 *   _cleanup_free_ char *data = malloc();
 *   return steal(&data);
 *
 */
#define steal(ptr_) \
  (typeof(*ptr_))_steal(ptr_)

static inline int
steal_fd(int *fd)
{
	int copy = *fd;
	*fd = -1;
	return copy;
}
