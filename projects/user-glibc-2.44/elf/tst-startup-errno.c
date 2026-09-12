/* Test the value of errno at program startup.
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
#include <stdlib.h>

/* Verify that errno is 0 at first ELF constructor execution and at
   the start of main.  */

static void set_ctor_errno (void) __attribute__((constructor));
static int ctor_errno = -1;

static void
set_ctor_errno (void)
{
  ctor_errno = errno;
}

static int
get_ctor_errno (void)
{
  return ctor_errno;
}

int
main (void)
{
  if (errno != 0)
    {
      printf ("At start of main errno set to %d != 0\n", errno);
      exit (1);
    }

  if (get_ctor_errno () != 0)
    {
      printf ("At ctor exec errno set to %d != 0\n", get_ctor_errno ());
      exit (1);
    }

  return 0;
}
