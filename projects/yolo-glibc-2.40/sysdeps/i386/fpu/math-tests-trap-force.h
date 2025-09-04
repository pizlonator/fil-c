/* Configuration for math tests: support for setting exception flags
   without causing enabled traps.  i686 version.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#ifndef I386_FPU_MATH_TESTS_TRAP_FORCE_H
#define I386_FPU_MATH_TESTS_TRAP_FORCE_H 1

#include <cpu-features.h>

/* Setting exception flags in FPU Status Register results in enabled traps for
   those exceptions being taken.  */
#define EXCEPTION_SET_FORCES_TRAP !CPU_FEATURE_USABLE (SSE)

#endif /* math-tests-trap-force.h.  */
