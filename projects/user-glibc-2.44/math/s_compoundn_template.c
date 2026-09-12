/* Return (1+X)^Y for integer Y.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <fenv_private.h>
#include <limits.h>
#include <math.h>
#include <math-barriers.h>
#include <math-narrow-eval.h>
#include <math_private.h>
#include <stdlib.h>


/* Calculate X + Y exactly and store the result in *HI + *LO.  It is
   given that |X| >= |Y| and the values are small enough that no
   overflow occurs.  */

static inline void
add_split (FLOAT *hi, FLOAT *lo, FLOAT x, FLOAT y)
{
  /* Apply Dekker's algorithm.  */
  *hi = math_narrow_eval (x + y);
  *lo = (x - *hi) + y;
}


/* Store floating-point values that add up to A * (B + C + D) in
   OUT[0] through OUT[5].  It is given that no overflow or underflow
   can occur.  */

static inline void
mul3_split (FLOAT *out, FLOAT a, FLOAT b, FLOAT c, FLOAT d)
{
  out[0] = a * b;
  out[1] = M_SUF (fma) (a, b, -out[0]);
  out[2] = a * c;
  out[3] = M_SUF (fma) (a, c, -out[2]);
  out[4] = a * d;
  out[5] = M_SUF (fma) (a, d, -out[4]);
}


/* Compare absolute values of floating-point values pointed to by P
   and Q for qsort.  */

static int
compare (const void *p, const void *q)
{
  FLOAT pd = M_FABS (*(const FLOAT *) p);
  FLOAT qd = M_FABS (*(const FLOAT *) q);
  if (pd < qd)
    return -1;
  else if (pd == qd)
    return 0;
  else
    return 1;
}

