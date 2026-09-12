/* Multiple versions of memcmp.
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
/* Redefine memcmp so that the compiler won't complain about the type
   mismatch with the IFUNC selector in strong_alias, below.  */
# undef memcmp
# define memcmp __redirect_memcmp
# include <stdint.h>
# include <string.h>
# include <ifunc-init.h>
# include <riscv-ifunc.h>
# include <sys/hwprobe.h>

extern __typeof (__redirect_memcmp) __libc_memcmp;

extern __typeof (__redirect_memcmp) __memcmp_generic attribute_hidden;
extern __typeof (__redirect_memcmp) __memcmp_vector attribute_hidden;

static inline __typeof (__redirect_memcmp) *
select_memcmp_ifunc (uint64_t dl_hwcap, __riscv_hwprobe_t hwprobe_func)
{
  unsigned long long v;

  if (__riscv_hwprobe_one (hwprobe_func, RISCV_HWPROBE_KEY_IMA_EXT_0, &v) == 0
      && (v & RISCV_HWPROBE_IMA_V) == RISCV_HWPROBE_IMA_V)
    return __memcmp_vector;

  return __memcmp_generic;
}

riscv_libc_ifunc (__libc_memcmp, select_memcmp_ifunc);

# undef memcmp
# undef bcmp
strong_alias (__libc_memcmp, memcmp);
weak_alias (memcmp, bcmp);

# undef __memcmpeq
strong_alias (__libc_memcmp, __memcmpeq);
libc_hidden_def (__memcmpeq);
# ifdef SHARED
__hidden_ver1 (memcmp, __GI_memcmp, __redirect_memcmp)
  __attribute__ ((visibility ("hidden"))) __attribute_copy__ (memcmp);
# endif
#else
# include <string/memcmp.c>
#endif
