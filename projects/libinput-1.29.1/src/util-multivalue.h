
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

#pragma once

#include "config.h"

#include <stdbool.h>
#include <stdint.h>

#include "util-strings.h"

struct multivalue {
	char type;
	union {
		char s[256];
		char c;
		double d;
		bool b;
		uint32_t u;
		int32_t i;
	} value;
	char name[64];
};

static inline void
multivalue_extract(const struct multivalue *v, void *ptr)
{
	/* ignore false positives from gcc:
	 * ../src/util-multivalue.h:52:33: warning: array subscript ‘double[0]’ is
	 * partly outside array bounds of ‘int32_t[1]’ {aka ‘int[1]’} [-Warray-bounds=]
	 *  52 |         case 'd': *(double*)ptr = v->value.d; break;
	 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	switch (v->type) {
	case 'b':
		*(bool *)ptr = v->value.b;
		break;
	case 'c':
		*(char *)ptr = v->value.c;
		break;
	case 'u':
		*(uint32_t *)ptr = v->value.u;
		break;
	case 'i':
		*(int32_t *)ptr = v->value.i;
		break;
	case 'd':
		*(double *)ptr = v->value.d;
		break;
	case 's':
		*(const char **)ptr = v->value.s;
		break;
	default:
		abort();
	}
#pragma GCC diagnostic pop
}

static inline void
multivalue_extract_typed(const struct multivalue *v, char type, void *ptr)
{
	assert(type == v->type);
	multivalue_extract(v, ptr);
}

static inline struct multivalue
multivalue_copy(const struct multivalue *v)
{
	struct multivalue copy = *v;
	return copy;
}

static inline struct multivalue
multivalue_new_string(const char *str)
{
	struct multivalue v = { .type = 's' };

	assert(strlen(str) < sizeof(v.value.s));

	snprintf(v.value.s, sizeof(v.value.s), "%s", str);
	return v;
}

static inline struct multivalue
multivalue_new_char(char c)
{
	struct multivalue v = {
		.type = 'c',
		.value.c = c,
	};
	return v;
}

static inline struct multivalue
multivalue_new_double(double d)
{
	struct multivalue v = {
		.type = 'd',
		.value.d = d,
	};
	return v;
}

static inline struct multivalue
multivalue_new_u32(uint32_t u)
{
	struct multivalue v = {
		.type = 'u',
		.value.u = u,
	};
	return v;
}

static inline struct multivalue
multivalue_new_i32(int32_t i)
{
	struct multivalue v = {
		.type = 'i',
		.value.i = i,
	};
	return v;
}

static inline struct multivalue
multivalue_new_bool(bool b)
{
	struct multivalue v = {
		.type = 'b',
		.value.b = b,
	};
	return v;
}

static inline struct multivalue
multivalue_new_named_i32(int32_t value, const char *name)
{
	struct multivalue v = multivalue_new_i32(value);

	assert(strlen(name) < sizeof(v.name));

	snprintf(v.name, sizeof(v.name), "%s", name);
	return v;
}

static inline char *
multivalue_as_str(const struct multivalue *v)
{
	char *str;

	if (v->name[0])
		return safe_strdup(v->name);

	switch (v->type) {
	case 'd':
		xasprintf(&str, "%f", v->value.d);
		break;
	case 'u':
		xasprintf(&str, "%u", v->value.u);
		break;
	case 'i':
		xasprintf(&str, "%d", v->value.i);
		break;
	case 'b':
		xasprintf(&str, "%s", truefalse(v->value.b));
		break;
	case 'c':
		xasprintf(&str, "%c", v->value.c);
		break;
	case 's':
		str = safe_strdup(v->value.s);
		break;
	default:
		abort();
	}
	return str;
}
