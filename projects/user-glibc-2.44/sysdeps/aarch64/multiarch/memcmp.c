/* Multiple versions of memcmp. AARCH64 version.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* Define multiple versions only for the definition in libc.  */

#if IS_IN (libc)
/* Redefine memcmp so that the compiler won't complain about the type
   mismatch with the IFUNC selector in strong_alias, below.  */
# undef memcmp
# define memcmp __redirect_memcmp
# include <string.h>
# include <init-arch.h>

extern __typeof (__redirect_memcmp) __libc_memcmp;

extern __typeof (__redirect_memcmp) __memcmp_generic attribute_hidden;
extern __typeof (__redirect_memcmp) __memcmp_kunpeng950 attribute_hidden;

static inline __typeof (__redirect_memcmp) *
select_memcmp_ifunc (void)
{
  INIT_ARCH ();

  if (sve)
  {
    if (IS_KUNPENG950 (midr))
    {
      return __memcmp_kunpeng950;
    }
  }
  return __memcmp_generic;
}

libc_ifunc (__libc_memcmp, select_memcmp_ifunc ());

# undef memcmp
strong_alias (__libc_memcmp, memcmp);
#endif
