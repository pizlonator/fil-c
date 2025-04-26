/*
 * Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PIZLONATED_MATH_H
#define PIZLONATED_MATH_H

#include <stdfil.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} /* tell emacs what's up */
#endif

/* This file defines pizlonated math APIs. These are math functions that are best implemented in the
   Yolo libc and exposed as native functions to userland.

   There's no guarantee that the APIs in this file will be stable over time. */

int zmath_finitel(long double value);
long double zmath_scalbnl(long double value, int exp);
long double zmath_exp2l(long double value);
long double zmath_scalbl(long double value, long double exp);
long double zmath_exp10l(long double value);
long double zmath_expm1l(long double value);
long double zmath_expl(long double value);
long double zmath_fmodl(long double x, long double y);
long double zmath_ilogbl(long double value);
long double zmath_log10l(long double value);
long double zmath_log10l_finite(long double value);
long double zmath_log2l(long double value);
long double zmath_log2l_finite(long double value);
long double zmath_logl(long double value);
long double zmath_logl_finite(long double value);
long double zmath_powl(long double x, long double y);
long double zmath_remainderl(long double x, long double y);
long long zmath_llrintl(long double value);
long double zmath_log1pl(long double value);
long double zmath_nearbyintl(long double value);
unsigned zmath_getcw(void);
void zmath_setcw(unsigned cw);

#ifdef __cplusplus
}
#endif

#endif /* PIZLONATED_MATH_H */
