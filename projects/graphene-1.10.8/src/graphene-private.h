/* graphene-private.h: Private header
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2014  Emmanuele Bassi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "config.h"
#ifdef HAVE_INIT_ONCE
#define _WIN32_WINNT 0x0600
#include <windows.h>
#endif
#include "graphene-macros.h"
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define GRAPHENE_FLOAT_EPSILON  FLT_EPSILON

#ifndef MIN
# if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#  define MIN(a,b) \
  __extension__({ \
    __auto_type _a = (a); \
    __auto_type _b = (b); \
    _a < _b ? _a : _b; \
  })
# else
#  define MIN(a,b) ((a) < (b) ? (a) : (b))
# endif
#endif

#ifndef MAX
# if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#  define MAX(a,b) \
  __extension__({ \
    __auto_type _a = (a); \
    __auto_type _b = (b); \
    _a > _b ? _a : _b; \
  })
# else
#  define MAX(a,b) ((a) > (b) ? (a) : (b))
# endif
#endif

#ifndef CLAMP
# if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#  define CLAMP(v,min,max) \
  __extension__({ \
    __auto_type _v = (v); \
    __auto_type _min = (min); \
    __auto_type _max = (max); \
    _v < _min ? _min : (_v > _max ? _max : _v); \
  })
# else
#  define CLAMP(v,min,max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))
# endif
#endif

#if defined(__GNUC__) && __GNUC__ > 3
# define likely(x)      (__builtin_expect((x) ? 1 : 0, 1))
# define unlikely(x)    (__builtin_expect((x) ? 1 : 0, 0))
#else
# define likely(x)      (x)
# define unlikely(x)    (x)
#endif

#define GRAPHENE_DEG_TO_RAD(x)          ((x) * (GRAPHENE_PI / 180.f))
#define GRAPHENE_RAD_TO_DEG(x)          ((x) * (180.f / GRAPHENE_PI))

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
/* Use typeof on GCC */
# define graphene_fuzzy_equals(n1,n2,epsilon) \
  __extension__({ \
    __auto_type _n1 = (n1); \
    __auto_type _n2 = (n2); \
    __auto_type _epsilon = (epsilon); \
    (bool) ((_n1 > _n2 ? (_n1 - _n2) : (_n2 - _n1)) < _epsilon); \
  })

#else
/* fallback for Visual Studio, typeof not supported */
# define graphene_fuzzy_equals(n1,n2,epsilon) \
  (((n1) > (n2) ? ((n1) - (n2)) : ((n2) - (n1))) < (epsilon))

#endif /* __GNUC__ */

#if defined(_M_X64) && ! defined(isnanf)
# define graphene_isnan(x) _isnanf(x)
#elif defined(HAVE_ISNANF)
# define graphene_isnan(x) isnanf(x)
#else
# define graphene_isnan(x) isnan(x)
#endif

static inline bool
graphene_approx_val (float a, float b)
{
  return graphene_fuzzy_equals (a, b, FLT_EPSILON);
}

static inline float
graphene_flerpf (float a, float b, double factor)
{
  return (float) (((1.0 - factor) * (double) a) + (factor * (double) b));
}

static inline double
graphene_flerp (double a, double b, double factor)
{
  return ((1.0 - factor) * a) + (factor * b);
}

static inline int
graphene_ilerp (int a, int b, double factor)
{
  return (int) (((1.0 - factor) * (double) a) + (factor * (double) b));
}

#if defined(__GNUC__) && \
  ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9)) && \
  (__STDC_VERSION__ >= 201112L || !defined(__STRICT_ANSI__)) && \
  !defined(__cplusplus)
# define USE_C11_GENERIC 1
#endif

#ifdef USE_C11_GENERIC
# define graphene_lerp(a,b,f) \
  _Generic((a), \
    double: graphene_flerp, \
    float: graphene_flerpf, \
    int: graphene_ilerp, \
    default: graphene_flerp) ((a), (b), (f))

#else
# define graphene_lerp   graphene_flerpf
#endif

static inline void
graphene_sincos (float angle, float *sin_out, float *cos_out)
{
#ifdef HAVE_SINCOSF
  sincosf (angle, sin_out, cos_out);
#else
  if (sin_out != NULL)
    *sin_out = sinf (angle);
  if (cos_out != NULL)
    *cos_out = cosf (angle);
#endif /* HAVE_SINCOSF */
}

typedef bool (* graphene_compare_func_t) (const void *p1, const void *p2);

static inline bool
graphene_pointer_equal (const void *p1,
                        const void *p2,
                        graphene_compare_func_t func)
{
  if (p1 == p2)
    return true;

  if (p1 == NULL || p2 == NULL)
    return false;

  if (func == NULL)
    return true;

  return func (p1, p2);
}
