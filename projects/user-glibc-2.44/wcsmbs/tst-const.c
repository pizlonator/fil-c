/* Test <wchar.h> const-generic macros.
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

#include <wchar.h>

void *vp;
const void *cvp;
wchar_t *wp;
const wchar_t *cwp;
size_t sz;
wchar_t wc;

#define CHECK_TYPE(EXPR, TYPE)					\
  _Static_assert (_Generic (EXPR, TYPE: 1), "type check")

static int
do_test (void)
{
  /* This is a compilation test.  */
  CHECK_TYPE (wmemchr (vp, wc, sz), wchar_t *);
  CHECK_TYPE (wmemchr (cvp, wc, sz), const wchar_t *);
  CHECK_TYPE (wmemchr (wp, wc, sz), wchar_t *);
  CHECK_TYPE (wmemchr (cwp, wc, sz), const wchar_t *);
  CHECK_TYPE (wmemchr (0, wc, sz), wchar_t *);
  CHECK_TYPE (wmemchr ((void *) 0, wc, sz), wchar_t *);
  CHECK_TYPE ((wmemchr) (cwp, wc, sz), wchar_t *);
  CHECK_TYPE (wcschr (vp, wc), wchar_t *);
  CHECK_TYPE (wcschr (cvp, wc), const wchar_t *);
  CHECK_TYPE (wcschr (wp, wc), wchar_t *);
  CHECK_TYPE (wcschr (cwp, wc), const wchar_t *);
  CHECK_TYPE (wcschr (0, wc), wchar_t *);
  CHECK_TYPE (wcschr ((void *) 0, wc), wchar_t *);
  CHECK_TYPE ((wcschr) (cwp, wc), wchar_t *);
  CHECK_TYPE (wcspbrk (vp, vp), wchar_t *);
  CHECK_TYPE (wcspbrk (vp, cvp), wchar_t *);
  CHECK_TYPE (wcspbrk (cvp, vp), const wchar_t *);
  CHECK_TYPE (wcspbrk (cvp, cvp), const wchar_t *);
  CHECK_TYPE (wcspbrk (wp, wp), wchar_t *);
  CHECK_TYPE (wcspbrk (wp, cwp), wchar_t *);
  CHECK_TYPE (wcspbrk (cwp, wp), const wchar_t *);
  CHECK_TYPE (wcspbrk (cwp, cwp), const wchar_t *);
  CHECK_TYPE (wcspbrk (0, wp), wchar_t *);
  CHECK_TYPE (wcspbrk (0, cwp), wchar_t *);
  CHECK_TYPE (wcspbrk ((void *) 0, wp), wchar_t *);
  CHECK_TYPE (wcspbrk ((void *) 0, cwp), wchar_t *);
  CHECK_TYPE ((wcspbrk) (cwp, cwp), wchar_t *);
  CHECK_TYPE (wcsrchr (vp, wc), wchar_t *);
  CHECK_TYPE (wcsrchr (cvp, wc), const wchar_t *);
  CHECK_TYPE (wcsrchr (wp, wc), wchar_t *);
  CHECK_TYPE (wcsrchr (cwp, wc), const wchar_t *);
  CHECK_TYPE (wcsrchr (0, wc), wchar_t *);
  CHECK_TYPE (wcsrchr ((void *) 0, wc), wchar_t *);
  CHECK_TYPE ((wcsrchr) (cwp, wc), wchar_t *);
  CHECK_TYPE (wcsstr (vp, vp), wchar_t *);
  CHECK_TYPE (wcsstr (vp, cvp), wchar_t *);
  CHECK_TYPE (wcsstr (cvp, vp), const wchar_t *);
  CHECK_TYPE (wcsstr (cvp, cvp), const wchar_t *);
  CHECK_TYPE (wcsstr (wp, wp), wchar_t *);
  CHECK_TYPE (wcsstr (wp, cwp), wchar_t *);
  CHECK_TYPE (wcsstr (cwp, wp), const wchar_t *);
  CHECK_TYPE (wcsstr (cwp, cwp), const wchar_t *);
  CHECK_TYPE (wcsstr (0, wp), wchar_t *);
  CHECK_TYPE (wcsstr (0, cwp), wchar_t *);
  CHECK_TYPE (wcsstr ((void *) 0, wp), wchar_t *);
  CHECK_TYPE (wcsstr ((void *) 0, cwp), wchar_t *);
  CHECK_TYPE ((wcsstr) (cwp, cwp), wchar_t *);
  return 0;
}

#include <support/test-driver.c>
