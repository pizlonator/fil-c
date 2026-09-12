/* Module whose constructor should not be invoked by dlopen (NULL, RTLD_LAZY).
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

extern int mod1_status;
int mod2_status;

static void __attribute__ ((constructor))
init (void)
{
  printf ("info: tst-dlopen-constructor-null-mod2.so constructor"
          " (mod1_status=%d)", mod1_status);
  if (!(mod1_status == 1 && mod2_status == 0))
    {
      puts ("error: mod1_status == 1 && mod2_status == 0 expected");
      exit (1);
    }
  setenv ("mod2_status", "constructed", 1);
  mod2_status = 1;
}
