/* Multiple versions of memset. AARCH64 version.
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

/* Define multiple versions only for the definition in libc.  */

#if IS_IN (libc)
/* Redefine memset so that the compiler won't complain about the type
   mismatch with the IFUNC selector in strong_alias, below.  */
# undef memset
# define memset __redirect_memset
# include <string.h>
# include <init-arch.h>

extern __typeof (__redirect_memset) __libc_memset;

extern __typeof (__redirect_memset) __memset_zva64 attribute_hidden;
extern __typeof (__redirect_memset) __memset_emag attribute_hidden;
extern __typeof (__redirect_memset) __memset_kunpeng attribute_hidden;
extern __typeof (__redirect_memset) __memset_a64fx attribute_hidden;
extern __typeof (__redirect_memset) __memset_generic attribute_hidden;
extern __typeof (__redirect_memset) __memset_mops attribute_hidden;
extern __typeof (__redirect_memset) __memset_oryon1 attribute_hidden;

static inline __typeof (__redirect_memset) *
select_memset_ifunc (void)
{
  INIT_ARCH ();

  if (mops)
    return __memset_mops;

  if (sve && HAVE_AARCH64_SVE_ASM)
    {
      if (IS_A64FX (midr) && zva_size == 256)
	return __memset_a64fx;
    }

  if (IS_ORYON1 (midr) && zva_size == 64)
    return __memset_oryon1;

  if (IS_KUNPENG920 (midr))
    return __memset_kunpeng;

  if (IS_EMAG (midr))
    return __memset_emag;

  if (zva_size == 64)
    return __memset_zva64;

  return __memset_generic;
}

libc_ifunc (__libc_memset, select_memset_ifunc ());

# undef memset
strong_alias (__libc_memset, memset);
#endif
