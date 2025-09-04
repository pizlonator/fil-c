/* Check if kernel supports set VMA range name.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <support/check.h>
#include <support/support.h>
#include <support/xunistd.h>
#include <sys/mman.h>
#ifdef __linux__
# include <sys/prctl.h>
#endif

bool
support_set_vma_name_supported (void)
{
#ifdef __linux__
  size_t size = sysconf (_SC_PAGESIZE);
  if (size == -1)
    FAIL_EXIT1 ("sysconf (_SC_PAGESIZE): %m\n");

  void *vma = xmmap (0,
		     size,
		     PROT_NONE,
		     MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE,
		     -1);

  int r = prctl (PR_SET_VMA, PR_SET_VMA_ANON_NAME, vma, size, "vmaname");

  xmunmap (vma, size);

  return r == 0;
#else
  return false;
#endif
}
