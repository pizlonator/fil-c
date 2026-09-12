/* Loading TLS-using modules from auditors (bug 32412).  Audit module.
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

#include <dlfcn.h>
#include <link.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

unsigned int
la_version (unsigned int version)
{
  /* Open some modules, to trigger DTV resizing before the switch to
     the main malloc.  */
  for (int i = 1; i <= 19; ++i)
    {
      char dso[30];
      snprintf (dso, sizeof (dso), "tst-tlsmod17a%d.so", i);
      char sym[30];
      snprintf (sym, sizeof(sym), "tlsmod17a%d", i);

      void *handle = dlopen (dso, RTLD_LAZY);
      if (handle == NULL)
        {
          printf ("error: dlmopen from auditor: %s\n", dlerror  ());
          fflush (stdout);
          _exit (1);
        }
      int (*func) (void) = dlsym (handle, sym);
      if (func == NULL)
        {
          printf ("error: dlsym from auditor: %s\n", dlerror  ());
          fflush (stdout);
          _exit (1);
        }
      /* Trigger TLS allocation.  */
      func ();
    }

  puts ("info: TLS-using modules loaded from auditor");
  fflush (stdout);

  return LAV_CURRENT;
}
