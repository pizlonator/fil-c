/* m68k provides an optimized __ieee754_log10f.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_log10f_compat.c>
#else
# include <math-type-macros-float.h>
# include <w_log10_template.c>
#endif
