/* Fused multiply-add of double values, narrowing the result to float,
   using soft-fp.
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

#define f32fmaf64 __hide_f32fmaf64
#define f32fmaf32x __hide_f32fmaf32x
#define ffmal __hide_ffmal
#include <math.h>
#undef f32fmaf64
#undef f32fmaf32x
#undef ffmal

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
#include <single.h>
#include <double.h>

float
__ffma (double x, double y, double z)
{
  FP_DECL_EX;
  FP_DECL_D (X);
  FP_DECL_D (Y);
  FP_DECL_D (Z);
  FP_DECL_D (R);
  FP_DECL_S (RN);
  float ret;

  FP_INIT_ROUNDMODE;
  FP_UNPACK_D (X, x);
  FP_UNPACK_D (Y, y);
  FP_UNPACK_D (Z, z);
  FP_FMA_D (R, X, Y, Z);
#if _FP_W_TYPE_SIZE < _FP_FRACBITS_D
  FP_TRUNC_COOKED (S, D, 1, 2, RN, R);
#else
  FP_TRUNC_COOKED (S, D, 1, 1, RN, R);
#endif
  FP_PACK_S (ret, RN);
  FP_HANDLE_EXCEPTIONS;
  CHECK_NARROW_FMA (ret, x, y, z);
  return ret;
}
DIAG_POP_NEEDS_COMMENT;

libm_alias_float_double (fma)
