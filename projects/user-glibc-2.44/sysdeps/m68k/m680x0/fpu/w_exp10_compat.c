/* m68k provides an optimized __ieee754_exp10.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_exp10_compat.c>
#else
# include <math-type-macros-double.h>
# include <w_exp10_template.c>
#endif
