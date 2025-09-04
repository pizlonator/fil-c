/* Enumerate available IFUNC implementations of a function.  PowerPC64 version.
   Copyright (C) 2013-2024 Free Software Foundation, Inc.
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
#include <cpu-features.h>
#include <string.h>
#include <wchar.h>
#include <ldsodefs.h>
#include <ifunc-impl-list.h>

size_t
__libc_ifunc_impl_list (const char *name, struct libc_ifunc_impl *array,
			size_t max)
{
  size_t i = max;
  const struct cpu_features *features = &GLRO(dl_powerpc_cpu_features);
  unsigned long int hwcap = features->hwcap;
  unsigned long int hwcap2 = features->hwcap2;
#ifdef SHARED
  int cacheline_size = GLRO(dl_cache_line_size);
#endif

  /* hwcap contains only the latest supported ISA, the code checks which is
     and fills the previous supported ones.  */
  if (hwcap & PPC_FEATURE_ARCH_2_06)
    hwcap |= PPC_FEATURE_ARCH_2_05 | PPC_FEATURE_POWER5_PLUS |
             PPC_FEATURE_POWER5 | PPC_FEATURE_POWER4;
  else if (hwcap & PPC_FEATURE_ARCH_2_05)
    hwcap |= PPC_FEATURE_POWER5_PLUS | PPC_FEATURE_POWER5 | PPC_FEATURE_POWER4;
  else if (hwcap & PPC_FEATURE_POWER5_PLUS)
    hwcap |= PPC_FEATURE_POWER5 | PPC_FEATURE_POWER4;
  else if (hwcap & PPC_FEATURE_POWER5)
    hwcap |= PPC_FEATURE_POWER4;

