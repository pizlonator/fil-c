/* PLT trampolines.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <sys/asm.h>

#if !defined __loongarch_soft_float
#define USE_LASX
#define _dl_runtime_resolve _dl_runtime_resolve_lasx
#define _dl_runtime_profile _dl_runtime_profile_lasx
#include "dl-trampoline.h"
#undef FRAME_SIZE
#undef USE_LASX
#undef _dl_runtime_resolve
#undef _dl_runtime_profile

#define USE_LSX
#define _dl_runtime_resolve _dl_runtime_resolve_lsx
#define _dl_runtime_profile _dl_runtime_profile_lsx
#include "dl-trampoline.h"
#undef FRAME_SIZE
#undef USE_LSX
#undef _dl_runtime_resolve
#undef _dl_runtime_profile
#endif

#include "dl-trampoline.h"
