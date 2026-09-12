/* Math inline asm compat layer
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

#ifndef _MATH_INLINE_ASM
#define _MATH_INLINE_ASM

#include <sys/cdefs.h>

/* clang does not support the %v to select the AVX encoding, nor the '%d' asm
   contrain, and for AVX build it requires all 3 arguments.  */
#ifdef __clang__
#if defined __AVX__
#  define VPREFIX    "v"
#  define VROUND_ARG ", %0"
# else
#  define VPREFIX    ""
#  define VROUND_ARG ""
# endif
# define VARGPREFIX  "%"
#else
# define VPREFIX     "%v"
# define VARGPREFIX  "%d"
# define VROUND_ARG  ""
#endif

__extern_always_inline double
trunc_inline_asm (double x)
{
  asm (VPREFIX "roundsd $11, " VARGPREFIX "1, %0" VROUND_ARG : "=v" (x)
       : "v" (x));
  return x;
}

__extern_always_inline float
truncf_inline_asm (float x)
{
  asm (VPREFIX "roundss $11, " VARGPREFIX "1, %0" VROUND_ARG : "=v" (x)
       : "v" (x));
  return x;
}

static __always_inline void
stmxcsr_inline_asm (unsigned int *mxcsr)
{
  asm volatile (VPREFIX "stmxcsr %0" : "=m" (*mxcsr));
}

static __always_inline void
ldmxcsr_inline_asm (unsigned int *mxcsr)
{
  asm volatile (VPREFIX "ldmxcsr %0" : : "m" (*mxcsr));
}

static __always_inline float
divss_inline_asm (float x, float y)
{
  asm volatile (VPREFIX "divss %1, " VARGPREFIX "0" VROUND_ARG
		: "+x" (x) : "x" (y));
  return x;
}

static __always_inline double
fmod_inline (double x, double y)
{
#if HAVE_X86_INLINE_FMOD
  return __builtin_fmod (x, y);
#else
  double result;
  asm ("1:\n"
       "fprem\n"
       "fnstsw	%%ax\n"
       "sahf\n"
       "jp	1b\n"
       : "=t" (result)
       : "0" (x), "u" (y)
       : "ax", "cc"
    );
  return result;
#endif
}

static __always_inline float
fmodf_inline (float x, float y)
{
#if HAVE_X86_INLINE_FMOD
  return __builtin_fmodf (x, y);
#else
  float result;
  asm ("1:\n"
       "fprem\n"
       "fnstsw	%%ax\n"
       "sahf\n"
       "jp	1b\n"
       : "=t" (result)
       : "0" (x), "u" (y)
       : "ax", "cc"
    );
  return result;
#endif
}

static __always_inline double
remainder_inline (double x, double y)
{
#if HAVE_X86_INLINE_FMOD
   return __builtin_remainder (x, y);
#else
  double result;
  asm ("1:\n"
       "fprem1\n"
       "fnstsw	%%ax\n"
       "sahf\n"
       "jp	1b\n"
       : "=t" (result)
       : "0" (x), "u" (y)
       : "ax", "cc"
    );
  return result;
#endif
}

static __always_inline float
remainderf_inline (float x, float y)
{
#if HAVE_X86_INLINE_FMOD
   return __builtin_remainderf (x, y);
#else
  float result;
  asm ("1:\n"
       "fprem1\n"
       "fnstsw	%%ax\n"
       "sahf\n"
       "jp	1b\n"
       : "=t" (result)
       : "0" (x), "u" (y)
       : "ax", "cc"
    );
  return result;
#endif
}

#endif
