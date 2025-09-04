/* Helper macros for functions returning a narrower type.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
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

#ifndef	_MATH_NARROW_H
#define	_MATH_NARROW_H	1

#include <bits/floatn.h>
#include <bits/long-double.h>
#include <errno.h>
#include <fenv.h>
#include <ieee754.h>
#include <math-barriers.h>
#include <math_private.h>
#include <fenv_private.h>
#include <math-narrow-alias.h>
#include <stdbool.h>

/* Carry out a computation using round-to-odd.  The computation is
   EXPR; the union type in which to store the result is UNION and the
   subfield of the "ieee" field of that union with the low part of the
   mantissa is MANTISSA; SUFFIX is the suffix for both underlying libm
   functions for the argument type (for computations where a libm
   function rather than a C operator is used when argument and result
   types are the same) and the libc_fe* macros to ensure that the
   correct rounding mode is used, for platforms with multiple rounding
   modes where those macros set only the relevant mode.
   CLEAR_UNDERFLOW indicates whether underflow exceptions must be
   cleared (in the case where a round-toward-zero underflow might not
   indicate an underflow after narrowing, when that narrowing only
   reduces precision not exponent range and the architecture uses
   before-rounding tininess detection).  This macro does not work
   correctly if the sign of an exact zero result depends on the
   rounding mode, so that case must be checked for separately.  */
#define ROUND_TO_ODD(EXPR, UNION, SUFFIX, MANTISSA, CLEAR_UNDERFLOW)	\
  ({									\
    fenv_t env;								\
    UNION u;								\
									\
    libc_feholdexcept_setround ## SUFFIX (&env, FE_TOWARDZERO);		\
    u.d = (EXPR);							\
    math_force_eval (u.d);						\
    if (CLEAR_UNDERFLOW)						\
      feclearexcept (FE_UNDERFLOW);					\
    u.ieee.MANTISSA							\
      |= libc_feupdateenv_test ## SUFFIX (&env, FE_INEXACT) != 0;	\
									\
    u.d;								\
  })

/* Check for error conditions from a narrowing add function returning
   RET with arguments X and Y and set errno as needed.  Overflow and
   underflow can occur for finite arguments and a domain error for
   infinite ones.  */
#define CHECK_NARROW_ADD(RET, X, Y)			\
  do							\
    {							\
      if (!isfinite (RET))				\
	{						\
	  if (isnan (RET))				\
	    {						\
	      if (!isnan (X) && !isnan (Y))		\
		__set_errno (EDOM);			\
	    }						\
	  else if (isfinite (X) && isfinite (Y))	\
	    __set_errno (ERANGE);			\
	}						\
      else if ((RET) == 0 && (X) != -(Y))		\
	__set_errno (ERANGE);				\
    }							\
  while (0)

/* Implement narrowing add using round-to-odd.  The arguments are X
   and Y, the return type is TYPE and UNION, MANTISSA and SUFFIX are
   as for ROUND_TO_ODD.  */
#define NARROW_ADD_ROUND_TO_ODD(X, Y, TYPE, UNION, SUFFIX, MANTISSA)	\
  do									\
    {									\
      TYPE ret;								\
									\
      /* Ensure a zero result is computed in the original rounding	\
	 mode.  */							\
      if ((X) == -(Y))							\
	ret = (TYPE) ((X) + (Y));					\
      else								\
	ret = (TYPE) ROUND_TO_ODD (math_opt_barrier (X) + (Y),		\
				   UNION, SUFFIX, MANTISSA, false);	\
									\
      CHECK_NARROW_ADD (ret, (X), (Y));					\
      return ret;							\
    }									\
  while (0)

/* Implement a narrowing add function that is not actually narrowing
   or where no attempt is made to be correctly rounding (the latter
   only applies to IBM long double).  The arguments are X and Y and
   the return type is TYPE.  */
#define NARROW_ADD_TRIVIAL(X, Y, TYPE)		\
  do						\
    {						\
      TYPE ret;					\
						\
      ret = (TYPE) ((X) + (Y));			\
      CHECK_NARROW_ADD (ret, (X), (Y));		\
      return ret;				\
    }						\
  while (0)

/* Check for error conditions from a narrowing subtract function
   returning RET with arguments X and Y and set errno as needed.
   Overflow and underflow can occur for finite arguments and a domain
   error for infinite ones.  */
#define CHECK_NARROW_SUB(RET, X, Y)			\
  do							\
    {							\
      if (!isfinite (RET))				\
	{						\
	  if (isnan (RET))				\
	    {						\
	      if (!isnan (X) && !isnan (Y))		\
		__set_errno (EDOM);			\
	    }						\
	  else if (isfinite (X) && isfinite (Y))	\
	    __set_errno (ERANGE);			\
	}						\
      else if ((RET) == 0 && (X) != (Y))		\
	__set_errno (ERANGE);				\
    }							\
  while (0)

