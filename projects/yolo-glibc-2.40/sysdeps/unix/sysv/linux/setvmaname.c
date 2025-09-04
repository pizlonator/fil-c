/* Utilities functions to name memory mappings.
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

#include <ldsodefs.h>
#include <setvmaname.h>
#include <sys/prctl.h>
#include <sysdep.h>
#include <elf/dl-tunables.h>

/* If PR_SET_VMA_ANON_NAME is not supported by the kernel, prctl returns
   EINVAL.  However, it also returns the same error for invalid argument.
   Since it is an internal-only API, it assumes well formatted input:
   aligned START, with (START, START+LEN) being a valid memory range,
   and NAME with a limit of 80 characters without invalid one ("\\`$[]").  */

void
__set_vma_name (void *start, size_t len, const char *name)
{
  static int prctl_supported = 1;
  if (atomic_load_relaxed (&prctl_supported) == 0)
    return;

  /* Set the prctl as not supported to avoid checking the tunable on every
     call.  */
  if (TUNABLE_GET (glibc, mem, decorate_maps, int32_t, NULL) != 0)
    {
      int r = INTERNAL_SYSCALL_CALL (prctl, PR_SET_VMA, PR_SET_VMA_ANON_NAME,
				     start, len, name);
      if (r == 0 || r != -EINVAL)
	return;
    }
  atomic_store_relaxed (&prctl_supported, 0);
  return;
}
