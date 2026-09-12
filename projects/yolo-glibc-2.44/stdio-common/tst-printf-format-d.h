/* Test feature wrapper for formatted 'dprintf' output.
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

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

/* We need to go through the POSIX-mandated dance to switch between
   handles on an open file description.  */

#define printf_under_test(...)						\
({									\
  __label__ out;							\
  int result;								\
									\
  result = fflush (stdout);						\
  if (result == EOF)							\
    {									\
      perror ("fflush");						\
      goto out;								\
    }									\
  result = lseek (STDOUT_FILENO, 0, SEEK_END);				\
  if (result < 0 && errno == ESPIPE)					\
    result = 0;								\
  if (result < 0)							\
    {									\
      perror ("lseek");							\
      goto out;								\
    }									\
  result = dprintf (STDOUT_FILENO, __VA_ARGS__);			\
  if (result < 0)							\
    {									\
      perror ("dprintf");						\
      goto out;								\
    }									\
  result = fseek (stdout, 0, SEEK_END);					\
  if (result < 0 && errno == ESPIPE)					\
    result = 0;								\
  if (result < 0)							\
    perror ("fseek");							\
out:									\
  result;								\
})
