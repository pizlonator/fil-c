/* Test stpcpy functions.
   Copyright (C) 1999-2024 Free Software Foundation, Inc.
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

#define STRCPY_RESULT(dst, len) ((dst) + (len))
#define TEST_MAIN
#ifndef WIDE
# define TEST_NAME "stpcpy"
#else
# define TEST_NAME "wcpcpy"
#endif /* !WIDE */
#include "test-string.h"
#ifndef WIDE
# define CHAR char
# define STPCPY stpcpy
#else
# include <wchar.h>
# define CHAR wchar_t
# define STPCPY wcpcpy
#endif /* !WIDE */

IMPL (STPCPY, 1)

/* Also check the generic implementation.  */
#undef STPCPY
#undef weak_alias
#define weak_alias(a, b)
#undef libc_hidden_def
#define libc_hidden_def(a)
#undef libc_hidden_builtin_def
#define libc_hidden_builtin_def(a)
#undef attribute_hidden
#define attribute_hidden
#ifndef WIDE
# define STPCPY __stpcpy_default
# include "string/stpcpy.c"
IMPL (__stpcpy_default, 1)
#else
# define __wcslen wcslen
# define __wmemcpy wmemcpy
# define WCPCPY __wcpcpy_default
# include "wcsmbs/wcpcpy.c"
IMPL (__wcpcpy_default, 1)
#endif

#undef CHAR
#include "test-strcpy.c"
