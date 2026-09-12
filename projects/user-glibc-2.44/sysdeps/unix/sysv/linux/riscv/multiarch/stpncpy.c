/* Multiple versions of stpncpy.
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
#define SYMBOL_NAME stpncpy
# include <ifunc-init.h>

/* Redefine stpncpy so that the compiler won't complain about the type
   mismatch with the IFUNC selector in weak_alias, below.  */
# undef stpncpy
# undef __stpncpy
# define stpncpy REDIRECT_NAME
# define __stpncpy __redirect___stpncpy
# include <string.h>
# undef stpncpy
# undef __stpncpy

# include <stdint.h>
# include <riscv-ifunc.h>
# include <sys/hwprobe.h>

extern __typeof (REDIRECT_NAME) __stpncpy;
extern __typeof (REDIRECT_NAME) OPTIMIZE (generic) attribute_hidden;
extern __typeof (REDIRECT_NAME) OPTIMIZE (vector) attribute_hidden;

static inline __typeof (REDIRECT_NAME) *
select_stpncpy_ifunc (uint64_t dl_hwcap, __riscv_hwprobe_t hwprobe_func)
{
  unsigned long long int v;
  if (__riscv_hwprobe_one (hwprobe_func, RISCV_HWPROBE_KEY_IMA_EXT_0, &v) == 0
      && (v & RISCV_HWPROBE_IMA_V) == RISCV_HWPROBE_IMA_V)
    return OPTIMIZE (vector);
  return OPTIMIZE (generic);
}

riscv_libc_ifunc (__stpncpy, select_stpncpy_ifunc);

weak_alias (__stpncpy, stpncpy);

# ifdef SHARED
__hidden_ver1 (__stpncpy, __GI___stpncpy, __redirect___stpncpy)
  __attribute__ ((visibility ("hidden")));
# endif
#else
# include <string/stpncpy.c>
#endif
