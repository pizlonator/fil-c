/* m68k provides an optimized __cosh_atan2f.  */
#ifdef SHARED
# define NO_COMPAT_NEEDED 1
# include <math/w_cosh_compat.c>
#else
# include <math-type-macros-double.h>
# include <w_cosh_template.c>
#endif
