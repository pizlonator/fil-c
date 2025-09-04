/* Helpers for evaluating polynomials on double-precision AdvSIMD input, using
   various schemes.

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

#ifndef AARCH64_FPU_POLY_ADVSIMD_F64_H
#define AARCH64_FPU_POLY_ADVSIMD_F64_H

#include <arm_neon.h>

/* Wrap AdvSIMD f64 helpers: evaluation of some scheme/order has form:
   v_[scheme]_[order]_f64.  */
#define VTYPE float64x2_t
#define FMA(x, y, z) vfmaq_f64 (z, x, y)
#define VWRAP(f) v_##f##_f64
#include "poly_generic.h"
#undef VWRAP
#undef FMA
#undef VTYPE

#endif
