/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2013-2015 Red Hat, Inc.
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

#ifndef HAVE_C23_AUTO
#define auto __auto_type
#endif

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])
/**
 * Iterate through the array _arr, assigning the variable elem to each
 * element. elem only exists within the loop.
 */
#define ARRAY_FOR_EACH(_arr, _elem) \
	for (__typeof__((_arr)[0]) *_elem = _arr; \
	     _elem < (_arr) + ARRAY_LENGTH(_arr); \
	     _elem++)

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(v, lo, hi) min((hi), max((lo), (v)))

#define ANSI_BOLD		"\x1B[0;1m"
#define ANSI_RED		"\x1B[0;31m"
#define ANSI_GREEN		"\x1B[0;32m"
#define ANSI_YELLOW		"\x1B[0;33m"
#define ANSI_BLUE		"\x1B[0;34m"
#define ANSI_MAGENTA		"\x1B[0;35m"
#define ANSI_CYAN		"\x1B[0;36m"
#define ANSI_WHITE		"\x1B[0;37m"
#define ANSI_BRIGHT_RED		"\x1B[0;91m"
#define ANSI_BRIGHT_GREEN	"\x1B[0;92m"
#define ANSI_BRIGHT_YELLOW	"\x1B[0;93m"
#define ANSI_BRIGHT_BLUE	"\x1B[0;94m"
#define ANSI_BRIGHT_MAGENTA	"\x1B[0;95m"
#define ANSI_BRIGHT_CYAN	"\x1B[0;96m"
#define ANSI_BRIGHT_WHITE	"\x1B[0;97m"
#define ANSI_BOLD_RED		"\x1B[0;1;31m"
#define ANSI_BOLD_GREEN		"\x1B[0;1;32m"
#define ANSI_BOLD_YELLOW	"\x1B[0;1;33m"
#define ANSI_BOLD_BLUE		"\x1B[0;1;34m"
#define ANSI_BOLD_MAGENTA	"\x1B[0;1;35m"
#define ANSI_BOLD_CYAN		"\x1B[0;1;36m"
#define ANSI_BOLD_WHITE		"\x1B[0;1;37m"
#define ANSI_BOLD_BRIGHT_RED	"\x1B[0;1;91m"
#define ANSI_BOLD_BRIGHT_GREEN	"\x1B[0;1;92m"
#define ANSI_BOLD_BRIGHT_YELLOW	"\x1B[0;1;93m"
#define ANSI_BOLD_BRIGHT_BLUE	"\x1B[0;1;94m"
#define ANSI_BOLD_BRIGHT_MAGENTA "\x1B[0;1;95m"
#define ANSI_BOLD_BRIGHT_CYAN	"\x1B[0;1;96m"
#define ANSI_BOLD_BRIGHT_WHITE	"\x1B[0;1;97m"
#define ANSI_NORMAL		"\x1B[0m"

#define ANSI_BG_RED		"\x1B[0;41m"
#define ANSI_BG_GREEN		"\x1B[0;42m"
#define ANSI_BG_YELLOW		"\x1B[0;43m"
#define ANSI_BG_BLUE		"\x1B[0;44m"
#define ANSI_BG_MAGENTA		"\x1B[0;45m"
#define ANSI_BG_CYAN		"\x1B[0;46m"
#define ANSI_BG_WHITE		"\x1B[0;47m"
#define ANSI_BG_BRIGHT_RED	"\x1B[0;101m"
#define ANSI_BG_BRIGHT_GREEN	"\x1B[0;102m"
#define ANSI_BG_BRIGHT_YELLOW	"\x1B[0;103m"
#define ANSI_BG_BRIGHT_BLUE	"\x1B[0;104m"
#define ANSI_BG_BRIGHT_MAGENTA	"\x1B[0;105m"
#define ANSI_BG_BRIGHT_CYAN	"\x1B[0;106m"
#define ANSI_BG_BRIGHT_WHITE    "\x1B[0;107m"

