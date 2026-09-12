/* Check that the entire main program image is readable if contiguous.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <link.h>
#include <support/check.h>
#include <support/xdlfcn.h>

static int
do_test (void)
{
  struct link_map *l = xdlopen ("", RTLD_NOW);
  if (!l->l_contiguous)
    FAIL_UNSUPPORTED ("main link map is not contiguous");

  /* This check only works if the kernel loaded the main program.  The
     dynamic loader replaces gaps with PROT_NONE mappings, resulting
     in faults.  */
  volatile long int *p = (volatile long int *) l->l_map_start;
  volatile long int *end = (volatile long int *) l->l_map_end;
  while (p < end)
    {
      *p;
      ++p;
    }

  xdlclose (l);

  return 0;
}
#include <support/test-driver.c>
