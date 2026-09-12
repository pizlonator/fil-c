/* Check that the entire libc.so program image is readable if contiguous.
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

#include <gnu/lib-names.h>
#include <link.h>
#include <support/check.h>
#include <support/xdlfcn.h>
#include <support/xunistd.h>
#include <sys/mman.h>
#include <unistd.h>

static int
do_test (void)
{
  struct link_map *l = xdlopen (LIBC_SO, RTLD_NOW);

  /* The dynamic loader fills holes with PROT_NONE mappings.  */
  if (!l->l_contiguous)
    FAIL_EXIT1 ("libc.so link map is not contiguous");

  /* Direct probing does not work because not everything is readable
     due to PROT_NONE mappings.  */
  int pagesize = getpagesize ();
  ElfW(Addr) addr = l->l_map_start;
  TEST_COMPARE (addr % pagesize, 0);
  while (addr < l->l_map_end)
    {
      void *expected = (void *) addr;
      void *ptr = xmmap (expected, 1, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1);
      if (ptr == expected)
        FAIL ("hole in libc.so memory image after %lu bytes",
              (unsigned long int) (addr - l->l_map_start));
      xmunmap (ptr, 1);
      addr += pagesize;
    }

  xdlclose (l);

  return 0;
}
#include <support/test-driver.c>
