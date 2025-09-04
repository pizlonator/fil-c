/* strnlen dispatch for RTLD and non-multiarch build
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#define STRNLEN	__strnlen

#define DEFAULT_IMPL_V1	"multiarch/strnlen-sse2.S"
#define DEFAULT_IMPL_V3	"multiarch/strnlen-avx2.S"
#define DEFAULT_IMPL_V4	"multiarch/strnlen-evex.S"

#include "isa-default-impl.h"

weak_alias (__strnlen, strnlen)
libc_hidden_def (__strnlen)
libc_hidden_def (strnlen)
