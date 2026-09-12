/* Enumerate available IFUNC implementations of a function.  RISCV version.
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

#include <ifunc-impl-list.h>
#include <string.h>
#include <sys/hwprobe.h>

size_t
__libc_ifunc_impl_list (const char *name, struct libc_ifunc_impl *array,
			size_t max)
{
  size_t i = max;

  bool fast_unaligned = false;
  bool rvv_enabled = false;

  struct riscv_hwprobe pairs[2] = {
    {.key = RISCV_HWPROBE_KEY_CPUPERF_0},
    {.key = RISCV_HWPROBE_KEY_IMA_EXT_0}
  };

  if (__riscv_hwprobe (pairs, 2, 0, NULL, 0) == 0) {
    if ((pairs[0].value & RISCV_HWPROBE_MISALIGNED_MASK)
          == RISCV_HWPROBE_MISALIGNED_FAST)
      fast_unaligned = true;

    if (pairs[1].value & RISCV_HWPROBE_IMA_V)
      rvv_enabled = true;
  }

  IFUNC_IMPL (i, name, memcpy,
	      IFUNC_IMPL_ADD (array, i, memcpy, rvv_enabled,
			      __memcpy_vector)
	      IFUNC_IMPL_ADD (array, i, memcpy, fast_unaligned,
			      __memcpy_noalignment)
	      IFUNC_IMPL_ADD (array, i, memcpy, 1, __memcpy_generic))

  IFUNC_IMPL (i, name, memset,
	      IFUNC_IMPL_ADD (array, i, memset, rvv_enabled,
			      __memset_vector)
	      IFUNC_IMPL_ADD (array, i, memset, 1, __memset_generic))

  IFUNC_IMPL (i, name, strcat,
	      IFUNC_IMPL_ADD (array, i, strcat, rvv_enabled,
			      __strcat_vector)
	      IFUNC_IMPL_ADD (array, i, strcat, 1, __strcat_generic))

  IFUNC_IMPL (i, name, strcpy,
	      IFUNC_IMPL_ADD (array, i, strcpy, rvv_enabled,
			      __strcpy_vector)
	      IFUNC_IMPL_ADD (array, i, strcpy, 1, __strcpy_generic))

  IFUNC_IMPL (i, name, strlen,
	      IFUNC_IMPL_ADD (array, i, strlen, rvv_enabled,
			      __strlen_vector)
	      IFUNC_IMPL_ADD (array, i, strlen, 1, __strlen_generic))

  IFUNC_IMPL (i, name, strcmp,
	      IFUNC_IMPL_ADD (array, i, strcmp, rvv_enabled,
			      __strcmp_vector)
	      IFUNC_IMPL_ADD (array, i, strcmp, 1, __strcmp_generic))

  IFUNC_IMPL (i, name, strncmp,
	      IFUNC_IMPL_ADD (array, i, strncmp, rvv_enabled,
			      __strncmp_vector)
	      IFUNC_IMPL_ADD (array, i, strncmp, 1, __strncmp_generic))

  IFUNC_IMPL (i, name, memccpy,
	      IFUNC_IMPL_ADD (array, i, memccpy, rvv_enabled,
			      __memccpy_vector)
	      IFUNC_IMPL_ADD (array, i, memccpy, 1, __memccpy_generic))

  IFUNC_IMPL (i, name, memcmp,
	      IFUNC_IMPL_ADD (array, i, memcmp, rvv_enabled,
			      __memcmp_vector)
	      IFUNC_IMPL_ADD (array, i, memcmp, 1, __memcmp_generic))

  IFUNC_IMPL (i, name, memchr,
	      IFUNC_IMPL_ADD (array, i, memchr, rvv_enabled,
			      __memchr_vector)
	      IFUNC_IMPL_ADD (array, i, memchr, 1, __memchr_generic))

  IFUNC_IMPL (i, name, strchr,
	      IFUNC_IMPL_ADD (array, i, strchr, rvv_enabled,
			      __strchr_vector)
	      IFUNC_IMPL_ADD (array, i, strchr, 1, __strchr_generic))

  IFUNC_IMPL (i, name, strrchr,
	      IFUNC_IMPL_ADD (array, i, strrchr, rvv_enabled,
			      __strrchr_vector)
	      IFUNC_IMPL_ADD (array, i, strrchr, 1, __strrchr_generic))

  IFUNC_IMPL (i, name, memmove,
	      IFUNC_IMPL_ADD (array, i, memmove, rvv_enabled,
			      __memmove_vector)
	      IFUNC_IMPL_ADD (array, i, memmove, 1, __memmove_generic))

  IFUNC_IMPL (i, name, stpncpy,
	      IFUNC_IMPL_ADD (array, i, stpncpy, rvv_enabled,
			      __stpncpy_vector)
	      IFUNC_IMPL_ADD (array, i, stpncpy, 1, __stpncpy_generic))

  IFUNC_IMPL (i, name, strncpy,
	      IFUNC_IMPL_ADD (array, i, strncpy, rvv_enabled,
			      __strncpy_vector)
	      IFUNC_IMPL_ADD (array, i, strncpy, 1, __strncpy_generic))

  return 0;
}
