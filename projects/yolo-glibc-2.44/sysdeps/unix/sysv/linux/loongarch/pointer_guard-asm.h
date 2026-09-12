/* Pointer obfuscation implementation, assembly version.  LoongArch version.
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

#ifndef POINTER_GUARD_ASM_H
#define POINTER_GUARD_ASM_H

#ifdef __ASSEMBLER__
# if (IS_IN (rtld) \
      || (!defined SHARED && (IS_IN (libc) \
      || IS_IN (libpthread))))
#  define PTR_MANGLE_LOAD(guard) \
  LOAD_LOCAL (guard, __pointer_chk_guard_local);
# else
#  define PTR_MANGLE_LOAD(guard) \
  LOAD_GLOBAL (guard, __pointer_chk_guard);
# endif
# define PTR_MANGLE(dst, src, guard) \
  PTR_MANGLE_LOAD (guard); \
  PTR_MANGLE2 (dst, src, guard);
# define PTR_DEMANGLE(dst, src, guard) \
  PTR_MANGLE_LOAD (guard); \
  PTR_DEMANGLE2 (dst, src, guard);
/* Use PTR_MANGLE2 for efficiency if guard is already loaded.  */
# define PTR_MANGLE2(dst, src, guard) \
  xor  dst, src, guard; \
  rotri.d  dst, dst, 47;
# define PTR_DEMANGLE2(dst, src, guard) \
  rotri.d  dst, src, 17; \
  xor  dst, dst, guard;
#endif

#endif /* POINTER_GUARD_ASM_H */
