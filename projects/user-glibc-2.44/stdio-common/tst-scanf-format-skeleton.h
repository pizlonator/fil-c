/* Test skeleton declarations for formatted scanf input.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <stddef.h>

/* Amount dynamic memory allocations are increased by.  */
#define SIZE_CHUNK 32

/* Error codes for use by the tests.  */
enum
{
  INPUT_EOF = -1,		/* Premature end of input.  */
  INPUT_ERROR = -2,		/* System error reading input data.  */
  INPUT_FORMAT = -3,		/* Input data format error.  */
  INPUT_OVERFLOW = -4,		/* Input data arithmetic overflow.  */
  OUTPUT_TERM = -5,		/* String termination missing from output.  */
  OUTPUT_OVERRUN = -6		/* Output data overrun.  */
};

/* Input data retrieval handlers made available to individual tests.  */
static long long read_integer (int *);
static int read_input (void);

/* Input data line number tracker.  */
static size_t line;
