/* Test with interposed malloc with dynamic TLS.
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

#include <array_length.h>
#include <stdio.h>
#include <support/check.h>
#include <support/xdlfcn.h>

/* Defined in tst-recursive-tlsmallocmod.so.  */
extern __thread unsigned int malloc_subsytem_counter;

static int
do_test (void)
{
  /* 16 is large enough to exercise the DTV resizing case.  */
  void *handles[16];

  for (unsigned int i = 0; i < array_length (handles); ++i)
    {
      /* Re-use the TLS slot for module 0.  */
      if (i > 0)
        xdlclose (handles[0]);

      char soname[30];
      snprintf (soname, sizeof (soname), "tst-recursive-tlsmod%u.so", i);
      handles[i] = xdlopen (soname, RTLD_NOW);

      if (i > 0)
        {
          handles[0] = xdlopen ("tst-recursive-tlsmod0.so", RTLD_NOW);
          int (*fptr) (void) = xdlsym (handles[0], "get_threadvar_0");
          /* May trigger TLS storage allocation using malloc.  */
          TEST_COMPARE (fptr (), 0);
        }
    }

  for (unsigned int i = 0; i < array_length (handles); ++i)
    xdlclose (handles[i]);

  printf ("info: malloc subsystem calls: %u\n", malloc_subsytem_counter);
  TEST_VERIFY (malloc_subsytem_counter > 0);
  return 0;
}

#include <support/test-driver.c>
