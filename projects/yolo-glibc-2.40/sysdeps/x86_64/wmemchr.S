/* Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#define WMEMCHR	__wmemchr

#define DEFAULT_IMPL_V1	"multiarch/wmemchr-sse2.S"
#define DEFAULT_IMPL_V3	"multiarch/wmemchr-avx2.S"
#define DEFAULT_IMPL_V4	"multiarch/wmemchr-evex.S"

#include "isa-default-impl.h"

libc_hidden_def (__wmemchr)
weak_alias (__wmemchr, wmemchr)
libc_hidden_weak (wmemchr)
