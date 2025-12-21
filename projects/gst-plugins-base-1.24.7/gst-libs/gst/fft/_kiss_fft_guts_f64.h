/*
 *  Copyright (c) 2003-2010, Mark Borgerding. All rights reserved.
 *  This file is part of KISS FFT - https://github.com/mborgerding/kissfft
 *
 *  SPDX-License-Identifier: BSD-3-Clause
 *  See COPYING file for more information.
 */

/* kiss_fft_f64.h
   defines kiss_fft_f64_scalar as either short or a float type
   and defines
   typedef struct { kiss_fft_f64_scalar r; kiss_fft_f64_scalar i; }kiss_fft_f64_cpx; */
#include "kiss_fft_f64.h"
#include <limits.h>

/* The 2*sizeof(size_t) alignment here is borrowed from
 * GNU libc, so it should be good most everywhere.
 * It is more conservative than is needed on some 64-bit
 * platforms, but ia64 does require a 16-byte alignment.
 * The SIMD extensions for x86 and ppc32 would want a
 * larger alignment than this, but we don't need to
 * do better than malloc.
 *
 * Borrowed from GLib's gobject/gtype.c
 */
#define STRUCT_ALIGNMENT (2 * sizeof (size_t))
#define ALIGN_STRUCT(offset) \
      ((offset + (STRUCT_ALIGNMENT - 1)) & -STRUCT_ALIGNMENT)

#define MAXFACTORS 32
/* e.g. an fft of length 128 has 4 factors 
 as far as kissfft is concerned
 4*4*4*2
 */

struct kiss_fft_f64_state{
    int nfft;
    int inverse;
    int factors[2*MAXFACTORS];
    kiss_fft_f64_cpx twiddles[1];
};

/*
  Explanation of macros dealing with complex math:

   C_MUL(m,a,b)         : m = a*b
   C_FIXDIV( c , div )  : if a fixed point impl., c /= div. noop otherwise
   C_SUB( res, a,b)     : res = a - b
   C_SUBFROM( res , a)  : res -= a
   C_ADDTO( res , a)    : res += a
 * */
#ifdef FIXED_POINT
#include <stdint.h>
#if (FIXED_POINT==32)
# define FRACBITS 31
# define SAMPPROD int64_t
#define SAMP_MAX INT32_MAX
#define SAMP_MIN INT32_MIN
#else
# define FRACBITS 15
# define SAMPPROD int32_t
#define SAMP_MAX INT16_MAX
#define SAMP_MIN INT16_MIN
#endif

