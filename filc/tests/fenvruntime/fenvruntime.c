#include <fenv.h>
#include <stdfil.h>
#include <stdio.h>

__attribute__((noinline))
static double div_volatile_d(volatile double a, volatile double b)
{
    return a / b;
}

__attribute__((noinline))
static long double div_volatile_ld(volatile long double a, volatile long double b)
{
    return a / b;
}

int main(void)
{
    ZASSERT(fegetround() == FE_TONEAREST);

    /* Verify fesetround updates hardware rounding for both double and long double. */
    ZASSERT(fesetround(FE_DOWNWARD) == 0);
    ZASSERT(fegetround() == FE_DOWNWARD);
    double d_down = div_volatile_d(1.0, 3.0);
    long double ld_down = div_volatile_ld(1.0L, 3.0L);

    ZASSERT(fesetround(FE_UPWARD) == 0);
    ZASSERT(fegetround() == FE_UPWARD);
    double d_up = div_volatile_d(1.0, 3.0);
    long double ld_up = div_volatile_ld(1.0L, 3.0L);

    ZASSERT(d_down < d_up);
    ZASSERT(ld_down < ld_up);

    ZASSERT(fesetround(FE_TOWARDZERO) == 0);
    ZASSERT(fegetround() == FE_TOWARDZERO);
    ZASSERT(fesetround(0x12345) != 0);

    /* Verify exception flags and fegetenv/fesetenv/FE_DFL_ENV roundtrip. */
    ZASSERT(fesetround(FE_UPWARD) == 0);
    ZASSERT(feclearexcept(FE_ALL_EXCEPT) == 0);
    ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);
    ZASSERT(feraiseexcept(FE_INEXACT | FE_OVERFLOW) == 0);
    ZASSERT(fetestexcept(FE_INEXACT | FE_OVERFLOW) == (FE_INEXACT | FE_OVERFLOW));

    fenv_t saved_up;
    ZASSERT(fegetenv(&saved_up) == 0);

    ZASSERT(fesetenv(FE_DFL_ENV) == 0);
    ZASSERT(fegetround() == FE_TONEAREST);
    ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);

    ZASSERT(fesetround(FE_DOWNWARD) == 0);
    ZASSERT(div_volatile_d(1.0, 3.0) == d_down);
    ZASSERT(div_volatile_ld(1.0L, 3.0L) == ld_down);

    ZASSERT(fesetenv(&saved_up) == 0);
    ZASSERT(fegetround() == FE_UPWARD);
    ZASSERT(fetestexcept(FE_INEXACT | FE_OVERFLOW) == (FE_INEXACT | FE_OVERFLOW));
    ZASSERT(div_volatile_d(1.0, 3.0) == d_up);
    ZASSERT(div_volatile_ld(1.0L, 3.0L) == ld_up);

    ZASSERT(fesetenv(FE_DFL_ENV) == 0);
    printf("fenv runtime ok\n");
    return 0;
}
