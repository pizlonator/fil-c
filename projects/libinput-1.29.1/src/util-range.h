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

#ifndef UTIL_RANGE_H
#define UTIL_RANGE_H

#include <stdbool.h>

struct range {
	int lower; /* inclusive */
	int upper; /* exclusive */
};

static inline struct range
range_init_empty(void)
{
	return (struct range){ .lower = 0, .upper = -1 };
}

static inline struct range
range_init_inclusive(int lower, int upper)
{
	return (struct range){ .lower = lower, .upper = upper + 1 };
}

static inline struct range
range_init_exclusive(int lower, int upper)
{
	return (struct range){ .lower = lower, .upper = upper };
}

static inline bool
range_is_valid(const struct range *r)
{
	return r->upper > r->lower;
}

#define range_for_each(range_, r_) \
	for (r_ = (range_)->lower; r < (range_)->upper; r++)

#endif
