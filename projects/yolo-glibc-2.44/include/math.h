#ifndef	_MATH_H

#ifdef _ISOMAC
# undef NO_LONG_DOUBLE
#endif

#include <math/math.h>

#ifndef _ISOMAC
/* Now define the internal interfaces.  */
extern int __signgam;

# undef __MATHDECLX
# define __MATHDECLX(type, function,suffix, args, attrib) \
  __MATHDECL_1(type, function,suffix, args) __attribute__ (attrib); \
  __MATHDECL_1(type, __CONCAT(__,function),suffix, args) __attribute__ (attrib)

# if IS_IN (libc) || IS_IN (libm)
hidden_proto (__finite)
hidden_proto (__isinf)
hidden_proto (__isnan)
hidden_proto (__finitef)
hidden_proto (__isinff)
hidden_proto (__isnanf)

#  if !defined __NO_LONG_DOUBLE_MATH \
      && __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 0
hidden_proto (__finitel)
hidden_proto (__isinfl)
hidden_proto (__isnanl)
#  endif

#  if __HAVE_DISTINCT_FLOAT128
hidden_proto (__finitef128)
hidden_proto (__isinff128)
hidden_proto (__isnanf128)
#  endif
# endif

libm_hidden_proto (__fpclassify)
libm_hidden_proto (__fpclassifyf)
libm_hidden_proto (__issignaling)
libm_hidden_proto (__issignalingf)
libm_hidden_proto (__exp)
libm_hidden_proto (__expf)

#  if !defined __NO_LONG_DOUBLE_MATH \
      && __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 0
libm_hidden_proto (__fpclassifyl)
libm_hidden_proto (__issignalingl)
libm_hidden_proto (__expl)
libm_hidden_proto (__expm1l)
# endif

# if __HAVE_DISTINCT_FLOAT128
libm_hidden_proto (__fpclassifyf128)
libm_hidden_proto (__issignalingf128)
libm_hidden_proto (__expf128)
libm_hidden_proto (__expm1f128)
# endif

#include <stdint.h>
#include <nan-high-order-bit.h>

/* Get a 32 bit int from a float.  */
#ifndef GET_FLOAT_WORD
# define GET_FLOAT_WORD(__i, __d)				\
do {								\
  union { float f; uint32_t i; } u = { .f = (__d) };		\
  (__i) = u.i;							\
} while (0)
#endif

/* Set a float from a 32 bit int.  */
#ifndef SET_FLOAT_WORD
# define SET_FLOAT_WORD(__d, __i)				\
do {								\
  union { float f; uint32_t i; } u = { .i = (__i) };		\
  (__d) = u.f;							\
} while (0)
#endif

extern inline int
__issignalingf (float x)
{
  uint32_t xi;
  GET_FLOAT_WORD (xi, x);

  /* We only have to care about the high-order bit of x's significand, because
     having it set (sNaN) already makes the significand different from that
     used to designate infinity.  */
  if (HIGH_ORDER_BIT_IS_SET_FOR_SNAN)
    return (xi & 0x7fc00000) == 0x7fc00000;

  /* IEEE 754-2008 is_quiet flag is zero for signaling NaN.  To simplify the
     comparison logic, first toggle the flag, so that it is set for a sNaN.
     We shift out the sign bit and compare for greater than because xi's
     significand being all-zero means infinity, not sNaN.  */
  return 2 * (xi ^ 0x00400000) > 2 * 0x7fc00000U;
}

extern inline int
__issignaling (double x)
{
  union { double f; uint64_t i; } u = { .f = x };
  uint64_t xi = u.i;

  /* We only have to care about the high-order bit of x's significand, because
     having it set (sNaN) already makes the significand different from that
     used to designate infinity.  */
  if (HIGH_ORDER_BIT_IS_SET_FOR_SNAN)
    return (xi & UINT64_C (0x7ff8000000000000))
	    == UINT64_C (0x7ff8000000000000);

  /* IEEE 754-2008 is_quiet flag is zero for signaling NaN.  To simplify the
     comparison logic, first toggle the flag, so that it is set for a sNaN.
     We shift out the sign bit and compare for greater than because xi's
     significand being all-zero means infinity, not sNaN.  */
  return 2 * (xi ^ UINT64_C (0x0008000000000000))
	  > UINT64_C (0xfff0000000000000);
}

