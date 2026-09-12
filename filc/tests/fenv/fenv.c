/* Exercises the x86 floating-point environment functions that Qt reaches through qdtoa
   (feholdexcept/fesetenv when formatting a double), plus fegetenv and the FE_DFL_ENV sentinel.
   Under Fil-C these must run out of line in the runtime (zmath_feholdexcept/fegetenv/fesetenv)
   rather than through glibc's inline asm with pointer operands (fnstenv/fldenv/stmxcsr/ldmxcsr),
   which the instrumenter rejects -- calling them that way panics with "cannot handle inline asm". */

#include <fenv.h>
#include <stdio.h>
#include <string.h>
#include <stdfil.h>

int main(void)
{
    fenv_t saved, cur;

    /* feholdexcept: save the environment and mask/clear exceptions. */
    ZASSERT(feholdexcept(&saved) == 0);

    /* fegetenv: read the (now masked) environment back into another buffer. */
    ZASSERT(fegetenv(&cur) == 0);

    /* fesetenv with a real buffer restores a saved environment. */
    ZASSERT(fesetenv(&saved) == 0);

    /* fesetenv with the FE_DFL_ENV sentinel installs the default environment; this exercises the
       special-cased pointer value that must be recognised, not dereferenced as a buffer. */
    ZASSERT(fesetenv(FE_DFL_ENV) == 0);

    /* The path Qt actually hits: formatting a double runs qdtoa, which brackets its work with
       feholdexcept()/fesetenv(). If those still used the rejected inline asm this would panic
       rather than print, and the value would be wrong if the out-of-line versions misbehaved. */
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %.3f", 3.14159, 2.0 / 3.0);
    ZASSERT(!strcmp(buf, "3.14 0.667"));

    printf("fenv ok: %s\n", buf);
    return 0;
}
