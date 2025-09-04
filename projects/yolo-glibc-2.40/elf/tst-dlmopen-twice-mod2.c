/* Initialization of libc after dlmopen/dlclose/dlmopen (bug 29528).  Module 2.
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

#include <ctype.h>
#include <stdio.h>

static void __attribute__ ((constructor))
init (void)
{
  puts ("info: tst-dlmopen-twice-mod2.so loaded");
  fflush (stdout);
}

static void __attribute__ ((destructor))
fini (void)
{
  puts ("info: tst-dlmopen-twice-mod2.so about to be unloaded");
  fflush (stdout);
}

int
run_check (void)
{
  puts ("info: about to call isalpha");
  fflush (stdout);

  volatile char ch = 'a';
  if (!isalpha (ch))
    {
      puts ("error: isalpha ('a') is not true");
      fflush (stdout);
      return 1;
    }
  return 0;
}
