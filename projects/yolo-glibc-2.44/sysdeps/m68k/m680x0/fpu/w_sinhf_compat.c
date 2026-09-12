/* m68k provides an optimized __ieee754_sinhhf.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_sinhf_compat.c>
#else
# include <math-type-macros-float.h>
# include <w_sinh_template.c>
#endif