#ifdef SHARED
  /* Support sysdeps/powerpc/powerpc64/multiarch/memcpy.c.  */
  IFUNC_IMPL (i, name, memcpy,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, memcpy,
			      hwcap2 & PPC_FEATURE2_ARCH_3_1
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __memcpy_power10)
#endif
	      IFUNC_IMPL_ADD (array, i, memcpy, hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __memcpy_power8_cached)
	      IFUNC_IMPL_ADD (array, i, memcpy, hwcap & PPC_FEATURE_ARCH_2_06
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __memcpy_power7)
	      IFUNC_IMPL_ADD (array, i, memcpy, hwcap & PPC_FEATURE_ARCH_2_06,
			      __memcpy_a2)
	      IFUNC_IMPL_ADD (array, i, memcpy, hwcap & PPC_FEATURE_ARCH_2_05,
			      __memcpy_power6)
	      IFUNC_IMPL_ADD (array, i, memcpy, hwcap & PPC_FEATURE_CELL_BE,
			      __memcpy_cell)
	      IFUNC_IMPL_ADD (array, i, memcpy, hwcap & PPC_FEATURE_POWER4,
			      __memcpy_power4)
	      IFUNC_IMPL_ADD (array, i, memcpy, 1, __memcpy_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/memmove.c.  */
  IFUNC_IMPL (i, name, memmove,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, memmove,
			      hwcap2 & PPC_FEATURE2_ARCH_3_1
			      && hwcap2 & PPC_FEATURE2_HAS_ISEL
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __memmove_power10)
#endif
	      IFUNC_IMPL_ADD (array, i, memmove, hwcap & PPC_FEATURE_ARCH_2_06
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __memmove_power7)
	      IFUNC_IMPL_ADD (array, i, memmove, 1, __memmove_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/memset.c.  */
  IFUNC_IMPL (i, name, memset,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, memset,
			      hwcap2 & PPC_FEATURE2_ARCH_3_1
			      && hwcap2 & PPC_FEATURE2_HAS_ISEL
			      && hwcap & PPC_FEATURE_HAS_VSX
			      && cacheline_size == 128,
			      __memset_power10)
#endif
	      IFUNC_IMPL_ADD (array, i, memset, hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC
			      && cacheline_size == 128,
			      __memset_power8)
	      IFUNC_IMPL_ADD (array, i, memset, hwcap & PPC_FEATURE_ARCH_2_06
			      && cacheline_size == 128,
			      __memset_power7)
	      IFUNC_IMPL_ADD (array, i, memset, hwcap & PPC_FEATURE_ARCH_2_05
			      && cacheline_size == 128,
			      __memset_power6)
	      IFUNC_IMPL_ADD (array, i, memset, hwcap & PPC_FEATURE_POWER4
			      && cacheline_size == 128,
			      __memset_power4)
	      IFUNC_IMPL_ADD (array, i, memset, 1, __memset_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strcpy.c.  */
  IFUNC_IMPL (i, name, strcpy,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, strcpy, hwcap2 & PPC_FEATURE2_ARCH_3_00
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strcpy_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, strcpy, hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strcpy_power8)
	      IFUNC_IMPL_ADD (array, i, strcpy, hwcap & PPC_FEATURE_ARCH_2_06
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strcpy_power7)
	      IFUNC_IMPL_ADD (array, i, strcpy, 1,
			      __strcpy_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/stpcpy.c.  */
  IFUNC_IMPL (i, name, stpcpy,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, stpcpy, hwcap2 & PPC_FEATURE2_ARCH_3_00
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __stpcpy_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, stpcpy, hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __stpcpy_power8)
	      IFUNC_IMPL_ADD (array, i, stpcpy, hwcap & PPC_FEATURE_ARCH_2_06
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __stpcpy_power7)
	      IFUNC_IMPL_ADD (array, i, stpcpy, 1,
			      __stpcpy_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strlen.c.  */
  IFUNC_IMPL (i, name, strlen,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, strlen, hwcap2 & PPC_FEATURE2_ARCH_3_1
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strlen_power10)
	      IFUNC_IMPL_ADD (array, i, strlen, hwcap2 & PPC_FEATURE2_ARCH_3_00
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strlen_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, strlen, hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strlen_power8)
	      IFUNC_IMPL_ADD (array, i, strlen, hwcap & PPC_FEATURE_ARCH_2_06,
			      __strlen_power7)
	      IFUNC_IMPL_ADD (array, i, strlen, 1,
			      __strlen_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strncmp.c.  */
  IFUNC_IMPL (i, name, strncmp,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, strncmp, hwcap2 & PPC_FEATURE2_ARCH_3_1
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strncmp_power10)
	      IFUNC_IMPL_ADD (array, i, strncmp, hwcap2 & PPC_FEATURE2_ARCH_3_00
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strncmp_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, strncmp, hwcap2 & PPC_FEATURE2_ARCH_2_07,
			      __strncmp_power8)
	      IFUNC_IMPL_ADD (array, i, strncmp, 1,
			      __strncmp_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strchr.c.  */
  IFUNC_IMPL (i, name, strchr,
	      IFUNC_IMPL_ADD (array, i, strchr,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strchr_power8)
	      IFUNC_IMPL_ADD (array, i, strchr,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strchr_power7)
	      IFUNC_IMPL_ADD (array, i, strchr, 1,
			      __strchr_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strchrnul.c.  */
  IFUNC_IMPL (i, name, strchrnul,
	      IFUNC_IMPL_ADD (array, i, strchrnul,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strchrnul_power8)
	      IFUNC_IMPL_ADD (array, i, strchrnul,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strchrnul_power7)
	      IFUNC_IMPL_ADD (array, i, strchrnul, 1,
			      __strchrnul_ppc))
#endif

  /* Support sysdeps/powerpc/powerpc64/multiarch/memcmp.c.  */
  IFUNC_IMPL (i, name, memcmp,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, memcmp,
            hwcap2 & PPC_FEATURE2_ARCH_3_1
            && hwcap & PPC_FEATURE_HAS_VSX,
			      __memcmp_power10)
#endif
	      IFUNC_IMPL_ADD (array, i, memcmp, hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __memcmp_power8)
	      IFUNC_IMPL_ADD (array, i, memcmp, hwcap & PPC_FEATURE_ARCH_2_06,
			      __memcmp_power7)
	      IFUNC_IMPL_ADD (array, i, memcmp, hwcap & PPC_FEATURE_POWER4,
			      __memcmp_power4)
	      IFUNC_IMPL_ADD (array, i, memcmp, 1, __memcmp_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/mempcpy.c.  */
  IFUNC_IMPL (i, name, mempcpy,
	      IFUNC_IMPL_ADD (array, i, mempcpy,
			      hwcap & PPC_FEATURE_ARCH_2_06
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __mempcpy_power7)
	      IFUNC_IMPL_ADD (array, i, mempcpy, 1,
			      __mempcpy_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/memchr.c.  */
  IFUNC_IMPL (i, name, memchr,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, memchr,
		              hwcap2 & PPC_FEATURE2_ARCH_3_1
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __memchr_power10)
#endif
	      IFUNC_IMPL_ADD (array, i, memchr,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __memchr_power8)
	      IFUNC_IMPL_ADD (array, i, memchr,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __memchr_power7)
	      IFUNC_IMPL_ADD (array, i, memchr, 1,
			      __memchr_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/memrchr.c.  */
  IFUNC_IMPL (i, name, memrchr,
	      IFUNC_IMPL_ADD (array, i, memrchr,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __memrchr_power8)
	      IFUNC_IMPL_ADD (array, i, memrchr,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __memrchr_power7)
	      IFUNC_IMPL_ADD (array, i, memrchr, 1,
			      __memrchr_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/rawmemchr.c.  */
  IFUNC_IMPL (i, name, rawmemchr,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, rawmemchr,
			      (hwcap2 & PPC_FEATURE2_ARCH_3_1)
                              && (hwcap & PPC_FEATURE_HAS_VSX),
                              __rawmemchr_power10)
	      IFUNC_IMPL_ADD (array, i, rawmemchr,
			      hwcap2 & PPC_FEATURE2_ARCH_3_00
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __rawmemchr_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, rawmemchr,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __rawmemchr_power7)
	      IFUNC_IMPL_ADD (array, i, rawmemchr, 1,
			      __rawmemchr_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strnlen.c.  */
  IFUNC_IMPL (i, name, strnlen,
	      IFUNC_IMPL_ADD (array, i, strnlen,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strnlen_power8)
	      IFUNC_IMPL_ADD (array, i, strnlen, hwcap & PPC_FEATURE_ARCH_2_06,
			      __strnlen_power7)
	      IFUNC_IMPL_ADD (array, i, strnlen, 1,
			      __strnlen_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strcasecmp.c.  */
  IFUNC_IMPL (i, name, strcasecmp,
	      IFUNC_IMPL_ADD (array, i, strcasecmp,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strcasecmp_power8)
	      IFUNC_IMPL_ADD (array, i, strcasecmp,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strcasecmp_power7)
	      IFUNC_IMPL_ADD (array, i, strcasecmp, 1, __strcasecmp_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strcasecmp_l.c.  */
  IFUNC_IMPL (i, name, strcasecmp_l,
	      IFUNC_IMPL_ADD (array, i, strcasecmp_l,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strcasecmp_l_power7)
	      IFUNC_IMPL_ADD (array, i, strcasecmp_l, 1,
			      __strcasecmp_l_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strncase.c.  */
  IFUNC_IMPL (i, name, strncasecmp,
	      IFUNC_IMPL_ADD (array, i, strncasecmp,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			       && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strncasecmp_power8)
	      IFUNC_IMPL_ADD (array, i, strncasecmp,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strncasecmp_power7)
	      IFUNC_IMPL_ADD (array, i, strncasecmp, 1, __strncasecmp_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strncase_l.c.  */
  IFUNC_IMPL (i, name, strncasecmp_l,
	      IFUNC_IMPL_ADD (array, i, strncasecmp_l,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strncasecmp_l_power7)
	      IFUNC_IMPL_ADD (array, i, strncasecmp_l, 1,
			      __strncasecmp_l_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strrchr.c.  */
  IFUNC_IMPL (i, name, strrchr,
	      IFUNC_IMPL_ADD (array, i, strrchr,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strrchr_power8)
	      IFUNC_IMPL_ADD (array, i, strrchr,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strrchr_power7)
	      IFUNC_IMPL_ADD (array, i, strrchr, 1,
			      __strrchr_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strncat.c.  */
  IFUNC_IMPL (i, name, strncat,
	      IFUNC_IMPL_ADD (array, i, strncat,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strncat_power8)
	      IFUNC_IMPL_ADD (array, i, strncat,
			      hwcap & PPC_FEATURE_ARCH_2_06
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strncat_power7)
	      IFUNC_IMPL_ADD (array, i, strncat, 1,
			      __strncat_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strncpy.c.  */
  IFUNC_IMPL (i, name, strncpy,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, strncpy,
			      (hwcap2 & PPC_FEATURE2_ARCH_3_00)
			      && (hwcap & PPC_FEATURE_HAS_VSX),
			      __strncpy_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, strncpy,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07,
			      __strncpy_power8)
	      IFUNC_IMPL_ADD (array, i, strncpy,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strncpy_power7)
	      IFUNC_IMPL_ADD (array, i, strncpy, 1,
			     __strncpy_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/stpncpy.c.  */
  IFUNC_IMPL (i, name, stpncpy,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, stpncpy,
			      (hwcap2 & PPC_FEATURE2_ARCH_3_00)
			      && (hwcap & PPC_FEATURE_HAS_VSX),
			      __stpncpy_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, stpncpy,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07,
			      __stpncpy_power8)
	      IFUNC_IMPL_ADD (array, i, stpncpy,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __stpncpy_power7)
	      IFUNC_IMPL_ADD (array, i, stpncpy, 1,
			     __stpncpy_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strcmp.c.  */
  IFUNC_IMPL (i, name, strcmp,
#ifdef __LITTLE_ENDIAN__
	      IFUNC_IMPL_ADD (array, i, strcmp,
			      (hwcap2 & PPC_FEATURE2_ARCH_3_1)
			      && (hwcap & PPC_FEATURE_HAS_VSX),
			      __strcmp_power10)
	      IFUNC_IMPL_ADD (array, i, strcmp,
			      hwcap2 & PPC_FEATURE2_ARCH_3_00
			      && hwcap & PPC_FEATURE_HAS_ALTIVEC,
			      __strcmp_power9)
#endif
	      IFUNC_IMPL_ADD (array, i, strcmp,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07,
			      __strcmp_power8)
	      IFUNC_IMPL_ADD (array, i, strcmp,
			      hwcap & PPC_FEATURE_ARCH_2_06,
			      __strcmp_power7)
	      IFUNC_IMPL_ADD (array, i, strcmp, 1,
			     __strcmp_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strcat.c.  */
  IFUNC_IMPL (i, name, strcat,
	      IFUNC_IMPL_ADD (array, i, strcat,
			      hwcap2 & PPC_FEATURE2_ARCH_2_07
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strcat_power8)
	      IFUNC_IMPL_ADD (array, i, strcat,
			      hwcap & PPC_FEATURE_ARCH_2_06
			      && hwcap & PPC_FEATURE_HAS_VSX,
			      __strcat_power7)
	      IFUNC_IMPL_ADD (array, i, strcat, 1,
			     __strcat_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strspn.c.  */
  IFUNC_IMPL (i, name, strspn,
             IFUNC_IMPL_ADD (array, i, strspn,
                             hwcap2 & PPC_FEATURE2_ARCH_2_07
                             && hwcap & PPC_FEATURE_HAS_VSX,
                             __strspn_power8)
             IFUNC_IMPL_ADD (array, i, strspn, 1,
                             __strspn_ppc))

  /* Support sysdeps/powerpc/powerpc64/multiarch/strcspn.c.  */
  IFUNC_IMPL (i, name, strcspn,
             IFUNC_IMPL_ADD (array, i, strcspn,
                             hwcap2 & PPC_FEATURE2_ARCH_2_07
                             && hwcap & PPC_FEATURE_HAS_VSX,
                             __strcspn_power8)
             IFUNC_IMPL_ADD (array, i, strcspn, 1,
                             __strcspn_ppc))

  return 0;
}
