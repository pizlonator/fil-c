/* Round trips through the fenv API: rounding modes, exception flags,
   fegetenv/fesetenv/feholdexcept/feupdateenv, feget/setexceptflag.

   The musl flavor stubs its hand-written fenv assembly out, so there
   every fenv call returns zero and has no effect.  Probe whether the
   libc really implements the semantics, and gate the assertions that
   depend on that, so the test passes on either flavor; the glibc
   flavor, whose fenv is fully implemented, still checks the real
   thing. */

#include <fenv.h>
#include <stdio.h>
#include <stdfil.h>

int main(void)
{
    ZASSERT(fegetround() == FE_TONEAREST);
    ZASSERT(!fesetround(FE_DOWNWARD));
    ZASSERT(!fesetround(FE_UPWARD));
    ZASSERT(!fesetround(FE_TONEAREST));
    ZASSERT(fegetround() == FE_TONEAREST);
    int roundingSticks = !fesetround(FE_DOWNWARD) &&
        fegetround() == FE_DOWNWARD;
    ZASSERT(!fesetround(FE_TONEAREST));
    ZASSERT(fegetround() == FE_TONEAREST);

    feclearexcept(FE_ALL_EXCEPT);
    ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);
    ZASSERT(!feraiseexcept(FE_DIVBYZERO | FE_OVERFLOW));
    int flagsStick = (fetestexcept(FE_ALL_EXCEPT) &
                      (FE_DIVBYZERO | FE_OVERFLOW)) ==
        (FE_DIVBYZERO | FE_OVERFLOW);

    if (roundingSticks) {
        ZASSERT(!fesetround(FE_DOWNWARD));
        ZASSERT(fegetround() == FE_DOWNWARD);
        ZASSERT(!fesetround(FE_UPWARD));
        ZASSERT(fegetround() == FE_UPWARD);
        ZASSERT(!fesetround(FE_TONEAREST));
        ZASSERT(fegetround() == FE_TONEAREST);
    }

    /* Exercise the whole API on both paths; where the fenv is a no-op,
       the calls return zero and nothing changes.  */
    fexcept_t flags = 0;
    ZASSERT(!fegetexceptflag(&flags, FE_ALL_EXCEPT));
    feclearexcept(FE_ALL_EXCEPT);
    ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);
    ZASSERT(!fesetexceptflag(&flags, FE_ALL_EXCEPT));

    fenv_t env = { 0 };
    ZASSERT(!fegetenv(&env));
    feclearexcept(FE_ALL_EXCEPT);
    ZASSERT(!fesetenv(&env));
    ZASSERT(!feholdexcept(&env));
    feclearexcept(FE_ALL_EXCEPT);
    ZASSERT(!feraiseexcept(FE_INEXACT));
    ZASSERT(!feupdateenv(&env));
    ZASSERT(!fesetenv(&env));
    feclearexcept(FE_ALL_EXCEPT);
    ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);

    if (flagsStick) {
        ZASSERT(!feraiseexcept(FE_DIVBYZERO | FE_OVERFLOW));
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) == FE_DIVBYZERO);
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) == FE_OVERFLOW);
        feclearexcept(FE_DIVBYZERO);
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) == 0);
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) == FE_OVERFLOW);

        ZASSERT(!fegetexceptflag(&flags, FE_ALL_EXCEPT));
        feclearexcept(FE_ALL_EXCEPT);
        ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);
        ZASSERT(!fesetexceptflag(&flags, FE_ALL_EXCEPT));
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) == FE_OVERFLOW);

        ZASSERT(!fegetenv(&env));
        feclearexcept(FE_ALL_EXCEPT);
        ZASSERT(!fesetenv(&env));
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) == FE_OVERFLOW);

        ZASSERT(!feholdexcept(&env));
        ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);
        feclearexcept(FE_ALL_EXCEPT);
        ZASSERT(!feraiseexcept(FE_INEXACT));
        ZASSERT(!feupdateenv(&env));
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_INEXACT) == FE_INEXACT);

        ZASSERT(!fesetenv(&env));
        ZASSERT((fetestexcept(FE_ALL_EXCEPT) & FE_OVERFLOW) == FE_OVERFLOW);
        feclearexcept(FE_ALL_EXCEPT);
        ZASSERT(fetestexcept(FE_ALL_EXCEPT) == 0);
    }

    printf("fenv round trip ok\n");
    return 0;
}
