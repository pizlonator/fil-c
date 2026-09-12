/* Auditor that opens again an object that just has been opened.
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
#include <link.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

unsigned int
la_version (unsigned int v)
{
  return LAV_CURRENT;
}

static bool trigger_on_la_activity;

unsigned int
la_objopen (struct link_map *map, Lmid_t lmid, uintptr_t *cookie)
{
  printf ("info: la_objopen: \"%s\"\n", map->l_name);
  if (strstr (map->l_name, "/tst-dlopen-auditdupmod.so") != NULL)
    trigger_on_la_activity = true;
  return 0;
}

void
la_activity (uintptr_t *cookie, unsigned int flag)
{
  static unsigned int calls;
  ++calls;
  printf ("info: la_activity: call %u (flag %u)\n", calls, flag);
  fflush (stdout);
  if (trigger_on_la_activity)
    {
      /* Avoid triggering on the dlmopen call below.  */
      static bool recursion;
      if (recursion)
        return;
      recursion = true;

      puts ("info: about to dlmopen tst-dlopen-auditdupmod.so");
      fflush (stdout);
      void *handle = dlmopen (LM_ID_BASE, "tst-dlopen-auditdupmod.so",
                              RTLD_NOW);
      if (handle == NULL)
        {
          printf ("error: dlmopen: %s\n", dlerror ());
          fflush (stdout);
          _exit (1);
        }

      /* Check that the constructor has not run.  Running the
         constructor would require constructing its dependencies, but
         the constructor call that triggered this auditing activity
         has not completed, and constructors among the dependencies
         may not be able to deal with that.  */
      int *status = dlsym (handle, "auditdupmod_status");
      if (status == NULL)
        {
          printf ("error: dlsym: %s\n", dlerror ());
          fflush (stdout);
          _exit (1);
        }
      printf ("info: auditdupmod_status == %d\n", *status);
      if (*status != 0)
        {
          puts ("error: auditdupmod_status == 0 expected");
          fflush (stdout);
          _exit (1);
        }
      /* Checked in the destructor and the main program.  */
      ++*status;
      printf ("info: auditdupmod_status == %d\n", *status);

      /* Check that the module has been relocated.  */
      int **status_address = dlsym (handle, "auditdupmod_status_address");
      if (status_address == NULL || *status_address != status)
        {
          puts ("error: invalid auditdupmod_status address in"
                " tst-dlopen-auditdupmod.so");
          fflush (stdout);
          _exit (1);
        }

      fflush (stdout);
    }
}
