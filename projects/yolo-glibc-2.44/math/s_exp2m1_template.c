/* Return exp2(X) - 1.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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
#include <fenv.h>
#include <math.h>
#include <math_private.h>
#include <math-underflow.h>

FLOAT
M_DECL_FUNC (__exp2m1) (FLOAT x)
{
  if (isgreaterequal (x, M_LIT (-1.0)) && islessequal (x, M_LIT (1.0)))
    {
      FLOAT ret = M_SUF (__expm1) (M_MLIT (M_LN2) * x);
      math_check_force_underflow (ret);
      if (x != 0 && ret == 0)
	__set_errno (ERANGE);
      return ret;
    }
  else if (isgreater (x, M_MANT_DIG + M_LIT (2.0)))
    {
#if defined FE_DOWNWARD || defined FE_TOWARDZERO
      /* exp2m1 (MAX_EXP) should not overflow in these two rounding
	 modes, but exp2 does overflow.  */
      if (x == M_MAX_EXP)
	{
	  int rnd_mode = fegetround ();
	  if (0
# ifdef FE_DOWNWARD
	      || rnd_mode == FE_DOWNWARD
# endif
# ifdef FE_TOWARDZERO
	      || rnd_mode == FE_TOWARDZERO
# endif
	      )
	    return M_MAX;
	}
#endif
      FLOAT ret = M_SUF (__ieee754_exp2) (x);
      if (!isfinite (ret) && isfinite (x))
	__set_errno (ERANGE);
      return ret;
    }
  else if (isless (x, -M_MANT_DIG - M_LIT (2.0)))
    return M_LIT (-1.0);
  else
    return M_SUF (__ieee754_exp2) (x) - M_LIT (1.0);
}
declare_mgen_alias (__exp2m1, exp2m1);
