/* Floating-point remainder function.
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

#include <libm-alias-finite.h>
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include <math.h>
#include "math_config.h"

/* With x = mx * 2^ex and y = my * 2^ey (mx, my, ex, ey being integers), the
   simplest implementation is:

   mx * 2^ex == 2 * mx * 2^(ex - 1)

   or

   while (ex > ey)
     {
       mx *= 2;
       --ex;
       mx %= my;
     }

   With the mathematical equivalence of:

   r == x % y == (x % (N * y)) % y

   And with mx/my being mantissa of a single floating point number (which uses
   less bits than the storage type), on each step the argument reduction can
   be improved by 8 (which is the size of uint32_t minus MANTISSA_WIDTH plus
   the implicit one bit):

   mx * 2^ex == 2^8 * mx * 2^(ex - 8)

   or

   while (ex > ey)
     {
       mx << 8;
       ex -= 8;
       mx %= my;
     }

   Special cases:
     - If x or y is a NaN, a NaN is returned.
     - If x is an infinity, or y is zero, a NaN is returned and EDOM is set.
     - If x is +0/-0, and y is not zero, +0/-0 is returned.  */

float
__fmodf (float x, float y)
{
  uint32_t hx = asuint (x);
  uint32_t hy = asuint (y);

  uint32_t sx = hx & SIGN_MASK;
  /* Get |x| and |y|.  */
  hx ^= sx;
  hy &= ~SIGN_MASK;

  if (__glibc_likely (hx < hy))
    {
      /* If y is a NaN, return a NaN.  */
      if (__glibc_unlikely (hy > EXPONENT_MASK))
	return x * y;
      return x;
    }

  int ex = hx >> MANTISSA_WIDTH;
  int ey = hy >> MANTISSA_WIDTH;
  int exp_diff = ex - ey;

  /* Common case where exponents are close: |x/y| < 2^9, x not inf/NaN
     and |x%y| not denormal.  */
  if (__glibc_likely (ey < (EXPONENT_MASK >> MANTISSA_WIDTH) - EXPONENT_WIDTH
		     && ey > MANTISSA_WIDTH
		     && exp_diff <= EXPONENT_WIDTH))
    {
      uint32_t mx = (hx << EXPONENT_WIDTH) | SIGN_MASK;
      uint32_t my = (hy << EXPONENT_WIDTH) | SIGN_MASK;

      mx %= (my >> exp_diff);

      if (__glibc_unlikely (mx == 0))
	return asfloat (sx);
      int shift = __builtin_clz (mx);
      ex -= shift + 1;
      mx <<= shift;
      mx = sx | (mx >> EXPONENT_WIDTH);
      return asfloat (mx + ((uint32_t)ex << MANTISSA_WIDTH));
    }

  if (__glibc_unlikely (hy == 0 || hx >= EXPONENT_MASK))
    {
      /* If x is a NaN, return a NaN.  */
      if (hx > EXPONENT_MASK)
	return x * y;

      /* If x is an infinity or y is zero, return a NaN and set EDOM.  */
      return __math_edomf ((x * y) / (x * y));
    }

  /* Special case, both x and y are denormal.  */
  if (__glibc_unlikely (ex == 0))
    return asfloat (sx | hx % hy);

  /* Extract normalized mantissas - hx is not denormal and hy != 0.  */
  uint32_t mx = get_mantissa (hx) | (MANTISSA_MASK + 1);
  uint32_t my = get_mantissa (hy) | (MANTISSA_MASK + 1);
  int lead_zeros_my = EXPONENT_WIDTH;

  ey--;
  /* Special case for denormal y.  */
  if (__glibc_unlikely (ey < 0))
    {
      my = hy;
      ey = 0;
      exp_diff--;
      lead_zeros_my = __builtin_clz (my);
    }

  int tail_zeros_my = __builtin_ctz (my);
  int sides_zeroes = lead_zeros_my + tail_zeros_my;

  int right_shift = exp_diff < tail_zeros_my ? exp_diff : tail_zeros_my;
  my >>= right_shift;
  exp_diff -= right_shift;
  ey += right_shift;

  int left_shift = exp_diff < EXPONENT_WIDTH ? exp_diff : EXPONENT_WIDTH;
  mx <<= left_shift;
  exp_diff -= left_shift;

  mx %= my;

  if (__glibc_unlikely (mx == 0))
    return asfloat (sx);

  if (exp_diff == 0)
    return make_float (mx, ey, sx);

  /* Multiplication with the inverse is faster than repeated modulo.  */
  uint32_t inv_hy = UINT32_MAX / my;
  while (exp_diff > sides_zeroes) {
    exp_diff -= sides_zeroes;
    uint32_t hd = (mx * inv_hy) >> (BIT_WIDTH - sides_zeroes);
    mx <<= sides_zeroes;
    mx -= hd * my;
    while (__glibc_unlikely (mx > my))
      mx -= my;
  }
  uint32_t hd = (mx * inv_hy) >> (BIT_WIDTH - exp_diff);
  mx <<= exp_diff;
  mx -= hd * my;
  while (__glibc_unlikely (mx > my))
    mx -= my;

  return make_float (mx, ey, sx);
}
strong_alias (__fmodf, __ieee754_fmodf)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __fmodf, fmodf, GLIBC_2_38);
libm_alias_float_other (__fmod, fmod)
#else
libm_alias_float (__fmod, fmod)
#endif
libm_alias_finite (__ieee754_fmodf, __fmodf)
