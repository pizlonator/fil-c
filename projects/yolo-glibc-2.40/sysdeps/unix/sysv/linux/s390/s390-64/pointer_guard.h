/* Pointer obfuscation implenentation.  s390x version.
   Copyright (C) 2005-2024 Free Software Foundation, Inc.
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
/* For the time being just use stack_guard rather than a separate
   pointer_guard.  */
# ifdef __ASSEMBLER__
#  define PTR_MANGLE(reg, tmpreg) \
  ear     tmpreg,%a0;			\
  sllg    tmpreg,tmpreg,32;		\
  ear     tmpreg,%a1;			\
  xg      reg,STACK_GUARD(tmpreg)
#  define PTR_MANGLE2(reg, tmpreg) \
  xg      reg,STACK_GUARD(tmpreg)
#  define PTR_DEMANGLE(reg, tmpreg) PTR_MANGLE (reg, tmpreg)
# else
#  include <stdint.h>
#  include <tls.h>
#  define PTR_MANGLE(var) \
  (var) = (void *) ((uintptr_t) (var) ^ THREAD_GET_POINTER_GUARD ())
#  define PTR_DEMANGLE(var)	PTR_MANGLE (var)
# endif
#endif

#endif /* POINTER_GUARD_H */
