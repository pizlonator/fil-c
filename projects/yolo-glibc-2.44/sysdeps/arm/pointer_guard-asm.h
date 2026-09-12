/* Pointer guard implementation, assembly version.  Arm version.
   Copyright (C) 2013-2026 Free Software Foundation, Inc.
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

#ifndef POINTER_GUARD_ASM_H
#define POINTER_GUARD_ASM_H

#ifdef __ASSEMBLER__
# if (IS_IN (rtld) \
      || (!defined SHARED && (IS_IN (libc) || IS_IN (libpthread))))
#  define PTR_MANGLE_LOAD(guard, tmp)                                   \
  LDR_HIDDEN (guard, tmp, C_SYMBOL_NAME(__pointer_chk_guard_local), 0)
# else
#  define PTR_MANGLE_LOAD(guard, tmp)                                   \
  LDR_GLOBAL (guard, tmp, C_SYMBOL_NAME(__pointer_chk_guard), 0)
# endif
# define PTR_MANGLE(dst, src, guard, tmp)                              \
  PTR_MANGLE_LOAD(guard, tmp);                                          \
  PTR_MANGLE2(dst, src, guard)
/* Use PTR_MANGLE2 for efficiency if guard is already loaded.  */
# define PTR_MANGLE2(dst, src, guard)          \
  eor dst, src, guard;                         \
  ror dst, dst, #23
# define PTR_DEMANGLE(dst, src, guard, tmp)    \
  PTR_MANGLE_LOAD(guard, tmp);                  \
  PTR_DEMANGLE2(dst, src, guard)
# define PTR_DEMANGLE2(dst, src, guard)        \
  ror dst, src, #9;                            \
  eor dst, dst, guard
#endif

#endif /* POINTER_GUARD_ASM_H */
