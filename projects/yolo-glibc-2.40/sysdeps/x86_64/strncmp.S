/* strncmp dispatch for RTLD and non-multiarch build
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

/* Symbol = strncmp.  */

#define DEFAULT_IMPL_V1	"multiarch/strncmp-sse2.S"
/* This may cause regressions on some processors that heavily prefer
   aligned loads or have slow a implementation of the `pcmpstri`
   instruction.  */
#define DEFAULT_IMPL_V2	"multiarch/strncmp-sse4_2.S"
#define DEFAULT_IMPL_V3	"multiarch/strncmp-avx2.S"
#define DEFAULT_IMPL_V4	"multiarch/strncmp-evex.S"

#include "isa-default-impl.h"

libc_hidden_builtin_def (strncmp)