/* Implement narrowing subtract using round-to-odd.  The arguments are
   X and Y, the return type is TYPE and UNION, MANTISSA and SUFFIX are
   as for ROUND_TO_ODD.  */
#define NARROW_SUB_ROUND_TO_ODD(X, Y, TYPE, UNION, SUFFIX, MANTISSA)	\
  do									\
    {									\
      TYPE ret;								\
									\
      /* Ensure a zero result is computed in the original rounding	\
	 mode.  */							\
      if ((X) == (Y))							\
	ret = (TYPE) ((X) - (Y));					\
      else								\
	ret = (TYPE) ROUND_TO_ODD (math_opt_barrier (X) - (Y),		\
				   UNION, SUFFIX, MANTISSA, false);	\
									\
      CHECK_NARROW_SUB (ret, (X), (Y));					\
      return ret;							\
    }									\
  while (0)

/* Implement a narrowing subtract function that is not actually
   narrowing or where no attempt is made to be correctly rounding (the
   latter only applies to IBM long double).  The arguments are X and Y
   and the return type is TYPE.  */
#define NARROW_SUB_TRIVIAL(X, Y, TYPE)		\
  do						\
    {						\
      TYPE ret;					\
						\
      ret = (TYPE) ((X) - (Y));			\
      CHECK_NARROW_SUB (ret, (X), (Y));		\
      return ret;				\
    }						\
  while (0)

/* Check for error conditions from a narrowing multiply function
   returning RET with arguments X and Y and set errno as needed.
   Overflow and underflow can occur for finite arguments and a domain
   error for Inf * 0.  */
#define CHECK_NARROW_MUL(RET, X, Y)			\
  do							\
    {							\
      if (!isfinite (RET))				\
	{						\
	  if (isnan (RET))				\
	    {						\
	      if (!isnan (X) && !isnan (Y))		\
		__set_errno (EDOM);			\
	    }						\
	  else if (isfinite (X) && isfinite (Y))	\
	    __set_errno (ERANGE);			\
	}						\
      else if ((RET) == 0 && (X) != 0 && (Y) != 0)	\
	__set_errno (ERANGE);				\
    }							\
  while (0)

/* Implement narrowing multiply using round-to-odd.  The arguments are
   X and Y, the return type is TYPE and UNION, MANTISSA, SUFFIX and
   CLEAR_UNDERFLOW are as for ROUND_TO_ODD.  */
#define NARROW_MUL_ROUND_TO_ODD(X, Y, TYPE, UNION, SUFFIX, MANTISSA,	\
				CLEAR_UNDERFLOW)			\
  do									\
    {									\
      TYPE ret;								\
									\
      ret = (TYPE) ROUND_TO_ODD (math_opt_barrier (X) * (Y),		\
				 UNION, SUFFIX, MANTISSA,		\
				 CLEAR_UNDERFLOW);			\
									\
      CHECK_NARROW_MUL (ret, (X), (Y));					\
      return ret;							\
    }									\
  while (0)

/* Implement a narrowing multiply function that is not actually
   narrowing or where no attempt is made to be correctly rounding (the
   latter only applies to IBM long double).  The arguments are X and Y
   and the return type is TYPE.  */
#define NARROW_MUL_TRIVIAL(X, Y, TYPE)		\
  do						\
    {						\
      TYPE ret;					\
						\
      ret = (TYPE) ((X) * (Y));			\
      CHECK_NARROW_MUL (ret, (X), (Y));		\
      return ret;				\
    }						\
  while (0)

/* Check for error conditions from a narrowing divide function
   returning RET with arguments X and Y and set errno as needed.
   Overflow, underflow and divide-by-zero can occur for finite
   arguments and a domain error for Inf / Inf and 0 / 0.  */
#define CHECK_NARROW_DIV(RET, X, Y)			\
  do							\
    {							\
      if (!isfinite (RET))				\
	{						\
	  if (isnan (RET))				\
	    {						\
	      if (!isnan (X) && !isnan (Y))		\
		__set_errno (EDOM);			\
	    }						\
	  else if (isfinite (X))			\
	    __set_errno (ERANGE);			\
	}						\
      else if ((RET) == 0 && (X) != 0 && !isinf (Y))	\
	__set_errno (ERANGE);				\
    }							\
  while (0)

/* Implement narrowing divide using round-to-odd.  The arguments are X
   and Y, the return type is TYPE and UNION, MANTISSA, SUFFIX and
   CLEAR_UNDERFLOW are as for ROUND_TO_ODD.  */
#define NARROW_DIV_ROUND_TO_ODD(X, Y, TYPE, UNION, SUFFIX, MANTISSA,	\
				CLEAR_UNDERFLOW)			\
  do									\
    {									\
      TYPE ret;								\
									\
      ret = (TYPE) ROUND_TO_ODD (math_opt_barrier (X) / (Y),		\
				 UNION, SUFFIX, MANTISSA,		\
				 CLEAR_UNDERFLOW);			\
									\
      CHECK_NARROW_DIV (ret, (X), (Y));					\
      return ret;							\
    }									\
  while (0)

