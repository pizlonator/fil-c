/* Configure optimized libm functions.  AArch64 version.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#ifndef ARM_MATH_PRIVATE_H
#define ARM_MATH_PRIVATE_H 1

#include <stdint.h>

/* For int64_t to double conversion, libgcc might not respect the rounding
   mode [1].

   [1] https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91970  */
#define TOINT64_INTRINSICS 0

#include_next <math_private.h>

#endif
