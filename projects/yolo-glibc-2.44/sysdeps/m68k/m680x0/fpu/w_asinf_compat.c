/* m68k provides an optimized __ieee754_asinf.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_asinf_compat.c>
#else
# include <math-type-macros-float.h>
# include <w_asin_template.c>
#endif
