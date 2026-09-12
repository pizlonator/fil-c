/* m68k provides an optimized __ieee754_atan2f.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_atan2f_compat.c>
#else
# include <math-type-macros-float.h>
# include <w_atan2_template.c>
#endif
