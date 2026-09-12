/* m68k provides an optimized __ieee754_acosf.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_acosf_compat.c>
#else
# include <math-type-macros-float.h>
# include <w_acos_template.c>
#endif
