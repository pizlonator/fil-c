/* e_jnf.c -- float version of e_jn.c.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include <errno.h>
#include <float.h>
#include <math.h>
#include <math-narrow-eval.h>
#include <math_private.h>
#include <fenv_private.h>
#include <math-underflow.h>
#include <libm-alias-finite.h>
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include "math_config.h"

static const float
two   =  2.0000000000e+00, /* 0x40000000 */
one   =  1.0000000000e+00; /* 0x3F800000 */

static const float zero  =  0.0000000000e+00;

float
__jnf(int n, float x)
{
    float ret;
    {
	int32_t i,hx,ix, sgn;
	float a, b, temp, di;
	float z, w;

    /* J(-n,x) = (-1)^n * J(n, x), J(n, -x) = (-1)^n * J(n, x)
     * Thus, J(-n,x) = J(n,-x)
     */
	GET_FLOAT_WORD(hx,x);
	ix = 0x7fffffff&hx;
    /* if J(n,NaN) is NaN */
	if(__builtin_expect(ix>0x7f800000, 0)) return x+x;
	if(n<0){
		n = -n;
		x = -x;
		hx ^= 0x80000000;
	}
	if(n==0) return(__ieee754_j0f(x));
	if(n==1) return(__ieee754_j1f(x));
	sgn = (n&1)&(hx>>31);	/* even n -- 0, odd n -- sign(x) */
	x = fabsf(x);
	SET_RESTORE_ROUNDF (FE_TONEAREST);
	if(__builtin_expect(ix==0||ix>=0x7f800000, 0))	/* if x is 0 or inf */
	    return sgn == 1 ? -zero : zero;
	else if((float)n<=x) {
		/* Safe to use J(n+1,x)=2n/x *J(n,x)-J(n-1,x) */
	    a = __ieee754_j0f(x);
	    b = __ieee754_j1f(x);
	    for(i=1;i<n;i++){
		temp = b;
                /* the following computation should be done in double precision */
		b = (double)b*((double)(i+i)/(double)x) - (double)a; /* avoid underflow */
		a = temp;
	    }
	} else {
	    if(ix<0x30800000) {	/* x < 2**-29 */
    /* x is tiny, return the first Taylor expansion of J(n,x)
     * J(n,x) = 1/n!*(x/2)^n  - ...
     */
		if(n>33)	/* underflow */
		    b = zero;
		else {
		    temp = x*(float)0.5; b = temp;
		    for (a=one,i=2;i<=n;i++) {
			a *= (float)i;		/* a = n! */
			b *= temp;		/* b = (x/2)^n */
		    }
		    b = b/a;
		}
	    } else {
		/* use backward recurrence */
		/*			x      x^2      x^2
		 *  J(n,x)/J(n-1,x) =  ----   ------   ------   .....
		 *			2n  - 2(n+1) - 2(n+2)
		 *
		 *			1      1        1
		 *  (for large x)   =  ----  ------   ------   .....
		 *			2n   2(n+1)   2(n+2)
		 *			-- - ------ - ------ -
		 *			 x     x         x
		 *
		 * Let w = 2n/x and h=2/x, then the above quotient
		 * is equal to the continued fraction:
		 *		    1
		 *	= -----------------------
		 *		       1
		 *	   w - -----------------
		 *			  1
		 *		w+h - ---------
		 *		       w+2h - ...
		 *
		 * To determine how many terms needed, let
		 * Q(0) = w, Q(1) = w(w+h) - 1,
		 * Q(k) = (w+k*h)*Q(k-1) - Q(k-2),
		 * When Q(k) > 1e4	good for single
		 * When Q(k) > 1e9	good for double
		 * When Q(k) > 1e17	good for quadruple
		 */
	    /* determine k */
		float t,v;
		float q0,q1,h,tmp; int32_t k,m;
		w  = (n+n)/(float)x; h = (float)2.0/(float)x;
		q0 = w;  z = w+h; q1 = w*z - (float)1.0; k=1;
		while(q1<(float)1.0e9) {
			k += 1; z += h;
			tmp = z*q1 - q0;
			q0 = q1;
			q1 = tmp;
		}
		m = n+n;
		for(t=zero, i = 2*(n+k); i>=m; i -= 2) t = one/(i/x-t);
		a = t;
		b = one;
		/*  estimate log((2/x)^n*n!) = n*log(2/x)+n*ln(n)
		 *  Hence, if n*(log(2n/x)) > ...
		 *  single 8.8722839355e+01
		 *  double 7.09782712893383973096e+02
		 *  long double 1.1356523406294143949491931077970765006170e+04
		 *  then recurrent value may overflow and the result is
		 *  likely underflow to zero
		 */
		tmp = n;
		v = two/x;
		tmp = tmp*__ieee754_logf(fabsf(v*tmp));
		if(tmp<8.8721679688e+01f) {
		    for(i=n-1,di=(float)(i+i);i>0;i--){
			temp = b;
			b *= di;
			b  = b/x - a;
			a = temp;
			di -= two;
		    }
		} else {
		    for(i=n-1,di=(float)(i+i);i>0;i--){
			temp = b;
			b *= di;
			b  = b/x - a;
			a = temp;
			di -= two;
		    /* scale b to avoid spurious overflow */
			if(b>(float)1e10) {
			    a /= b;
			    t /= b;
			    b  = one;
			}
		    }
		}
		/* j0() and j1() suffer enormous loss of precision at and
		 * near zero; however, we know that their zero points never
		 * coincide, so just choose the one further away from zero.
		 */
		z = __ieee754_j0f (x);
		w = __ieee754_j1f (x);
		if (fabsf (z) >= fabsf (w))
		  b = (t * z / b);
		else
		  b = (t * w / a);
	    }
	}
	if(sgn==1) ret = -b; else ret = b;
	ret = math_narrow_eval (ret);
    }
    if (ret == 0)
      {
	ret = math_narrow_eval (copysignf (FLT_MIN, ret) * FLT_MIN);
	__set_errno (ERANGE);
      }
    else
	math_check_force_underflow (ret);
    return ret;
}
strong_alias (__jnf, __ieee754_jnf)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __jnf, jnf, GLIBC_2_43);
libm_alias_float_other (__jn, jn)
#else
libm_alias_float (__jn, jn)
#endif
libm_alias_finite (__ieee754_jnf, __jnf)

