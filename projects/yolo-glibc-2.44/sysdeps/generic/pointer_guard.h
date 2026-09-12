/* Pointer obfuscation implenentation.  Generic version.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#ifndef POINTER_GUARD_H
#define POINTER_GUARD_H

#include <pointer_guard-asm.h>

#ifndef __ASSEMBLER__
# include <stdbit.h>
# include <stdint.h>

# if (IS_IN (rtld) \
      || (!defined SHARED && (IS_IN (libc) || IS_IN (libpthread))))
extern uintptr_t __pointer_chk_guard_local attribute_relro attribute_hidden;
#  define PTR_GUARD_VALUE	__pointer_chk_guard_local
# else
extern uintptr_t __pointer_chk_guard attribute_relro;
#  define PTR_GUARD_VALUE	__pointer_chk_guard
# endif

# define PTR_MANGLE(var)						      \
    do {								      \
      (var) = (__typeof (var)) ((uintptr_t) (var) ^ PTR_GUARD_VALUE);	      \
      (var) = (__typeof (var)) stdc_rotate_left ((uintptr_t) (var),	      \
						 2 * sizeof (uintptr_t) + 1); \
    } while (0)
# define PTR_DEMANGLE(var)						      \
    do {								      \
      (var) = (__typeof (var)) stdc_rotate_right ((uintptr_t) (var),	      \
						  2 * sizeof (uintptr_t) + 1); \
      (var) = (__typeof (var)) ((uintptr_t) (var) ^ PTR_GUARD_VALUE);	      \
    } while (0)
#endif

#endif /* POINTER_GUARD_H */