FLOAT
M_DECL_FUNC (__compoundn) (FLOAT x, long long int y)
{
  FLOAT ret;
  if (issignaling (x))
    return x + x;
  if (isless (x, -M_LIT (1.0)))
    {
      __set_errno (EDOM);
      return (x - x) / (x - x);
    }
  if (y == 0)
    return M_LIT (1.0);
  if (isnan (x))
    return x;
  if (x == -M_LIT (1.0))
    {
      if (y > 0)
	return M_LIT (0.0);
      else
	{
	  __set_errno (ERANGE);
	  return M_LIT (1.0) / M_LIT (0.0);
	}
    }
  if (isinf (x))
    return y > 0 ? x : M_LIT (0.0);
  if (y == 1)
    {
      /* Ensure overflow in FE_UPWARD mode when X is the largest
	 positive finite value.  */
      ret = math_narrow_eval (M_LIT (1.0) + x);
      if (isinf (ret))
	__set_errno (ERANGE);
      return ret;
    }
  /* Now we know X is finite and greater than -1, and Y is not 0 or
     1.  */
  {
    M_SET_RESTORE_ROUND (FE_TONEAREST);
    x = math_opt_barrier (x);
    /* Split 1 + X into high and low parts, where the sign of the low
       part is the same as the sign of X to avoid possible spurious
       intermediate overflow or underflow later.  */
    FLOAT xhi, xlo;
    if (x >= M_LIT (1.0))
      add_split (&xhi, &xlo, x, M_LIT (1.0));
    else
      add_split (&xhi, &xlo, M_LIT (1.0), x);
    if (xlo != M_LIT (0.0) && !!signbit (xlo) != !!signbit (x))
      {
	FLOAT xhi_n = (signbit (x)
		       ? M_SUF (__nextup) (xhi)
		       : M_SUF (__nextdown) (xhi));
	xlo += xhi - xhi_n;
	xhi = xhi_n;
      }
    ret = math_narrow_eval (M_SUF (__pown) (xhi, y));
    /* The result is RET * (1 + XLO/XHI)^Y.  Evaluate XLO/XHI with
       extra precision.  If XLO/XHI is sufficiently small, the extra
       factor is not needed and internal underflow should be avoided.
       If the calculation of RET has overflowed or underflowed, avoid
       calculations of the extra factor that might end up as
       Inf*0.  */
    _Static_assert (-M_MANT_DIG - 65 > M_MIN_EXP,
		    "no underflow from dividing EPSILON by long long");
    if (!isinf (ret)
	&& ret != M_LIT (0.0)
	&& (xhi >= M_LIT (1.0)
	    ? M_FABS (xlo) >= xhi * (M_EPSILON * M_LIT (0x1p-65))
	    : xhi <= M_FABS (xlo) / (M_EPSILON * M_LIT (0x1p-65))))
      {
	FLOAT qhi, qlo, nqhi2;
	qhi = xlo / xhi;
	FLOAT xlo_rem = M_SUF (fma) (-qhi, xhi, xlo);
	if (xhi >= M_LIT (1.0)
	    ? M_FABS (xlo_rem) >= xhi * (M_EPSILON * M_LIT (0x1p-65))
	    : xhi <= M_FABS (xlo_rem) / (M_EPSILON * M_LIT (0x1p-65)))
	  qlo = xlo_rem / xhi;
	else
	  qlo = M_LIT (0.0);
	if (M_FABS (qhi) >= M_EPSILON * M_LIT (0x1p-33))
	  nqhi2 = qhi * qhi * -M_LIT (0.5);
	else
	  nqhi2 = M_LIT (0.0);
	/* To sufficient precision, log1p(XLO/XHI) is QHI + QLO + NQHI2.  */
#define NUM_PARTS ((LLONG_WIDTH + M_MANT_DIG - 1) / M_MANT_DIG)
	_Static_assert (NUM_PARTS <= 3,
			"long long fits in at most three FLOATs");
	FLOAT parts[NUM_PARTS * 6];
	FLOAT ypart;
	ypart = y;
	mul3_split (parts, ypart, qhi, qlo, nqhi2);
#if NUM_PARTS >= 2
	y -= ypart;
	ypart = y;
	mul3_split (parts + 6, ypart, qhi, qlo, nqhi2);
#endif
#if NUM_PARTS >= 3
	y -= ypart;
	ypart = y;
	mul3_split (parts + 12, ypart, qhi, qlo, nqhi2);
#endif
	qsort (parts, NUM_PARTS * 6, sizeof (FLOAT), compare);
	/* Add up the values so that each element of PARTS has
	   absolute value at most equal to the last set bit of the
	   next nonzero element.  */
	for (size_t i = 0; i <= NUM_PARTS * 6 - 2; i++)
	  {
	    add_split (&parts[i + 1], &parts[i], parts[i + 1], parts[i]);
	    qsort (parts + i + 1, NUM_PARTS * 6 - 1 - i, sizeof (FLOAT),
		   compare);
	  }
	/* Add up the values in the other direction, so that each
	   element of PARTS has absolute value less than NUM_PARTS * 3
	   ulp of the next value.  */
	size_t dstpos = NUM_PARTS * 6 - 1;
	for (size_t i = 1; i <= NUM_PARTS * 6 - 1; i++)
	  {
	    if (parts[dstpos] == M_LIT (0.0))
	      {
		parts[dstpos] = parts[NUM_PARTS * 6 - 1 - i];
		parts[NUM_PARTS * 6 - 1 - i] = M_LIT (0.0);
	      }
	    else
	      {
		add_split (&parts[dstpos], &parts[NUM_PARTS * 6 - 1 - i],
			   parts[dstpos], parts[NUM_PARTS * 6 - 1 - i]);
		if (parts[NUM_PARTS * 6 - 1 - i] != M_LIT (0.0))
		  {
		    if (NUM_PARTS * 6 - 1 - i < dstpos - 1)
		      {
			parts[dstpos - 1] = parts[NUM_PARTS * 6 - 1 - i];
			parts[NUM_PARTS * 6 - 1 - i] = M_LIT (0.0);
		      }
		    dstpos--;
		  }
	      }
	  }
	ret *= (M_SUF (__exp) (parts[NUM_PARTS * 6 - 1])
		* M_SUF (__exp) (parts[NUM_PARTS * 6 - 2]));
      }
    ret = math_narrow_eval (ret);
    math_force_eval (ret);
  }
  if (isinf (ret))
    ret = math_narrow_eval (M_MAX * M_MAX);
  else if (ret == M_LIT (0.0))
    ret = math_narrow_eval (M_MIN * M_MIN);
  if (isinf (ret) || ret == M_LIT (0.0))
    __set_errno (ERANGE);
  return ret;
}
declare_mgen_alias (__compoundn, compoundn);
