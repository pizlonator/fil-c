/* Test wcstol functions with C23 binary integers (wide strings, GNU
   extensions, C23 wcstol features disabled).
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <features.h>
/* This file tests the old versions of GNU extension functions, which
   are not normally available to new binaries because GNU extensions
   normally imply C23 wcstol features.  */
#undef __GLIBC_USE_C23_STRTOL
#define __GLIBC_USE_C23_STRTOL 0

#define CHAR wchar_t
#define WIDE 1
#define FNPFX wcsto
#define L_(C) L ## C
#define TEST_C23 0
#define TEST_Q 1
#define TEST_LOCALE 1

#include "../stdlib/tst-strtol-binary-main.c"
