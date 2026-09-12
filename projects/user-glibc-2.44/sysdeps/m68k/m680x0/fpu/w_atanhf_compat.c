/* m68k provides an optimized __ieee754_atanhf.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_atanhf_compat.c>
#else
# include <math-type-macros-float.h>
# include <w_atanh_template.c>
#endif
