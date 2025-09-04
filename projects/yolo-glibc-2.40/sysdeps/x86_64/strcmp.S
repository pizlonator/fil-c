/* strcmp dispatch for RTLD and non-multiarch build
   Copyright (C) 1999-2024 Free Software Foundation, Inc.
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

/* Symbol = strcmp.  */

#define DEFAULT_IMPL_V1	"multiarch/strcmp-sse2.S"
/* strcmp-sse2-unaligned.S is often faster than strcmp-sse42.S and
   doesn't have the drawback of using the `pcmpstri` instruction
   which can be very slow on some CPUs.  */
#define DEFAULT_IMPL_V2	"multiarch/strcmp-sse2-unaligned.S"
#define DEFAULT_IMPL_V3	"multiarch/strcmp-avx2.S"
#define DEFAULT_IMPL_V4	"multiarch/strcmp-evex.S"

#include "isa-default-impl.h"

libc_hidden_builtin_def (strcmp)
