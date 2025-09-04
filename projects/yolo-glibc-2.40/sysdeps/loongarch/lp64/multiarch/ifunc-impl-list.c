/* Enumerate available IFUNC implementations of a function LoongArch64 version.
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
   <http://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <string.h>
#include <wchar.h>
#include <ldsodefs.h>
#include <ifunc-impl-list.h>
#include <stdio.h>

size_t
__libc_ifunc_impl_list (const char *name, struct libc_ifunc_impl *array,
			size_t max)
{

  size_t i = max;

  IFUNC_IMPL (i, name, strlen,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strlen, SUPPORT_LASX, __strlen_lasx)
	      IFUNC_IMPL_ADD (array, i, strlen, SUPPORT_LSX, __strlen_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strlen, 1, __strlen_aligned)
	      )

  IFUNC_IMPL (i, name, strnlen,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strnlen, SUPPORT_LASX, __strnlen_lasx)
	      IFUNC_IMPL_ADD (array, i, strnlen, SUPPORT_LSX, __strnlen_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strnlen, 1, __strnlen_aligned)
	      )

  IFUNC_IMPL (i, name, strchr,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strchr, SUPPORT_LASX, __strchr_lasx)
	      IFUNC_IMPL_ADD (array, i, strchr, SUPPORT_LSX, __strchr_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strchr, 1, __strchr_aligned)
	      )

  IFUNC_IMPL (i, name, strchrnul,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strchrnul, SUPPORT_LASX, __strchrnul_lasx)
	      IFUNC_IMPL_ADD (array, i, strchrnul, SUPPORT_LSX, __strchrnul_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strchrnul, 1, __strchrnul_aligned)
	      )

  IFUNC_IMPL (i, name, strcmp,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strcmp, SUPPORT_LSX, __strcmp_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strcmp, 1, __strcmp_aligned)
	      )

  IFUNC_IMPL (i, name, strncmp,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strncmp, SUPPORT_LSX, __strncmp_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strncmp, 1, __strncmp_aligned)
	      )

  IFUNC_IMPL (i, name, strcpy,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strcpy, SUPPORT_LASX, __strcpy_lasx)
	      IFUNC_IMPL_ADD (array, i, strcpy, SUPPORT_LSX, __strcpy_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strcpy, SUPPORT_UAL, __strcpy_unaligned)
	      IFUNC_IMPL_ADD (array, i, strcpy, 1, __strcpy_aligned)
	      )

  IFUNC_IMPL (i, name, stpcpy,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, stpcpy, SUPPORT_LASX, __stpcpy_lasx)
	      IFUNC_IMPL_ADD (array, i, stpcpy, SUPPORT_LSX, __stpcpy_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, stpcpy, SUPPORT_UAL, __stpcpy_unaligned)
	      IFUNC_IMPL_ADD (array, i, stpcpy, 1, __stpcpy_aligned)
	      )

  IFUNC_IMPL (i, name, strrchr,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, strrchr, SUPPORT_LASX, __strrchr_lasx)
	      IFUNC_IMPL_ADD (array, i, strrchr, SUPPORT_LSX, __strrchr_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, strrchr, 1, __strrchr_aligned)
	      )

  IFUNC_IMPL (i, name, memcpy,
#if !defined __loongarch_soft_float
              IFUNC_IMPL_ADD (array, i, memcpy, SUPPORT_LASX, __memcpy_lasx)
              IFUNC_IMPL_ADD (array, i, memcpy, SUPPORT_LSX, __memcpy_lsx)
#endif
              IFUNC_IMPL_ADD (array, i, memcpy, SUPPORT_UAL, __memcpy_unaligned)
              IFUNC_IMPL_ADD (array, i, memcpy, 1, __memcpy_aligned)
              )

  IFUNC_IMPL (i, name, memmove,
#if !defined __loongarch_soft_float
              IFUNC_IMPL_ADD (array, i, memmove, SUPPORT_LASX, __memmove_lasx)
              IFUNC_IMPL_ADD (array, i, memmove, SUPPORT_LSX, __memmove_lsx)
#endif
              IFUNC_IMPL_ADD (array, i, memmove, SUPPORT_UAL, __memmove_unaligned)
              IFUNC_IMPL_ADD (array, i, memmove, 1, __memmove_aligned)
              )

  IFUNC_IMPL (i, name, rawmemchr,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, rawmemchr, SUPPORT_LASX, __rawmemchr_lasx)
	      IFUNC_IMPL_ADD (array, i, rawmemchr, SUPPORT_LSX, __rawmemchr_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, rawmemchr, 1, __rawmemchr_aligned)
	      )

  IFUNC_IMPL (i, name, memchr,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, memchr, SUPPORT_LASX, __memchr_lasx)
	      IFUNC_IMPL_ADD (array, i, memchr, SUPPORT_LSX, __memchr_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, memchr, 1, __memchr_aligned)
	      )

  IFUNC_IMPL (i, name, memrchr,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, memrchr, SUPPORT_LASX, __memrchr_lasx)
	      IFUNC_IMPL_ADD (array, i, memrchr, SUPPORT_LSX, __memrchr_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, memrchr, 1, __memrchr_generic)
	      )

  IFUNC_IMPL (i, name, memset,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, memset, SUPPORT_LASX, __memset_lasx)
	      IFUNC_IMPL_ADD (array, i, memset, SUPPORT_LSX, __memset_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, memset, SUPPORT_UAL, __memset_unaligned)
	      IFUNC_IMPL_ADD (array, i, memset, 1, __memset_aligned)
	      )

  IFUNC_IMPL (i, name, memcmp,
#if !defined __loongarch_soft_float
	      IFUNC_IMPL_ADD (array, i, memcmp, SUPPORT_LASX, __memcmp_lasx)
	      IFUNC_IMPL_ADD (array, i, memcmp, SUPPORT_LSX, __memcmp_lsx)
#endif
	      IFUNC_IMPL_ADD (array, i, memcmp, 1, __memcmp_aligned)
	      )
  return i;
}
