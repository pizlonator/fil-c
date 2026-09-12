/* Configuration for math routines.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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

#ifndef _MATH_CONFIG_H
#define _MATH_CONFIG_H

#include <math.h>
#include <math_private.h>
#include <nan-high-order-bit.h>
#include <stdint.h>
#include <stdbit.h>

#ifndef WANT_ROUNDING
/* Correct special case results in non-nearest rounding modes.  */
# define WANT_ROUNDING 1
#endif
#ifndef WANT_ERRNO
/* Set errno according to ISO C with (math_errhandling & MATH_ERRNO) != 0.  */
# define WANT_ERRNO 1
#endif
#ifndef WANT_ERRNO_UFLOW
/* Set errno to ERANGE if result underflows to 0 (in all rounding modes).  */
# define WANT_ERRNO_UFLOW (WANT_ROUNDING && WANT_ERRNO)
#endif

#ifndef TOINT_INTRINSICS
/* When set, the roundtoint and converttoint functions are provided with
   the semantics documented below.  */
# define TOINT_INTRINSICS 0
#endif

#if TOINT_INTRINSICS
/* Round x to nearest int in all rounding modes, ties have to be rounded
   consistently with converttoint so the results match.  If the result
   would be outside of [-2^31, 2^31-1] then the semantics is unspecified.  */
static inline double
roundtoint (double x);

/* Convert x to nearest int in all rounding modes, ties have to be rounded
   consistently with roundtoint.  If the result is not representible in an
   int32_t then the semantics is unspecified.  */
static inline int32_t
converttoint (double x);
#endif

#ifndef ROUNDEVEN_INTRINSICS
/* When set, roundeven_finite will route to the internal roundeven function.  */
# define ROUNDEVEN_INTRINSICS 1
#endif

/* Round x to nearest integer value in floating-point format, rounding halfway
  cases to even.  If the input is non finite the result is unspecified.  */
static inline double
roundeven_finite (double x)
{
  if (!isfinite (x))
    __builtin_unreachable ();
#if ROUNDEVEN_INTRINSICS
  return roundeven (x);
#else
  double y = round (x);
  if (fabs (x - y) == 0.5)
    {
      union { double f; uint64_t i; } u = {y};
      union { double f; uint64_t i; } v = {y - copysign (1.0, x)};
      if (stdc_trailing_zeros (v.i) > stdc_trailing_zeros (u.i))
        y = v.f;
    }
  return y;
#endif
}

#ifndef ROUNDEVENF_INTRINSICS
/* When set, roundevenf_finite will route to the internal roundevenf function.  */
# define ROUNDEVENF_INTRINSICS 1
#endif

static inline float
roundevenf_finite (float x)
{
  if (!isfinite (x))
    __builtin_unreachable ();
#if ROUNDEVENF_INTRINSICS
  return roundevenf (x);
#else
  float y = roundf (x);
  if (fabs (x - y) == 0.5)
    {
      union { float f; uint32_t i; } u = {y};
      union { float f; uint32_t i; } v = {y - copysignf (1.0f, x)};
      if (stdc_trailing_zeros (v.i) > stdc_trailing_zeros (u.i))
        y = v.f;
    }
  return y;
#endif
}

static inline uint32_t
asuint (float f)
{
  union
  {
    float f;
    uint32_t i;
  } u = {f};
  return u.i;
}

static inline float
asfloat (uint32_t i)
{
  union
  {
    uint32_t i;
    float f;
  } u = {i};
  return u.f;
}

static inline uint64_t
asuint64 (double f)
{
  union
  {
    double f;
    uint64_t i;
  } u = {f};
  return u.i;
}

static inline double
asdouble (uint64_t i)
{
  union
  {
    uint64_t i;
    double f;
  } u = {i};
  return u.f;
}

#define BIT_WIDTH       32
#define MANTISSA_WIDTH  23
#define EXPONENT_WIDTH  8
#define EXPONENT_BIAS   127
#define MANTISSA_MASK   UINT32_C (0x007fffff)
#define EXPONENT_MASK   UINT32_C (0x7f800000)
#define EXP_MANT_MASK   UINT32_C (0x7fffffff)
#define QUIET_NAN_MASK  UINT32_C (0x00400000)
#define SIGN_MASK       UINT32_C (0x80000000)

static inline bool
is_nan (uint32_t x)
{
  return (x & EXP_MANT_MASK) > EXPONENT_MASK;
}

static inline bool
is_inf (uint32_t x)
{
  return (x << 1) == (EXPONENT_MASK << 1);
}

static inline uint32_t
get_mantissa (uint32_t x)
{
  return x & MANTISSA_MASK;
}

static inline int
get_exponent (uint32_t x)
{
  return (int)((x >> MANTISSA_WIDTH & 0xff) - EXPONENT_BIAS);
}

/* Convert integer number X, unbiased exponent EP, and sign S to double:

   result = X * 2^(EP+1 - exponent_bias)

   NB: zero is not supported.  */
static inline double
make_float (uint32_t x, int ep, uint32_t s)
{
  int lz = __builtin_clz (x) - EXPONENT_WIDTH;
  x <<= lz;
  ep -= lz;

  if (__glibc_unlikely (ep < 0 || x == 0))
    {
      x >>= -ep;
      ep = 0;
    }
  return asfloat (s + x + (ep << MANTISSA_WIDTH));
}

attribute_hidden float __math_oflowf (uint32_t);
attribute_hidden float __math_uflowf (uint32_t);
attribute_hidden float __math_may_uflowf (uint32_t);
attribute_hidden float __math_divzerof (uint32_t);
attribute_hidden float __math_invalidf (float);
attribute_hidden int __math_invalidf_i (int);
attribute_hidden long int __math_invalidf_li (long int);
attribute_hidden float __math_edomf (float x);
attribute_hidden float __math_erangef (float x);

/* Shared between expf, exp2f, exp10f, and powf.  */
#define EXP2F_TABLE_BITS 5
#define EXP2F_POLY_ORDER 3
extern const struct exp2f_data
{
  uint64_t tab[1 << EXP2F_TABLE_BITS];
  double shift_scaled;
  double poly[EXP2F_POLY_ORDER];
  double invln2_scaled;
  double poly_scaled[EXP2F_POLY_ORDER];
  double shift;
} __exp2f_data attribute_hidden;

#define LOGF_TABLE_BITS 4
#define LOGF_POLY_ORDER 4
extern const struct logf_data
{
  struct
  {
    double invc, logc;
  } tab[1 << LOGF_TABLE_BITS];
  double ln2;
  double poly[LOGF_POLY_ORDER - 1]; /* First order coefficient is 1.  */
} __logf_data attribute_hidden;

#define LOG2F_TABLE_BITS 4
#define LOG2F_POLY_ORDER 4
extern const struct log2f_data
{
  struct
  {
    double invc, logc;
  } tab[1 << LOG2F_TABLE_BITS];
  double poly[LOG2F_POLY_ORDER];
} __log2f_data attribute_hidden;

#define POWF_LOG2_TABLE_BITS 4
#define POWF_LOG2_POLY_ORDER 5
#if TOINT_INTRINSICS
# define POWF_SCALE_BITS EXP2F_TABLE_BITS
#else
# define POWF_SCALE_BITS 0
#endif
#define POWF_SCALE ((double) (1 << POWF_SCALE_BITS))
extern const struct powf_log2_data
{
  struct
  {
    double invc, logc;
  } tab[1 << POWF_LOG2_TABLE_BITS];
  double poly[POWF_LOG2_POLY_ORDER];
} __powf_log2_data attribute_hidden;

#endif
