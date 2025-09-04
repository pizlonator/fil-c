/* Lazy binding during dlclose.  Indirectly loaded module.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

void
exported_function (int ignored)
{
  /* This function is interposed from tst-dlclose-lazy-mod1.so and
     thus never called.  */
  abort ();
}

static void __attribute__ ((constructor))
init (void)
{
  puts ("info: tst-dlclose-lazy-mod2.so constructor called");

  /* Trigger lazy binding to the definition in
     tst-dlclose-lazy-mod1.so, but not for
     lazily_bound_exported_function in that module.  */
  exported_function (0);
}

static void __attribute__ ((destructor))
fini (void)
{
  puts ("info: tst-dlclose-lazy-mod2.so destructor called");

  /* Trigger the lazily_bound_exported_function call in
     exported_function in tst-dlclose-lazy-mod1.so.  */
  exported_function (1);
}