#if defined(CHECK_OVERFLOW)
#  define CHECK_OVERFLOW_OP(a,op,b)  \
	if ( (SAMPPROD)(a) op (SAMPPROD)(b) > SAMP_MAX || (SAMPPROD)(a) op (SAMPPROD)(b) < SAMP_MIN ) { \
		g_critical("overflow @ " __FILE__ "(%d): (%d " #op" %d) = %ld",__LINE__,(a),(b),(SAMPPROD)(a) op (SAMPPROD)(b) );  }
#endif


#   define smul(a,b) ( (SAMPPROD)(a)*(b) )
#   define sround( x )  (kiss_fft_f64_scalar)( ( (x) + (1<<(FRACBITS-1)) ) >> FRACBITS )

#   define S_MUL(a,b) sround( smul(a,b) )

#   define C_MUL(m,a,b) \
      do{ (m).r = sround( smul((a).r,(b).r) - smul((a).i,(b).i) ); \
          (m).i = sround( smul((a).r,(b).i) + smul((a).i,(b).r) ); }while(0)

#   define DIVSCALAR(x,k) \
	(x) = sround( smul(  x, SAMP_MAX/k ) )

#   define C_FIXDIV(c,div) \
	do {    DIVSCALAR( (c).r , div);  \
		DIVSCALAR( (c).i  , div); }while (0)

#   define C_MULBYSCALAR( c, s ) \
    do{ (c).r =  sround( smul( (c).r , s ) ) ;\
        (c).i =  sround( smul( (c).i , s ) ) ; }while(0)

#else  /* not FIXED_POINT*/

#   define S_MUL(a,b) ( (a)*(b) )
#define C_MUL(m,a,b) \
    do{ (m).r = (a).r*(b).r - (a).i*(b).i;\
        (m).i = (a).r*(b).i + (a).i*(b).r; }while(0)
#   define C_FIXDIV(c,div) /* NOOP */
#   define C_MULBYSCALAR( c, s ) \
    do{ (c).r *= (s);\
        (c).i *= (s); }while(0)
#endif

#ifndef CHECK_OVERFLOW_OP
#  define CHECK_OVERFLOW_OP(a,op,b) /* noop */
#endif

#define  C_ADD( res, a,b)\
    do { \
	    CHECK_OVERFLOW_OP((a).r,+,(b).r)\
	    CHECK_OVERFLOW_OP((a).i,+,(b).i)\
	    (res).r=(a).r+(b).r;  (res).i=(a).i+(b).i; \
    }while(0)
#define  C_SUB( res, a,b)\
    do { \
	    CHECK_OVERFLOW_OP((a).r,-,(b).r)\
	    CHECK_OVERFLOW_OP((a).i,-,(b).i)\
	    (res).r=(a).r-(b).r;  (res).i=(a).i-(b).i; \
    }while(0)
#define C_ADDTO( res , a)\
    do { \
	    CHECK_OVERFLOW_OP((res).r,+,(a).r)\
	    CHECK_OVERFLOW_OP((res).i,+,(a).i)\
	    (res).r += (a).r;  (res).i += (a).i;\
    }while(0)

#define C_SUBFROM( res , a)\
    do {\
	    CHECK_OVERFLOW_OP((res).r,-,(a).r)\
	    CHECK_OVERFLOW_OP((res).i,-,(a).i)\
	    (res).r -= (a).r;  (res).i -= (a).i; \
    }while(0)


#ifdef FIXED_POINT
#  define KISS_FFT_F64_COS(phase)  floor(.5+SAMP_MAX * cos (phase))
#  define KISS_FFT_F64_SIN(phase)  floor(.5+SAMP_MAX * sin (phase))
#  define HALF_OF(x) ((x)>>1)
#elif defined(USE_SIMD)
#  define KISS_FFT_F64_COS(phase) _mm_set1_ps( cos(phase) )
#  define KISS_FFT_F64_SIN(phase) _mm_set1_ps( sin(phase) )
#  define HALF_OF(x) ((x)*_mm_set1_ps(.5))
#else
#  define KISS_FFT_F64_COS(phase) (kiss_fft_f64_scalar) cos(phase)
#  define KISS_FFT_F64_SIN(phase) (kiss_fft_f64_scalar) sin(phase)
#  define HALF_OF(x) ((x)*.5)
#endif

#define  kf_cexp(x,phase) \
	do{ \
		(x)->r = KISS_FFT_F64_COS(phase);\
		(x)->i = KISS_FFT_F64_SIN(phase);\
	}while(0)


/* a debugging function */
#define pcpx(c)\
    fprintf(stderr,"%g + %gi\n",(double)((c)->r),(double)((c)->i) )


#ifdef KISS_FFT_F64_USE_ALLOCA
// define this to allow use of alloca instead of malloc for temporary buffers
// Temporary buffers are used in two case: 
// 1. FFT sizes that have "bad" factors. i.e. not 2,3 and 5
// 2. "in-place" FFTs.  Notice the quotes, since kissfft does not really do an in-place transform.
#include <alloca.h>
#define  KISS_FFT_F64_TMP_ALLOC(nbytes) alloca(nbytes)
#define  KISS_FFT_F64_TMP_FREE(ptr) 
#else
#define  KISS_FFT_F64_TMP_ALLOC(nbytes) KISS_FFT_F64_MALLOC(nbytes)
#define  KISS_FFT_F64_TMP_FREE(ptr) KISS_FFT_F64_FREE(ptr)
#endif
