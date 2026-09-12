/* Using math gcc builtins instead of generic implementation.  Generic version.
   Copyright (C) 2019-2026 Free Software Foundation, Inc.
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

#ifndef MATH_USE_BUILTINS_H
#define MATH_USE_BUILTINS_H	1

#include <features.h> /* For __GNUC_PREREQ.  */

/* Define these macros to 1 to use __builtin_xyz instead of the
   generic implementation.  */

#include <math-use-builtins-nearbyint.h>
#include <math-use-builtins-rint.h>
#include <math-use-builtins-floor.h>
#include <math-use-builtins-ceil.h>
#include <math-use-builtins-trunc.h>
#include <math-use-builtins-round.h>
#include <math-use-builtins-roundeven.h>
#include <math-use-builtins-copysign.h>
#include <math-use-builtins-sqrt.h>
#include <math-use-builtins-fma.h>
#include <math-use-builtins-fmax.h>
#include <math-use-builtins-fmin.h>
#include <math-use-builtins-fabs.h>
#include <math-use-builtins-lrint.h>
#include <math-use-builtins-llrint.h>
#include <math-use-builtins-logb.h>
#include <math-use-builtins-ffs.h>
#include <math-use-builtins-lround.h>
#include <math-use-builtins-llround.h>


/* Disable internal alias optimizations done at include/math.h if the
   compiler can expand the builtin for the symbol.  Different than gcc, the
   clang will always expand the alias before handling the builtin expansion,
   which makes the builtin expansion ineffective.  */

#if USE_SQRT_BUILTIN
# define NO_sqrt_BUILTIN inline_sqrt
#else
# define NO_sqrt_BUILTIN sqrt
#endif
#if USE_SQRTF_BUILTIN
# define NO_sqrtf_BUILTIN inline_sqrtf
#else
# define NO_sqrtf_BUILTIN sqrtf
#endif
#if USE_SQRTL_BUILTIN
# define NO_sqrtl_BUILTIN inline_sqrtl
#else
# define NO_sqrtl_BUILTIN sqrtl
#endif
#if USE_SQRTF128_BUILTIN
# define NO_sqrtf128_BUILTIN inline_sqrtf128
#else
# define NO_sqrtf128_BUILTIN sqrtf128
#endif


#if USE_CEIL_BUILTIN
# define NO_ceil_BUILTIN inline_ceil
#else
# define NO_ceil_BUILTIN ceil
#endif
#if USE_CEILF_BUILTIN
# define NO_ceilf_BUILTIN inline_ceilf
#else
# define NO_ceilf_BUILTIN ceilf
#endif
#if USE_CEILL_BUILTIN
# define NO_ceill_BUILTIN inline_ceill
#else
# define NO_ceill_BUILTIN ceill
#endif
#if USE_CEILF128_BUILTIN
# define NO_ceilf128_BUILTIN inline_ceilf128
#else
# define NO_ceilf128_BUILTIN ceilf128
#endif

#if USE_FLOOR_BUILTIN
# define NO_floor_BUILTIN inline_floor
#else
# define NO_floor_BUILTIN floor
#endif
#if USE_FLOORF_BUILTIN
# define NO_floorf_BUILTIN inline_floorf
#else
# define NO_floorf_BUILTIN floorf
#endif
#if USE_FLOORL_BUILTIN
# define NO_floorl_BUILTIN inline_floorl
#else
# define NO_floorl_BUILTIN floorl
#endif
#if USE_FLOORF128_BUILTIN
# define NO_floorf128_BUILTIN inline_floorf128
#else
# define NO_floorf128_BUILTIN floorf128
#endif

