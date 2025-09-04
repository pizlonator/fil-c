/* Fused multiply-add of long double (ldbl-128) values, narrowing the result to
   double, using soft-fp.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#define f32xfmaf64x __hide_f32xfmaf64x
#define f32xfmaf128 __hide_f32xfmaf128
#define f64fmaf64x __hide_f64fmaf64x
#define f64fmaf128 __hide_f64fmaf128
#include <math.h>
#undef f32xfmaf64x
#undef f32xfmaf128
#undef f64fmaf64x
#undef f64fmaf128

#include <math-narrow.h>
#include <libc-diag.h>

/* R_e is not set in cases where it is not used in packing, but the
   compiler does not see that it is set in all cases where it is
   used, resulting in warnings that it may be used uninitialized.
   The location of the warning differs in different versions of GCC,
   it may be where R is defined using a macro or it may be where the
   macro is defined.  */
DIAG_PUSH_NEEDS_COMMENT;
DIAG_IGNORE_NEEDS_COMMENT (4.9, "-Wmaybe-uninitialized");

#include <soft-fp.h>
#include <double.h>
#include <quad.h>

double
__dfmal (_Float128 x, _Float128 y, _Float128 z)
{
  FP_DECL_EX;
  FP_DECL_Q (X);
  FP_DECL_Q (Y);
  FP_DECL_Q (Z);
  FP_DECL_Q (R);
  FP_DECL_D (RN);
  double ret;

  FP_INIT_ROUNDMODE;
  FP_UNPACK_Q (X, x);
  FP_UNPACK_Q (Y, y);
  FP_UNPACK_Q (Z, z);
  FP_FMA_Q (R, X, Y, Z);
#if _FP_W_TYPE_SIZE < 64
  FP_TRUNC_COOKED (D, Q, 2, 4, RN, R);
#else
  FP_TRUNC_COOKED (D, Q, 1, 2, RN, R);
#endif
  FP_PACK_D (ret, RN);
  FP_HANDLE_EXCEPTIONS;
  CHECK_NARROW_FMA (ret, x, y, z);
  return ret;
}
DIAG_POP_NEEDS_COMMENT;

libm_alias_double_ldouble (fma)
