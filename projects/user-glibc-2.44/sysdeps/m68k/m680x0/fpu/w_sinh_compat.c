/* m68k provides an optimized __ieee754_sinh.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_sinh_compat.c>
#else
# include <math-type-macros-double.h>
# include <w_sinh_template.c>
#endif