/* Get any long double optimization.  */
#include <math_ldbl.h>

# if __HAVE_DISTINCT_FLOAT128

#  ifdef __USE_EXTERN_INLINES

/* __builtin_isinf_sign is broken in GCC < 7 for float128.  */
#   if ! __GNUC_PREREQ (7, 0)
#    include <ieee754_float128.h>
extern inline int
__isinff128 (_Float128 x)
{
  int64_t hx, lx;
  GET_FLOAT128_WORDS64 (hx, lx, x);
  lx |= (hx & 0x7fffffffffffffffLL) ^ 0x7fff000000000000LL;
  lx |= -lx;
  return ~(lx >> 63) & (hx >> 62);
}
#   endif

extern inline _Float128
fabsf128 (_Float128 x)
{
  return __builtin_fabsf128 (x);
}
#  else
libm_hidden_proto (fabsf128)
#  endif
# endif


/* NB: Internal tests don't have access to internal symbols.  */
# if !IS_IN (testsuite_internal) \
     && !(defined __FINITE_MATH_ONLY__ && __FINITE_MATH_ONLY__ > 0)
/* NO_MATH_REDIRECT must be defined in the source implementing function,
   FUNC, if FUNC is implemented as an alias of __FUNC or vice versa to
   avoid redirecting FUNC to __FUNC.  */
# include <math-use-builtins.h>
/* NB: Do not redirect math builtin functions when they are inlined.  */
#  ifndef NO_MATH_REDIRECT
/* Declare some functions for use within GLIBC.  Compilers typically
   inline those functions as a single instruction.  Use an asm to
   avoid use of PLTs if it doesn't.  */
#   define MATH_REDIRECT(FUNC, PREFIX, ARGS)			\
  float (NO_ ## FUNC ## f ## _BUILTIN) (ARGS (float))		\
    asm (PREFIX #FUNC "f");					\
  double (NO_ ## FUNC ## _BUILTIN) (ARGS (double))		\
    asm (PREFIX #FUNC );					\
  MATH_REDIRECT_LDBL (FUNC, PREFIX, ARGS)			\
  MATH_REDIRECT_F128 (FUNC, PREFIX, ARGS)

#   if defined __NO_LONG_DOUBLE_MATH 				\
       || __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI == 1
#    define MATH_REDIRECT_LDBL(FUNC, PREFIX, ARGS)
#   else
#    define MATH_REDIRECT_LDBL(FUNC, PREFIX, ARGS)		  \
  long double (NO_ ## FUNC ## l ## _BUILTIN) (ARGS (long double)) \
    asm (PREFIX #FUNC "l");
#   endif
#   if __HAVE_DISTINCT_FLOAT128
#    define MATH_REDIRECT_F128(FUNC, PREFIX, ARGS)		 \
  _Float128 (NO_ ## FUNC ## f128 ## _BUILTIN) (ARGS (_Float128)) \
    asm (PREFIX #FUNC "f128");
#   else
#    define MATH_REDIRECT_F128(FUNC, PREFIX, ARGS)
#   endif
#   define MATH_REDIRECT_UNARY_ARGS(TYPE) TYPE
#   define MATH_REDIRECT_BINARY_ARGS(TYPE) TYPE, TYPE
#   define MATH_REDIRECT_TERNARY_ARGS(TYPE) TYPE, TYPE, TYPE
MATH_REDIRECT (sqrt, "__", MATH_REDIRECT_UNARY_ARGS)
MATH_REDIRECT (ceil, "__", MATH_REDIRECT_UNARY_ARGS)
MATH_REDIRECT (floor, "__", MATH_REDIRECT_UNARY_ARGS)
MATH_REDIRECT (roundeven, "__", MATH_REDIRECT_UNARY_ARGS)
MATH_REDIRECT (rint, "__", MATH_REDIRECT_UNARY_ARGS)
MATH_REDIRECT (trunc, "__", MATH_REDIRECT_UNARY_ARGS)
MATH_REDIRECT (round, "__", MATH_REDIRECT_UNARY_ARGS)
MATH_REDIRECT (copysign, "__", MATH_REDIRECT_BINARY_ARGS)
MATH_REDIRECT (fma, "__", MATH_REDIRECT_TERNARY_ARGS)
#  endif
# endif

#endif
#endif
