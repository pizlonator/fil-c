/* Pointer guard implementation.  Alpha version.
   Copyright (C) 2006-2024 Free Software Foundation, Inc.
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

/* Pointer mangling support.  Note that tls access is slow enough that
   we don't deoptimize things by placing the pointer check value there.  */

#ifdef __ASSEMBLER__
# if IS_IN (rtld)
#  define PTR_MANGLE(dst, src, tmp)                             \
        ldah    tmp, __pointer_chk_guard_local($29) !gprelhigh; \
        ldq     tmp, __pointer_chk_guard_local(tmp) !gprellow;  \
        xor     src, tmp, dst
#  define PTR_MANGLE2(dst, src, tmp)                            \
        xor     src, tmp, dst
# elif defined SHARED
#  define PTR_MANGLE(dst, src, tmp)             \
        ldq     tmp, __pointer_chk_guard;       \
        xor     src, tmp, dst
# else
#  define PTR_MANGLE(dst, src, tmp)             \
        ldq     tmp, __pointer_chk_guard_local; \
        xor     src, tmp, dst
# endif
# define PTR_MANGLE2(dst, src, tmp)             \
        xor     src, tmp, dst
# define PTR_DEMANGLE(dst, tmp)   PTR_MANGLE(dst, dst, tmp)
# define PTR_DEMANGLE2(dst, tmp)  PTR_MANGLE2(dst, dst, tmp)
#else
# include <stdint.h>
# if (IS_IN (rtld) \
      || (!defined SHARED && (IS_IN (libc) \
                              || IS_IN (libpthread))))
extern uintptr_t __pointer_chk_guard_local attribute_relro attribute_hidden;
#  define PTR_MANGLE(var) \
        (var) = (__typeof (var)) ((uintptr_t) (var) ^ __pointer_chk_guard_local)
# else
extern uintptr_t __pointer_chk_guard attribute_relro;
#  define PTR_MANGLE(var) \
        (var) = (__typeof(var)) ((uintptr_t) (var) ^ __pointer_chk_guard)
# endif
# define PTR_DEMANGLE(var)  PTR_MANGLE(var)
#endif /* ASSEMBLER */

#endif /* POINTER_GUARD_H */
