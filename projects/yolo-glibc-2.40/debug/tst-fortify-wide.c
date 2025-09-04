/* Fortify check for wprintf.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <setjmp.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>

#include <support/support.h>

static volatile int chk_fail_ok;
static volatile int ret;
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

static const wchar_t *wstr3 = L"%ls%n%ls%n";
static const wchar_t *wstr4 = L"Hello, ";
static const wchar_t *wstr5 = L"World!\n";
static wchar_t wbuf2[20] = L"%ls";

#define WFAIL \
  do { wprintf (L"Failure on line %d\n", __LINE__); ret = 1; } while (0)
#define CHK_FAIL_START \
  chk_fail_ok = 1;				\
  if (! setjmp (chk_fail_buf))			\
    {
#define CHK_FAIL_END \
      chk_fail_ok = 0;				\
      WFAIL;					\
    }

static int
do_test (void)
{
  set_fortify_handler (handler);

  int n1, n2;

  int orientation = fwide (stdout, 1);
  if (orientation <= 0)
    WFAIL;

  /* Constant literals passed directly are always ok
     (even with warnings about possible bugs from GCC).  */
  if (wprintf (L"%ls%n%ls%n", wstr4, &n1, wstr5, &n2) != 14
      || n1 != 7 || n2 != 14)
    WFAIL;

  /* In this case the format string is not known at compile time,
     but resides in read-only memory, so is ok.  */
  if (wprintf (wstr3, wstr4, &n1, wstr5, &n2) != 14
      || n1 != 7 || n2 != 14)
    WFAIL;

  wcpcpy (&wbuf2[3], L"%n%ls%n");
  /* When the format string is writable and contains %n,
     with -D_FORTIFY_SOURCE=2 it causes __chk_fail.  */
  CHK_FAIL_START
  if (wprintf (wbuf2, wstr4, &n1, wstr5, &n1) != 14)
    WFAIL;
  CHK_FAIL_END

  /* But if there is no %n, even writable format string
     should work.  */
  wbuf2[8] = L'\0';
  if (wprintf (&wbuf2[5], wstr5) != 7)
    WFAIL;

  /* Check whether missing N$ formats are detected.  */
  CHK_FAIL_START
  wprintf (L"%3$d\n", 1, 2, 3, 4);
  CHK_FAIL_END

  return ret;
}

#include <support/test-driver.c>
