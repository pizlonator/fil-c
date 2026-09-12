/* Pointer obfuscation implementation, assembly version.  C-SKY version.
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
      || (!defined SHARED && (IS_IN (libc) || IS_IN (libpthread))))
#  define PTR_MANGLE_LOAD(guard)                        \
        grs     t0, 1f;                                 \
1:                                                      \
        lrw     guard, 1b@GOTPC;                        \
        addu    t0, guard;                              \
        lrw     guard, __pointer_chk_guard_local@GOT;   \
        ldr.w   guard, (t0, guard << 0);                \
        ldw     guard, (guard, 0);
# else
#  define PTR_MANGLE_LOAD(guard)                        \
        grs     t0, 1f;                                 \
1:                                                      \
        lrw     guard, 1b@GOTPC;                        \
        addu    t0, guard;                              \
        lrw     guard, __pointer_chk_guard@GOT;         \
        ldr.w   guard, (t0, guard << 0);                \
        ldw     guard, (guard, 0);
# endif
# define PTR_MANGLE(dst, src, guard)                    \
        PTR_MANGLE_LOAD (guard);                        \
        PTR_MANGLE2 (dst, src, guard)
# define PTR_MANGLE2(dst, src, guard)                   \
        xor     dst, src, guard;                        \
        rotli   dst, dst, 9
# define PTR_DEMANGLE(dst, src, guard)                  \
        PTR_MANGLE_LOAD (guard);                        \
        PTR_DEMANGLE2 (dst, src, guard)
# define PTR_DEMANGLE2(dst, src, guard)                 \
        rotli   dst, src, 23;                           \
        xor     dst, dst, guard
#endif

#endif /* POINTER_GUARD_ASM_H */
