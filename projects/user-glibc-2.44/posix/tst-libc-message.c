/* Internal test to verify __libc_fatal.
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

#include <signal.h>
#include <stdio.h>

#include <support/check.h>
#include <support/capture_subprocess.h>

static _Noreturn void
run_libc_message (void *closure)
{
  /* We only support 7 (LIBC_MESSAGE_MAX_ARGS) arguments.  Call with 8 to
     trigger failure.  */
  __libc_message_impl ("glibc: test",
		       "%s %s %s %s %s %s %s %s\n",
		       "1", "2", "3", "4", "5", "6", "7", "8");
  __builtin_unreachable ();
}

static int
do_test (void)
{
  struct support_capture_subprocess result
    = support_capture_subprocess (run_libc_message, NULL);
  support_capture_subprocess_check (&result, "libc_message", -SIGABRT,
				    sc_allow_stderr);

  TEST_COMPARE_STRING (result.err.buffer, IOVEC_MAX_ERR_MSG);

  support_capture_subprocess_free (&result);

  return 0;
}

#include <support/test-driver.c>
