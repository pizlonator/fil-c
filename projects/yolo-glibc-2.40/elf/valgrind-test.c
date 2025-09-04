/* This is the simple test intended to be called by
   tst-valgrind-smoke to perform vagrind smoke test.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <libintl.h>
#include <locale.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include <support/support.h>

int
main (int argc, char **argv)
{
  /* Do some non-trivial stuff that has been known to trigger
     issues under valgrind in the past.
     Setting up the locale textdomain makes sure to test some
     string/path comparisons, file search and library loading. */
  xsetlocale (LC_ALL, "");
  if (bindtextdomain ("translit", "") == NULL)
      return errno;
  if (textdomain ("translit") == NULL)
      return errno;

  /* Show what we are executing and how...  */
  char *me = realpath (argv[0], NULL);
  printf ("bin: %s\n", me);
  printf ("ld.so: %s\n", argv[1]);
  free (me);

  return 0;
}
