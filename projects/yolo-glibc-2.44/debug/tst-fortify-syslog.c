/* Fortify tests for syslog interface.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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

#include <stdarg.h>
#include <setjmp.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

#include <support/check.h>
#include <support/support.h>
#include <support/capture_subprocess.h>

static const char *str2 = "F";
static char buf2[10] = "%s";

static volatile int chk_fail_ok;
static jmp_buf chk_fail_buf;

static void
handler (int sig)
{
  if (chk_fail_ok)
    {
      chk_fail_ok = 0;
      longjmp (chk_fail_buf, 1);
    }
  else
    _exit (127);
}

#define CHK_FAIL_START					\
  chk_fail_ok = 1;					\
  if (! setjmp (chk_fail_buf))				\
    {
#define CHK_FAIL_END					\
      chk_fail_ok = 0;					\
      FAIL ("not supposed to reach here");		\
    }

static void
call_vsyslog (int priority, const char *format, ...)
{
  va_list va;
  va_start (va, format);
  vsyslog (priority, format, va);
  va_end (va);
}

static void
run_syslog_chk (void *closure)
{
  int n1;
  CHK_FAIL_START
  syslog (LOG_USER | LOG_DEBUG, buf2, str2, &n1, str2, &n1);
  CHK_FAIL_END
}

static void
run_vsyslog_chk (void *closure)
{
  int n1;
  CHK_FAIL_START
  call_vsyslog (LOG_USER | LOG_DEBUG, buf2, str2, &n1, str2, &n1);
  CHK_FAIL_END
}

static int
do_test (void)
{
  set_fortify_handler (handler);

  int n1, n2;

  n1 = n2 = 0;
  syslog (LOG_USER | LOG_DEBUG, "%s%n%s%n", str2, &n1, str2, &n2);
  TEST_COMPARE (n1, 1);
  TEST_COMPARE (n2, 2);

  n1 = n2 = 0;
  call_vsyslog (LOG_USER | LOG_DEBUG, "%s%n%s%n", str2, &n1, str2, &n2);
  TEST_COMPARE (n1, 1);
  TEST_COMPARE (n2, 2);

  strcpy (buf2 + 2, "%n%s%n");

  /* The wrapper tests need to be in a subprocess because the abort called by
     printf does not unlock the internal syslog lock.  */
  {
    struct support_capture_subprocess result
      = support_capture_subprocess (run_syslog_chk, NULL);
    support_capture_subprocess_check (&result, "syslog", 0, sc_allow_stderr);
    support_capture_subprocess_free (&result);
  }

  {
    struct support_capture_subprocess result
      = support_capture_subprocess (run_vsyslog_chk, NULL);
    support_capture_subprocess_check (&result, "syslog", 0, sc_allow_stderr);
    support_capture_subprocess_free (&result);
  }

  return 0;
}

#include <support/test-driver.c>
