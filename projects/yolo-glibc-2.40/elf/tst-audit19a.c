/* Check if DT_AUDIT a module without la_plt{enter,exit} symbols does not incur
   in profiling (BZ#15533).
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

#include <link.h>
#include <support/xdlfcn.h>
#include <support/check.h>

static int
do_test (void)
{
  void *h = xdlopen ("tst-auditmod19a.so", RTLD_NOW);

  struct link_map *lmap;
  TEST_VERIFY_EXIT (dlinfo (h, RTLD_DI_LINKMAP, &lmap) == 0);

  /* The internal array is only allocated if profiling is enabled.  */
  TEST_VERIFY (lmap->l_reloc_result == NULL);

  return 0;
}

#include <support/test-driver.c>
