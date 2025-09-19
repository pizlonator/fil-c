/*
 * Copyright © 2025 Red Hat, Inc.
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

#include "util-macros.h"

/**
 * This is a C version of the Rust newtypes in the
 * form struct Foo(u32);
 *
 * Use: DECLARE_NEWTYPE(foo, int)
 *
 * Defines a single-value struct called foo_t
 * with the following helper functions:
 *
 * - int foo_as_int(foo_t f);
 * - int foo(foo f);
 * - foo_t foo_from_int(int);
 * - foo_t foo_copy(foo_t f);
 * - foo_t foo_min(foo_t a, foo b);
 * - foo_t foo_max(foo_t a, foo b);
 * - foo_t foo_cmp(foo_t a, foo b);
 * - bool foo_eq(foo_t a, int b);
 * - bool foo_ne(foo_t a, int b);
 * - bool foo_le(foo_t a, int b);
 * - bool foo_lt(foo_t a, int b);
 * - bool foo_ge(foo_t a, int b);
 * - bool foo_gt(foo_t a, int b);
 *
 * Since all the structs are single-value the provided functions don't
 * use pointers but pass the struct around as value. The compiler
 * hopefully optimizes things so this is the same as passing ints around
 * but having some better type-safety.
 *
 * For example, this logic error is no longer possible using newtypes:
 * ```
 * double cm_to_inches(int cm) { return cm / 2.54; }
 *
 * struct person {
 *      int age;
 *      int height;
 * };
 * struct person p = { .age = 20, .height = 180 };
 * double inches = cm_to_inches(p.age);
 * ```
 * With newtypes this would be:
 * ```
 * DECLARE_NEWTYPE(year, int);
 * DECLARE_NEWTYPE(cm, int);
 * DECLARE_NEWTYPE(in, double);
 *
 * in_t cm_to_inches(cm_t cm) { return in_from_double(cm_as_int(cm) / 2.54); }
 *
 * struct person {
 *      year_t age;
 *      cm_t height;
 * };
 * struct person p = { .age = year_from_int(20), .height = cm_from_int(180) };
 * in_t inches = cm_to_inches(p.age); // Compiler error!
 * in_t inches = cm_to_inches(p.height); // Yay
 * ```
 * And a side-effect is that the units are documented as part of the type.
 */
#define DECLARE_NEWTYPE(name_, type_) \
	typedef struct { \
		type_ v; \
	} name_##_t; \
	\
	static inline name_##_t name_##_from_##type_(type_ v) { return (name_##_t) { .v = v }; }; \
	static inline type_ name_##_as_##type_(name_##_t name_) { return name_.v; }; \
	static inline type_ name_(name_##_t name_) { return name_##_as_##type_(name_); } \
	static inline name_##_t name_##_copy(name_##_t name_) { return name_##_from_##type_(name_.v); }; \
	static inline name_##_t name_##_min(name_##_t a, name_##_t b) { \
		return name_##_from_##type_(min(a.v, b.v)); \
	}; \
	static inline name_##_t name_##_max(name_##_t a, name_##_t b) { \
		return name_##_from_##type_(max(a.v, b.v)); \
	}; \
	static inline int name_##_cmp(name_##_t a, name_##_t b) { \
		return a.v < b.v ? -1 : (a.v > b.v ? 1 : 0); \
	}; \
	static inline int name_##_eq(name_##_t a, type_ b) { return a.v == b; }\
	static inline int name_##_ne(name_##_t a, type_ b) { return a.v != b; }\
	static inline int name_##_le(name_##_t a, type_ b) { return a.v <= b; }\
	static inline int name_##_lt(name_##_t a, type_ b) { return a.v < b; }\
	static inline int name_##_ge(name_##_t a, type_ b) { return a.v >= b; }\
	static inline int name_##_gt(name_##_t a, type_ b) { return a.v > b; }\
	struct __useless_struct_to_allow_trailing_semicolon__
