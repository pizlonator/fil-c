#include <math.h>
#include <stdfil.h>
#include "utils.h"

int main()
{
    ZASSERT(sin(0.) == 0.);
    __builtin_set_flt_rounds((int)opaque((void*)__builtin_flt_rounds()));
    ZASSERT(acosl(0.) > 1.57);
    ZASSERT(acosl(0.) < 1.6);
    ZASSERT(acoshf(1.5) > 0.96);
    ZASSERT(acoshf(1.5) < 0.97);
    ZASSERT(acoshl(1.5) > 0.96);
    ZASSERT(acoshl(1.5) < 0.97);
    ZASSERT(atan2l(2., 3.) > 0.588);
    ZASSERT(atan2l(2., 3.) < 0.589);
    ZASSERT(fmodl(3., 2.) == 1.);
    ZASSERT(atanl(2.) > 1.107);
    ZASSERT(atanl(2.) < 1.1072);
    ZASSERT(logbl(2.) == 1.);
    ZASSERT(rintl(2.2) == 2.);
    ZASSERT(significandl(2.2) > 1.09);
    ZASSERT(significandl(2.2) < 1.11);
    ZASSERT(asinl(.5) > 0.523);
    ZASSERT(asinl(.5) < 0.524);
    return 0;
}

