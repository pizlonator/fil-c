/* Test feature wrapper for formatted 'asprintf' output.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>

#define printf_under_test(...)						\
({									\
  __label__ out;							\
  int result;								\
  char *str;								\
									\
  result = asprintf (&str, __VA_ARGS__);				\
  if (result < 0)							\
    {									\
      perror ("asprintf");						\
      goto out;								\
    }									\
  if (fwrite (str, sizeof (*str), result, stdout) != result)		\
    {									\
      perror ("fwrite");						\
      result = -1;							\
    }									\
  free (str);								\
out:									\
  result;								\
})

#ifndef TIMEOUT
# define TIMEOUT (DEFAULT_TIMEOUT * 12)
#endif
