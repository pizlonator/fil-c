/* Check dlopen failure on audit modules.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
#include <stdlib.h>

unsigned int
la_version (unsigned int v)
{
  return LAV_CURRENT;
}

static void
check (void)
{
  {
    void *mod = dlopen ("nonexistent.so", RTLD_NOW);
    if (mod != NULL)
      abort ();
  }

  {
    void *mod = dlmopen (LM_ID_BASE, "nonexistent.so", RTLD_NOW);
    if (mod != NULL)
      abort ();
  }
}

void
la_activity (uintptr_t *cookie, unsigned int flag)
{
  if (flag != LA_ACT_CONSISTENT)
    return;
  check ();
}

void
la_preinit (uintptr_t *cookie)
{
  check ();
}
