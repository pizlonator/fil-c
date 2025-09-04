/* Pointer obfuscation implenentation.  Nios II version.
   Copyright (C) 2015-2024 Free Software Foundation, Inc.
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

#if IS_IN (rtld)
/* We cannot use the thread descriptor because in ld.so we use setjmp
   earlier than the descriptor is initialized.  */
# include <sysdeps/generic/pointer_guard.h>
#else
# ifdef __ASSEMBLER__
#  define PTR_MANGLE_GUARD(guard) ldw guard, POINTER_GUARD(r23)
#  define PTR_MANGLE(dst, src, guard) xor dst, src, guard
#  define PTR_DEMANGLE(dst, src, guard) PTR_MANGLE (dst, src, guard)
# else
#  include <stdint.h>
#  include <tls.h>
#  define PTR_MANGLE(var) \
  (var) = (__typeof (var)) ((uintptr_t) (var) ^ THREAD_GET_POINTER_GUARD ())
#  define PTR_DEMANGLE(var)     PTR_MANGLE (var)
# endif
#endif

#endif /* POINTER_GUARD_H */