#if USE_ROUNDEVEN_BUILTIN
# define NO_roundeven_BUILTIN inline_roundeven
#else
# define NO_roundeven_BUILTIN roundeven
#endif
#if USE_ROUNDEVENF_BUILTIN
# define NO_roundevenf_BUILTIN inline_roundevenf
#else
# define NO_roundevenf_BUILTIN roundevenf
#endif
#if USE_ROUNDEVENL_BUILTIN
# define NO_roundevenl_BUILTIN inline_roundevenl
#else
# define NO_roundevenl_BUILTIN roundevenl
#endif
#if USE_ROUNDEVENF128_BUILTIN
# define NO_roundevenf128_BUILTIN inline_roundevenf128
#else
# define NO_roundevenf128_BUILTIN roundevenf128
#endif

#if USE_RINT_BUILTIN
# define NO_rint_BUILTIN inline_rint
#else
# define NO_rint_BUILTIN rint
#endif
#if USE_RINTF_BUILTIN
# define NO_rintf_BUILTIN inline_rintf
#else
# define NO_rintf_BUILTIN rintf
#endif
#if USE_RINTL_BUILTIN
# define NO_rintl_BUILTIN inline_rintl
#else
# define NO_rintl_BUILTIN rintl
#endif
#if USE_RINTF128_BUILTIN
# define NO_rintf128_BUILTIN inline_rintf128
#else
# define NO_rintf128_BUILTIN rintf128
#endif

#if USE_TRUNC_BUILTIN
# define NO_trunc_BUILTIN inline_trunc
#else
# define NO_trunc_BUILTIN trunc
#endif
#if USE_TRUNCF_BUILTIN
# define NO_truncf_BUILTIN inline_truncf
#else
# define NO_truncf_BUILTIN truncf
#endif
#if USE_TRUNCL_BUILTIN
# define NO_truncl_BUILTIN inline_truncl
#else
# define NO_truncl_BUILTIN truncl
#endif
#if USE_TRUNCF128_BUILTIN
# define NO_truncf128_BUILTIN inline_truncf128
#else
# define NO_truncf128_BUILTIN truncf128
#endif

#if USE_ROUND_BUILTIN
# define NO_round_BUILTIN inline_round
#else
# define NO_round_BUILTIN round
#endif
#if USE_ROUNDF_BUILTIN
# define NO_roundf_BUILTIN inline_roundf
#else
# define NO_roundf_BUILTIN roundf
#endif
#if USE_ROUNDL_BUILTIN
# define NO_roundl_BUILTIN inline_roundl
#else
# define NO_roundl_BUILTIN roundl
#endif
#if USE_ROUNDF128_BUILTIN
# define NO_roundf128_BUILTIN inline_roundf128
#else
# define NO_roundf128_BUILTIN roundf128
#endif

#if USE_COPYSIGN_BUILTIN
# define NO_copysign_BUILTIN inline_copysign
#else
# define NO_copysign_BUILTIN copysign
#endif
#if USE_COPYSIGNF_BUILTIN
# define NO_copysignf_BUILTIN inline_copysignf
#else
# define NO_copysignf_BUILTIN copysignf
#endif
#if USE_COPYSIGNL_BUILTIN
# define NO_copysignl_BUILTIN inline_copysignl
#else
# define NO_copysignl_BUILTIN copysignl
#endif
#if USE_COPYSIGNF128_BUILTIN
# define NO_copysignf128_BUILTIN inline_copysignf128
#else
# define NO_copysignf128_BUILTIN copysignf128
#endif

#if USE_FMA_BUILTIN
# define NO_fma_BUILTIN inline_fma
#else
# define NO_fma_BUILTIN fma
#endif
#if USE_FMAF_BUILTIN
# define NO_fmaf_BUILTIN inline_fmaf
#else
# define NO_fmaf_BUILTIN fmaf
#endif
#if USE_FMAL_BUILTIN
# define NO_fmal_BUILTIN inline_fmal
#else
# define NO_fmal_BUILTIN fmal
#endif
#if USE_FMAF128_BUILTIN
# define NO_fmaf128_BUILTIN inline_fmaf128
#else
# define NO_fmaf128_BUILTIN fmaf128
#endif

#endif /* MATH_USE_BUILTINS_H  */
