/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "TestHarness.h"
#include "pas_utils.h"
#if !PAS_OS(WINDOWS)
#include <fenv.h>
#endif

using namespace std;

namespace {

void testIsDivisibleBy3(unsigned value)
{
    static const uint64_t magic_constant = PAS_IS_DIVISIBLE_BY_MAGIC_CONSTANT(3);
    CHECK_EQUAL(pas_is_divisible_by(value, magic_constant),
                !(value % 3));
}

#if !PAS_OS(WINDOWS)
__attribute__((noinline))
double divVolatileDouble(volatile double a, volatile double b)
{
    return a / b;
}

__attribute__((noinline))
long double divVolatileLongDouble(volatile long double a, volatile long double b)
{
    return a / b;
}

void testFloatEnvironmentIsolation()
{
    /* 1/7 rounds downward under FE_TONEAREST in binary64 (53-bit), x87 80-bit
       (64-bit), and binary128 (113-bit), so FE_TONEAREST < FE_UPWARD holds
       for both double and long double across x86_64 and AArch64. */
    CHECK_EQUAL(fesetenv(FE_DFL_ENV), 0);
    double dNear = divVolatileDouble(1.0, 7.0);
    long double ldNear = divVolatileLongDouble(1.0L, 7.0L);

    CHECK_EQUAL(fesetround(FE_UPWARD), 0);
    double dUp = divVolatileDouble(1.0, 7.0);
    long double ldUp = divVolatileLongDouble(1.0L, 7.0L);
    CHECK_LESS(dNear, dUp);
    CHECK_LESS(ldNear, ldUp);

    CHECK_EQUAL(feclearexcept(FE_ALL_EXCEPT), 0);
    CHECK_EQUAL(feraiseexcept(FE_OVERFLOW), 0);
    CHECK_EQUAL(fegetround(), FE_UPWARD);
    CHECK_EQUAL(fetestexcept(FE_ALL_EXCEPT), FE_OVERFLOW);

    pas_saved_float_environment saved;
    pas_save_float_environment(&saved);
    /* Saving alone must not alter active rounding mode or exception flags. */
    CHECK_EQUAL(fegetround(), FE_UPWARD);
    CHECK_EQUAL(fetestexcept(FE_ALL_EXCEPT), FE_OVERFLOW);

    pas_set_float_environment_to_libpas_default();
    CHECK_EQUAL(fegetround(), FE_TONEAREST);
    CHECK_EQUAL(fetestexcept(FE_ALL_EXCEPT), 0);
    CHECK_EQUAL(divVolatileDouble(1.0, 7.0), dNear);
    CHECK_EQUAL(divVolatileLongDouble(1.0L, 7.0L), ldNear);
    CHECK_EQUAL(feraiseexcept(FE_INVALID | FE_DIVBYZERO), 0);
    CHECK(fetestexcept(FE_INEXACT | FE_INVALID | FE_DIVBYZERO)
          == (FE_INEXACT | FE_INVALID | FE_DIVBYZERO));

    pas_restore_float_environment(&saved);
    /* Runtime rounding and raised flags must not leak into user state. */
    CHECK_EQUAL(fegetround(), FE_UPWARD);
    CHECK_EQUAL(fetestexcept(FE_ALL_EXCEPT), FE_OVERFLOW);
    CHECK_EQUAL(divVolatileDouble(1.0, 7.0), dUp);
    CHECK_EQUAL(divVolatileLongDouble(1.0L, 7.0L), ldUp);

    CHECK_EQUAL(fesetenv(FE_DFL_ENV), 0);
}
#endif

} // anonymous namespace

void addUtilsTests()
{
    ADD_TEST(testIsDivisibleBy3(0));
    ADD_TEST(testIsDivisibleBy3(1));
    ADD_TEST(testIsDivisibleBy3(2));
    ADD_TEST(testIsDivisibleBy3(3));
    ADD_TEST(testIsDivisibleBy3(4));
    ADD_TEST(testIsDivisibleBy3(5));
    ADD_TEST(testIsDivisibleBy3(6));
    ADD_TEST(testIsDivisibleBy3(7));
    ADD_TEST(testIsDivisibleBy3(8));
    ADD_TEST(testIsDivisibleBy3(9));
    ADD_TEST(testIsDivisibleBy3(10));
    ADD_TEST(testIsDivisibleBy3(11));
    ADD_TEST(testIsDivisibleBy3(12));
    ADD_TEST(testIsDivisibleBy3(13));
    ADD_TEST(testIsDivisibleBy3(14));
    ADD_TEST(testIsDivisibleBy3(15));
    ADD_TEST(testIsDivisibleBy3(16));
    ADD_TEST(testIsDivisibleBy3(17));
    ADD_TEST(testIsDivisibleBy3(18));
    ADD_TEST(testIsDivisibleBy3(19));
    ADD_TEST(testIsDivisibleBy3(20));
    ADD_TEST(testIsDivisibleBy3(21));
    ADD_TEST(testIsDivisibleBy3(22));
    ADD_TEST(testIsDivisibleBy3(23));
    ADD_TEST(testIsDivisibleBy3(24));
    ADD_TEST(testIsDivisibleBy3(25));
    ADD_TEST(testIsDivisibleBy3(26));
    ADD_TEST(testIsDivisibleBy3(27));
#if !PAS_OS(WINDOWS)
    ADD_TEST(testFloatEnvironmentIsolation());
#endif
}
