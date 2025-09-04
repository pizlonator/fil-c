/* Test struct r_debug_extended via DT_DEBUG.
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

#include <stdio.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <gnu/lib-names.h>
#include <support/xdlfcn.h>
#include <support/check.h>
#include <support/test-driver.h>

#define E(x) x
#define EW(x) ElfW(x)
#include <dl-r_debug.h>

static int
do_test (void)
{
  ElfW(Dyn) *d;
  struct r_debug_extended *debug = NULL;

  for (d = _DYNAMIC; d->d_tag != DT_NULL; ++d)
    {
      debug = (struct r_debug_extended *) r_debug_address (d);
      if (debug != NULL)
	break;
    }

  TEST_VERIFY_EXIT (debug != NULL);
  TEST_COMPARE (debug->base.r_version, 1);
  TEST_VERIFY_EXIT (debug->r_next == NULL);

  void *h = xdlmopen (LM_ID_NEWLM, "$ORIGIN/tst-dlmopen1mod.so",
		      RTLD_LAZY);

  TEST_COMPARE (debug->base.r_version, 2);
  TEST_VERIFY_EXIT (debug->r_next != NULL);
  TEST_VERIFY_EXIT (debug->r_next->r_next == NULL);
  TEST_VERIFY_EXIT (debug->r_next->base.r_map != NULL);
  TEST_VERIFY_EXIT (debug->r_next->base.r_map->l_name != NULL);
  const char *name = basename (debug->r_next->base.r_map->l_name);
  TEST_COMPARE_STRING (name, "tst-dlmopen1mod.so");

  xdlclose (h);

  return 0;
}

#include <support/test-driver.c>
