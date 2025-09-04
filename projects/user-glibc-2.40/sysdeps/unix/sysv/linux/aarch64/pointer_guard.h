/* Pointer guard implementation.  AArch64 version.
   Copyright (C) 2014-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef POINTER_GUARD_H
#define POINTER_GUARD_H

/* Pointer mangling is supported for AArch64.  */
#if (IS_IN (rtld) \
     || (!defined SHARED && (IS_IN (libc) \
                             || IS_IN (libpthread))))
# ifdef __ASSEMBLER__
/* Note, dst, src, guard, and tmp are all register numbers rather than
   register names so they will work with both ILP32 and LP64. */
#  define PTR_MANGLE(dst, src, guard, tmp)                                \
  LDST_PCREL (ldr, guard, tmp, C_SYMBOL_NAME(__pointer_chk_guard_local)); \
  PTR_MANGLE2 (dst, src, guard)
/* Use PTR_MANGLE2 for efficiency if guard is already loaded.  */
#  define PTR_MANGLE2(dst, src, guard)\
  eor x##dst, x##src, x##guard
#  define PTR_DEMANGLE(dst, src, guard, tmp)\
  PTR_MANGLE (dst, src, guard, tmp)
#  define PTR_DEMANGLE2(dst, src, guard)\
  PTR_MANGLE2 (dst, src, guard)
# else
extern uintptr_t __pointer_chk_guard_local attribute_relro attribute_hidden;
#  define PTR_MANGLE(var) \
  (var) = (__typeof (var)) ((uintptr_t) (var) ^ __pointer_chk_guard_local)
#  define PTR_DEMANGLE(var)     PTR_MANGLE (var)
# endif
#else
# ifdef __ASSEMBLER__
/* Note, dst, src, guard, and tmp are all register numbers rather than
   register names so they will work with both ILP32 and LP64. */
#  define PTR_MANGLE(dst, src, guard, tmp)                             \
  LDST_GLOBAL (ldr, guard, tmp, C_SYMBOL_NAME(__pointer_chk_guard));   \
  PTR_MANGLE2 (dst, src, guard)
/* Use PTR_MANGLE2 for efficiency if guard is already loaded.  */
#  define PTR_MANGLE2(dst, src, guard)\
  eor x##dst, x##src, x##guard
#  define PTR_DEMANGLE(dst, src, guard, tmp)\
  PTR_MANGLE (dst, src, guard, tmp)
#  define PTR_DEMANGLE2(dst, src, guard)\
  PTR_MANGLE2 (dst, src, guard)
# else
#  include <stdint.h>
extern uintptr_t __pointer_chk_guard attribute_relro;
#  define PTR_MANGLE(var) \
  (var) = (__typeof (var)) ((uintptr_t) (var) ^ __pointer_chk_guard)
#  define PTR_DEMANGLE(var) PTR_MANGLE (var)
# endif
#endif

#endif /* POINTER_GUARD_H */
