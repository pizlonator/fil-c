/* Module calling dlopen (NULL, RTLD_LAZY) to obtain the global scope.
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

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int mod1_status;

static void __attribute__ ((constructor))
init (void)
{
  puts ("info: tst-dlopen-constructor-null-mod1.so constructor");

  void *handle = dlopen (NULL, RTLD_LAZY);
  if (handle == NULL)
    {
      printf ("error: %s\n", dlerror ());
      exit (1);
    }
  puts ("info: dlopen returned");
  if (dlsym (handle, "malloc") != malloc)
    {
      puts ("error: dlsym did not produce expected result");
      exit (1);
    }
  dlclose (handle);

  /* Check that the second module's constructor has not executed.   */
  if (getenv ("mod2_status") != NULL)
    {
      printf ("error: mod2_status environment variable set: %s\n",
              getenv ("mod2_status"));
      exit (1);
    }

  /* Communicate to the second module that the constructor executed.   */
  mod1_status = 1;
}
