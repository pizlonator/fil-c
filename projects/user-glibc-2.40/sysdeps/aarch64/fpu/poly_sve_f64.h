/* Helpers for evaluating polynomials on double-precision SVE input, using
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

#ifndef AARCH64_FPU_POLY_SVE_F64_H
#define AARCH64_FPU_POLY_SVE_F64_H

#include <arm_sve.h>

/* Wrap SVE f64 helpers: evaluation of some scheme/order has form:
   sv_[scheme]_[order]_f64_x.  */
#define VTYPE svfloat64_t
#define STYPE double
#define VWRAP(f) sv_##f##_f64_x
#define DUP svdup_n_f64
#include "poly_sve_generic.h"
#undef DUP
#undef VWRAP
#undef STYPE
#undef VTYPE

#endif
