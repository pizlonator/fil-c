/* Test assert as a variadic macro.
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

#undef NDEBUG
#include <assert.h>

static void
test1 (void)
{
  /* This is two macro arguments, but one function argument, so must
     be accepted for C23.  */
  assert ((int [2]) { 0, 1 }[1]);
}

#define NDEBUG
#include <assert.h>

static void
test2 (void)
{
  /* With NDEBUG, arbitrary sequences of tokens are valid as assert
     arguments; they do not need to form a single expression.  */
  assert (1, 2, 3, *);
  assert ();
}

static int
do_test (void)
{
  test1 ();
  test2 ();
  return 0;
}

#include <support/test-driver.c>
