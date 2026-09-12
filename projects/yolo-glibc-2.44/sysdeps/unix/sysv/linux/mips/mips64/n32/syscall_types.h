/* Types and macros used for syscall issuing.  MIPS64n32 version.
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

typedef long long int __syscall_arg_t;

/* Convert X to a long long, without losing any bits if it is one
   already or warning if it is a 32-bit pointer.  */
#define __SSC(__x) ((__syscall_arg_t) (__typeof__ ((__x) - (__x))) (__x))

#endif