float
__ynf(int n, float x)
{
    float ret;
    {
	int32_t i,hx,ix;
	uint32_t ib;
	int32_t sign;
	float a, b, temp;

	GET_FLOAT_WORD(hx,x);
	ix = 0x7fffffff&hx;
    /* if Y(n,NaN) is NaN */
	if(ix>=0x7f800000)
	  {
	    if (hx==0xFF800000)
	      return __math_invalidf (1);
	    return one/(x+x*x);
	  }
	sign = 1;
	if(n<0){
		n = -n;
		sign = 1 - ((n&1)<<1);
	}
	if(n==0) return(__ieee754_y0f(x));
	if(ix==0)
		return __math_divzerof (sign == 1 ? 1 : 0);
	if(hx<0) return __math_invalidf (x);
	SET_RESTORE_ROUNDF (FE_TONEAREST);
	if(n==1) {
	    ret = sign*__ieee754_y1f(x);
	    goto out;
	}
	if(__builtin_expect(ix==0x7f800000, 0)) return zero;

	a = __ieee754_y0f(x);
	b = __ieee754_y1f(x);
	/* quit if b is -inf */
	GET_FLOAT_WORD(ib,b);
	for(i=1;i<n&&ib!=0xff800000;i++){
	    temp = b;
            /* the following computation should be done in double precision */
	    b = ((double)(i+i)/(double)x)*(double)b - (double)a;
	    GET_FLOAT_WORD(ib,b);
	    a = temp;
	}
	/* If B is +-Inf, set up errno accordingly.  */
	if (! isfinite (b))
	  __set_errno (ERANGE);
	if(sign>0) ret = b; else ret = -b;
    }
 out:
    if (isinf (ret))
	ret = copysignf (FLT_MAX, ret) * FLT_MAX;
    return ret;
}
strong_alias (__ynf, __ieee754_ynf)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __ynf, ynf, GLIBC_2_43);
libm_alias_float_other (__yn, yn)
#else
libm_alias_float (__yn, yn)
#endif
libm_alias_finite (__ieee754_ynf, __ynf)
