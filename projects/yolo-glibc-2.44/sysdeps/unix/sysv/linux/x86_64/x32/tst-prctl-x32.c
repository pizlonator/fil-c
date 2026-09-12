/* Smoke test for prctl.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <stdint.h>
#include <sys/prctl.h>
#include <support/check.h>

/* On x32, when parameters are passed in 64-bit registers, only the lower
   32 bits are used and the upper 32 bits must be cleared.  */
typedef union
{
  struct
    {
      union
	{
	  const char *ptr;
	  int i1;
	};
      int i2;
    } s;
  long long ll;
} parameter_t;

static int
__attribute__ ((noipa))
do_prctl (int op, long long arg1, long long arg2, long long arg3,
	  long long arg4)
{
  return prctl (op, arg1, arg2, arg3, arg4);
}

static int
do_test (void)
{
  parameter_t name = { { { "thread name" }, -1 } };
  parameter_t zero = { { { 0 }, -2 } };
  TEST_COMPARE (do_prctl (PR_SET_NAME, name.ll, zero.ll, zero.ll,
			  zero.ll), 0);
  char buffer[16] = { 0, };
  name.s.ptr = buffer;
  TEST_COMPARE (do_prctl (PR_GET_NAME, name.ll, zero.ll, zero.ll,
			  zero.ll), 0);
  char expected[16] = "thread name";
  TEST_COMPARE_BLOB (buffer, sizeof (buffer), expected, sizeof (expected));
  return 0;
}

#include <support/test-driver.c>
