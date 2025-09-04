/* Pointer obfuscation implenentation.  i386 version.
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

#include <tcb-offsets.h>

#if IS_IN (rtld)
/* We cannot use the thread descriptor because in ld.so we use setjmp
   earlier than the descriptor is initialized.  Using a global variable
   is too complicated here since we have no PC-relative addressing mode.  */
# include <sysdeps/generic/pointer_guard.h>
#else
# ifdef __ASSEMBLER__
#  define PTR_MANGLE(reg)       xorl %gs:POINTER_GUARD, reg;                  \
                                roll $9, reg
#  define PTR_DEMANGLE(reg)     rorl $9, reg;                                 \
                                xorl %gs:POINTER_GUARD, reg
# else
#  define PTR_MANGLE(var)       asm ("xorl %%gs:%c2, %0\n"                    \
                                     "roll $9, %0"                            \
                                     : "=r" (var)                             \
                                     : "0" (var),                             \
                                       "i" (POINTER_GUARD))
#  define PTR_DEMANGLE(var)     asm ("rorl $9, %0\n"                          \
                                     "xorl %%gs:%c2, %0"                      \
                                     : "=r" (var)                             \
                                     : "0" (var),                             \
                                       "i" (POINTER_GUARD))
# endif
#endif

#endif /* POINTER_GUARD_H */
