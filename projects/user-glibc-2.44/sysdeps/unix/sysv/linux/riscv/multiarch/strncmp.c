/* Multiple versions of strncmp.
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#if IS_IN (libc)
/* Redefine strncmp so that the compiler won't complain about the type
   mismatch with the IFUNC selector in strong_alias, below.  */
# undef strncmp
# define strncmp __redirect_strncmp
# include <stdint.h>
# include <string.h>
# include <ifunc-init.h>
# include <riscv-ifunc.h>
# include <sys/hwprobe.h>

extern __typeof (__redirect_strncmp) __libc_strncmp;

extern __typeof (__redirect_strncmp) __strncmp_generic attribute_hidden;
extern __typeof (__redirect_strncmp) __strncmp_vector attribute_hidden;

static inline __typeof (__redirect_strncmp) *
select_strncmp_ifunc (uint64_t dl_hwcap, __riscv_hwprobe_t hwprobe_func)
{
  unsigned long long int v;
  if (__riscv_hwprobe_one (hwprobe_func, RISCV_HWPROBE_KEY_IMA_EXT_0, &v) == 0
      && (v & RISCV_HWPROBE_IMA_V) == RISCV_HWPROBE_IMA_V)
    return __strncmp_vector;
  return __strncmp_generic;
}

riscv_libc_ifunc (__libc_strncmp, select_strncmp_ifunc);

# undef strncmp
strong_alias (__libc_strncmp, strncmp);
# ifdef SHARED
__hidden_ver1 (strncmp, __GI_strncmp, __redirect_strncmp)
  __attribute__ ((visibility ("hidden"))) __attribute_copy__ (strncmp);
# endif
#else
# include <string/strncmp.c>
#endif