/* Implement a narrowing divide function that is not actually
   narrowing or where no attempt is made to be correctly rounding (the
   latter only applies to IBM long double).  The arguments are X and Y
   and the return type is TYPE.  */
#define NARROW_DIV_TRIVIAL(X, Y, TYPE)		\
  do						\
    {						\
      TYPE ret;					\
						\
      ret = (TYPE) ((X) / (Y));			\
      CHECK_NARROW_DIV (ret, (X), (Y));		\
      return ret;				\
    }						\
  while (0)

/* Check for error conditions from a narrowing square root function
   returning RET with argument X and set errno as needed.  Overflow
   and underflow can occur for finite positive arguments and a domain
   error for negative arguments.  */
#define CHECK_NARROW_SQRT(RET, X)		\
  do						\
    {						\
      if (!isfinite (RET))			\
	{					\
	  if (isnan (RET))			\
	    {					\
	      if (!isnan (X))			\
		__set_errno (EDOM);		\
	    }					\
	  else if (isfinite (X))		\
	    __set_errno (ERANGE);		\
	}					\
      else if ((RET) == 0 && (X) != 0)		\
	__set_errno (ERANGE);			\
    }						\
  while (0)

/* Implement narrowing square root using round-to-odd.  The argument
   is X, the return type is TYPE and UNION, MANTISSA and SUFFIX are as
   for ROUND_TO_ODD.  */
#define NARROW_SQRT_ROUND_TO_ODD(X, TYPE, UNION, SUFFIX, MANTISSA)	\
  do									\
    {									\
      TYPE ret;								\
									\
      ret = (TYPE) ROUND_TO_ODD (sqrt ## SUFFIX (math_opt_barrier (X)),	\
				 UNION, SUFFIX, MANTISSA, false);	\
									\
      CHECK_NARROW_SQRT (ret, (X));					\
      return ret;							\
    }									\
  while (0)

/* Implement a narrowing square root function where no attempt is made
   to be correctly rounding (this only applies to IBM long double; the
   case where the function is not actually narrowing is handled by
   aliasing other sqrt functions in libm, not using this macro).  The
   argument is X and the return type is TYPE.  */
#define NARROW_SQRT_TRIVIAL(X, TYPE, SUFFIX)	\
  do						\
    {						\
      TYPE ret;					\
						\
      ret = (TYPE) (sqrt ## SUFFIX (X));	\
      CHECK_NARROW_SQRT (ret, (X));		\
      return ret;				\
    }						\
  while (0)

/* Check for error conditions from a narrowing fused multiply-add
   function returning RET with arguments X, Y and Z and set errno as
   needed.  Checking for error conditions for fma (either narrowing or
   not) and setting errno is not currently implemented.  See bug
   6801.  */
#define CHECK_NARROW_FMA(RET, X, Y, Z)		\
  do						\
    {						\
    }						\
  while (0)

/* Implement narrowing fused multiply-add using round-to-odd.  The
   arguments are X, Y and Z, the return type is TYPE and UNION,
   MANTISSA, SUFFIX and CLEAR_UNDERFLOW are as for ROUND_TO_ODD.  */
#define NARROW_FMA_ROUND_TO_ODD(X, Y, Z, TYPE, UNION, SUFFIX, MANTISSA, \
				CLEAR_UNDERFLOW)			\
  do									\
    {									\
      typeof (X) tmp;							\
      TYPE ret;								\
									\
      tmp = ROUND_TO_ODD (fma ## SUFFIX (math_opt_barrier (X), (Y),	\
					 (Z)),				\
			  UNION, SUFFIX, MANTISSA, CLEAR_UNDERFLOW);	\
      /* If the round-to-odd result is zero, the result is an exact	\
	 zero and must be recomputed in the original rounding mode.  */ \
      if (tmp == 0)							\
	ret = (TYPE) (math_opt_barrier (X) * (Y) + (Z));		\
      else								\
	ret = (TYPE) tmp;						\
									\
      CHECK_NARROW_FMA (ret, (X), (Y), (Z));				\
      return ret;							\
    }									\
  while (0)

/* Implement a narrowing fused multiply-add function where no attempt
   is made to be correctly rounding (this only applies to IBM long
   double; the case where the function is not actually narrowing is
   handled by aliasing other fma functions in libm, not using this
   macro).  The arguments are X, Y and Z and the return type is
   TYPE.  */
#define NARROW_FMA_TRIVIAL(X, Y, Z, TYPE, SUFFIX)	\
  do							\
    {							\
      TYPE ret;						\
							\
      ret = (TYPE) (fma ## SUFFIX ((X), (Y), (Z)));	\
      CHECK_NARROW_FMA (ret, (X), (Y), (Z));		\
      return ret;					\
    }							\
  while (0)

#endif /* math-narrow.h.  */
