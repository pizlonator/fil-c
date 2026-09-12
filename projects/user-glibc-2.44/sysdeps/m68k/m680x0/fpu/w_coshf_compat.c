/* m68k provides an optimized __ieee754_coshhf.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_coshf_compat.c>
#else
# include <math-type-macros-float.h>
# include <w_cosh_template.c>
#endif
