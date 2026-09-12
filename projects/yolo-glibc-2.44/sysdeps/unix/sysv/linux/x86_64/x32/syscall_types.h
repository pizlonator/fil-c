/* Types and macros used for syscall issuing.  x86_64/x32 version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

#ifndef _SYSCALL_TYPES_H
#define _SYSCALL_TYPES_H

#include <libc-diag.h>

typedef long long int __syscall_arg_t;

/* Syscall arguments for x32 follows x86_64 ABI, however pointers are 32 bits
   should be zero extended.  */
#define __SSC(__x) \
  ({					\
    TYPEFY (__x, __tmp) = ARGIFY (__x);	\
    (__syscall_arg_t) __tmp;		\
  })

#endif
