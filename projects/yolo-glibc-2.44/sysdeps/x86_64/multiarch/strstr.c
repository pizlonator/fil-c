/* Multiple versions of strstr.
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2012-2026 Free Software Foundation, Inc.
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

/* Redefine strstr so that the compiler won't complain about the type
   mismatch with the IFUNC selector in strong_alias, below.  */
#undef  strstr
#define strstr __redirect_strstr
#include <string.h>
#undef  strstr

#define STRSTR __strstr_generic
#ifdef SHARED
# undef libc_hidden_builtin_def
# define libc_hidden_builtin_def(name) \
  __hidden_ver1 (__strstr_generic, __GI_strstr, __strstr_generic);
#endif

extern __typeof (__redirect_strstr) __strstr_sse2_unaligned attribute_hidden;
extern __typeof (__redirect_strstr) __strstr_generic attribute_hidden;

#include "string/strstr.c"

#include "init-arch.h"

static inline void *
IFUNC_SELECTOR (void)
{
  const struct cpu_features *cpu_features = __get_cpu_features ();

  if (CPU_FEATURES_ARCH_P (cpu_features, Fast_Unaligned_Load))
    return __strstr_sse2_unaligned;

  return __strstr_generic;
}

libc_ifunc_redirected (__redirect_strstr, __libc_strstr, IFUNC_SELECTOR ());
#undef strstr
strong_alias (__libc_strstr, strstr)
