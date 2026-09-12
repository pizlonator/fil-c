/* Multiple versions of strchr.
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
/* Redefine strchr so that the compiler won't complain about the type
   mismatch with the IFUNC selector in strong_alias, below.  */
# undef strchr
# define strchr __redirect_strchr
# include <stdint.h>
# include <string.h>
# include <ifunc-init.h>
# include <riscv-ifunc.h>
# include <sys/hwprobe.h>

extern __typeof (__redirect_strchr) __libc_strchr;

extern __typeof (__redirect_strchr) __strchr_generic attribute_hidden;
extern __typeof (__redirect_strchr) __strchr_vector attribute_hidden;

static inline __typeof (__redirect_strchr) *
select_strchr_ifunc (uint64_t dl_hwcap, __riscv_hwprobe_t hwprobe_func)
{
  unsigned long long int v;
  if (__riscv_hwprobe_one (hwprobe_func, RISCV_HWPROBE_KEY_IMA_EXT_0, &v) == 0
      && (v & RISCV_HWPROBE_IMA_V) == RISCV_HWPROBE_IMA_V)
    return __strchr_vector;
  return __strchr_generic;
}

riscv_libc_ifunc (__libc_strchr, select_strchr_ifunc);

# undef strchr
# undef index
strong_alias (__libc_strchr, strchr);
weak_alias (strchr, index);
# ifdef SHARED
__hidden_ver1 (strchr, __GI_strchr, __redirect_strchr)
  __attribute__ ((visibility ("hidden"))) __attribute_copy__ (strchr);
# endif
#else
# include <string/strchr.c>
#endif
