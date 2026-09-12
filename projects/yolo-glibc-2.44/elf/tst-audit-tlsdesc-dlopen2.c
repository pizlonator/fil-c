/* Loading TLS-using modules from auditors (bug 32412).  Main program.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <support/xdlfcn.h>
#include <stdio.h>

static int
do_test (void)
{
  puts ("info: start of main program");

  /* Load TLS-using modules, to trigger DTV resizing.  The dynamic
     linker will load them again (requiring their own TLS) because the
     dlopen calls from the auditor were in the auditing namespace.  */
  for (int i = 1; i <= 19; ++i)
    {
      char dso[30];
      snprintf (dso, sizeof (dso), "tst-tlsmod17a%d.so", i);
      char sym[30];
      snprintf (sym, sizeof(sym), "tlsmod17a%d", i);

      void *handle = xdlopen (dso, RTLD_LAZY);
      int (*func) (void) = xdlsym (handle, sym);
      /* Trigger TLS allocation.  */
      func ();
    }

  return 0;
}

#include <support/test-driver.c>
