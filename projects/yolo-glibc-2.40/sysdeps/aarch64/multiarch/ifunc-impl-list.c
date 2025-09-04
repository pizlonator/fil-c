/* Enumerate available IFUNC implementations of a function.  AARCH64 version.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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

#include <assert.h>
#include <string.h>
#include <wchar.h>
#include <ldsodefs.h>
#include <ifunc-impl-list.h>
#include <init-arch.h>
#include <stdio.h>

size_t
__libc_ifunc_impl_list (const char *name, struct libc_ifunc_impl *array,
			size_t max)
{
  size_t i = max;

  INIT_ARCH ();

  /* Support sysdeps/aarch64/multiarch/memcpy.c, memmove.c and memset.c.  */
  IFUNC_IMPL (i, name, memcpy,
	      IFUNC_IMPL_ADD (array, i, memcpy, 1, __memcpy_thunderx)
	      IFUNC_IMPL_ADD (array, i, memcpy, 1, __memcpy_oryon1)
	      IFUNC_IMPL_ADD (array, i, memcpy, !bti, __memcpy_thunderx2)
#if HAVE_AARCH64_SVE_ASM
	      IFUNC_IMPL_ADD (array, i, memcpy, sve && !bti, __memcpy_a64fx)
	      IFUNC_IMPL_ADD (array, i, memcpy, sve, __memcpy_sve)
#endif
	      IFUNC_IMPL_ADD (array, i, memcpy, mops, __memcpy_mops)
	      IFUNC_IMPL_ADD (array, i, memcpy, 1, __memcpy_generic))
  IFUNC_IMPL (i, name, memmove,
	      IFUNC_IMPL_ADD (array, i, memmove, 1, __memmove_thunderx)
	      IFUNC_IMPL_ADD (array, i, memmove, 1, __memmove_oryon1)
	      IFUNC_IMPL_ADD (array, i, memmove, !bti, __memmove_thunderx2)
#if HAVE_AARCH64_SVE_ASM
	      IFUNC_IMPL_ADD (array, i, memmove, sve && !bti, __memmove_a64fx)
	      IFUNC_IMPL_ADD (array, i, memmove, sve, __memmove_sve)
#endif
	      IFUNC_IMPL_ADD (array, i, memmove, mops, __memmove_mops)
	      IFUNC_IMPL_ADD (array, i, memmove, 1, __memmove_generic))
  IFUNC_IMPL (i, name, memset,
	      IFUNC_IMPL_ADD (array, i, memset, (zva_size == 64), __memset_zva64)
	      IFUNC_IMPL_ADD (array, i, memset, (zva_size == 64), __memset_oryon1)
	      IFUNC_IMPL_ADD (array, i, memset, 1, __memset_emag)
	      IFUNC_IMPL_ADD (array, i, memset, 1, __memset_kunpeng)
#if HAVE_AARCH64_SVE_ASM
	      IFUNC_IMPL_ADD (array, i, memset, sve && !bti && zva_size == 256, __memset_a64fx)
#endif
	      IFUNC_IMPL_ADD (array, i, memset, mops, __memset_mops)
	      IFUNC_IMPL_ADD (array, i, memset, 1, __memset_generic))
  IFUNC_IMPL (i, name, memchr,
	      IFUNC_IMPL_ADD (array, i, memchr, !mte, __memchr_nosimd)
	      IFUNC_IMPL_ADD (array, i, memchr, 1, __memchr_generic))

  IFUNC_IMPL (i, name, strlen,
	      IFUNC_IMPL_ADD (array, i, strlen, !mte, __strlen_asimd)
	      IFUNC_IMPL_ADD (array, i, strlen, 1, __strlen_generic))

  return 0;
}