#define ANSI_UP			"\x1B[%dA"
#define ANSI_DOWN		"\x1B[%dB"
#define ANSI_RIGHT		"\x1B[%dC"
#define ANSI_LEFT		"\x1B[%dD"

#define ANSI_RGB(r, g, b)       "\x1B[38;2;" #r  ";" #g ";" #b "m"
#define ANSI_RGB_BG(r, g, b)    "\x1B[48;2;" #r  ";" #g ";" #b "m"

#define ANSI_CLEAR_LINE         "\x1B[2K"
#define ANSI_CLEAR_EOL          "\x1B[0K"

#define CASE_RETURN_STRING(a) case a: return #a

/**
 * Concatenate two macro args into one, e.g.:
 *	int CONCAT(foo_, __LINE__);
 * will produce:
 *	int foo_123;
 */
#define CONCAT2(X, Y) X##Y
#define CONCAT(X, Y) CONCAT2(X,Y)

#define _unused_ __attribute__((unused))
#define _fallthrough_ __attribute__((fallthrough))

/* Returns the number of macro arguments, this expands
 * _VARIABLE_MACRO_NARGS(a, b, c) to NTH_ARG(a, b, c, 15, 14, 13, .... 4, 3, 2, 1).
 * _VARIABLE_MACRO_NTH_ARG always returns the 16th argument which in our case is 3.
 *
 * If we want more than 16 values _VARIABLE_MACRO_COUNTDOWN and
 * _VARIABLE_MACRO_NTH_ARG both need to be updated.
 */
#define _VARIABLE_MACRO_NARGS(...)  _VARIABLE_MACRO_NARGS1(__VA_ARGS__, _VARIABLE_MACRO_COUNTDOWN)
#define _VARIABLE_MACRO_NARGS1(...) _VARIABLE_MACRO_NTH_ARG(__VA_ARGS__)

/* Add to this if we need more than 16 args */
#define _VARIABLE_MACRO_COUNTDOWN \
	15, 14, 13, 12, 11, 10, 9, 8, 7,  6,  5,  4,  3,  2, 1, 0

/* Return the 16th argument passed in. See _VARIABLE_MACRO_NARGS above for usage.
 * Note this is 1-indexed.
 */
#define _VARIABLE_MACRO_NTH_ARG(_1,                                                    \
				_2,                                                    \
				_3,                                                    \
				_4,                                                    \
				_5,                                                    \
				_6,                                                    \
				_7,                                                    \
				_8,                                                    \
				_9,                                                    \
				_10,                                                   \
				_11,                                                   \
				_12,                                                   \
				_13,                                                   \
				_14,                                                   \
				_15,                                                   \
				N,                                                     \
				...) N

/* Defines a different expansion of macros depending on the
 * number of arguments, e.g. it turns
 * VARIABLE_MACRO(_ARG, a, b, c) into _ARG3(a, b, c)
 *
 * This can be used to have custom macros that expand to different things
 * depending on the number of arguments. This example converts a
 * single macro argument into value + stringify, two arguments into
 * first and second argument.
 *
 *     #define _ARG1(_1)     _1, #_1,
 *     #define _ARG2(_1, _2) _1, _2,
 *
 *     #define MYMACRO(...) _VARIABLE_MACRO(_ARG, __VA_ARGS__)
 *
 *     static void foo(int value, char *name) { printf("%d: %s\n", value, name); }
 *
 *     int main(void) {
 *         foo(MYMACRO(0));           // prints "0: 0"
 *         foo(MYMACRO(0, "zero"));   // prints "0: zero"
 *         return 0;
 *     }
 *
 * The first argument to VARIABLE_MACRO defines the prefix of the
 * expander macros (here _ARG -> _ARG0, _ARG1, ...). These need to be defined
 * in the caller for the number of arguments accepted
 * (up to _VARIABLE_MACRO_COUNTDOWN args).
 */
#define _VARIABLE_MACRO(func, ...) CONCAT(func, _VARIABLE_MACRO_NARGS(__VA_ARGS__)) (__VA_ARGS__)
