/* Pointer guard implementation, assembly version.  Alpha version.
   Copyright (C) 2006-2026 Free Software Foundation, Inc.
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
# if IS_IN (rtld)
#  define PTR_GUARD_LOAD(tmp)                                   \
        ldah    tmp, __pointer_chk_guard_local($29) !gprelhigh; \
        ldq     tmp, __pointer_chk_guard_local(tmp) !gprellow
# elif defined SHARED
#  define PTR_GUARD_LOAD(tmp)                   \
        ldq     tmp, __pointer_chk_guard
# else
#  define PTR_GUARD_LOAD(tmp)                   \
        ldq     tmp, __pointer_chk_guard_local
# endif
# define PTR_MANGLE(dst, src, tmp, tmp2)        \
        PTR_GUARD_LOAD (tmp);                   \
        PTR_MANGLE2 (dst, src, tmp, tmp2)
# define PTR_MANGLE2(dst, src, tmp, tmp2)       \
        xor     src, tmp, dst;                  \
        sll     dst, 17, tmp2;                  \
        srl     dst, 47, dst;                   \
        bis     dst, tmp2, dst
# define PTR_DEMANGLE(dst, tmp, tmp2)           \
        PTR_GUARD_LOAD (tmp);                   \
        PTR_DEMANGLE2 (dst, tmp, tmp2)
# define PTR_DEMANGLE2(dst, tmp, tmp2)          \
        sll     dst, 47, tmp2;                  \
        srl     dst, 17, dst;                   \
        bis     dst, tmp2, dst;                 \
        xor     dst, tmp, dst
#endif

#endif /* POINTER_GUARD_ASM_H */
