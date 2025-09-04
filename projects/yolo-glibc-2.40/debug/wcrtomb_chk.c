/* Copyright (C) 2005-2024 Free Software Foundation, Inc.
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

#include <langinfo.h>
#include <locale.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale/localeinfo.h>


size_t
__wcrtomb_chk (char *s, wchar_t wchar, mbstate_t *ps, size_t buflen)
{
  return __wcrtomb_internal (s, wchar, ps, buflen);
}
libc_hidden_def (__wcrtomb_chk)
