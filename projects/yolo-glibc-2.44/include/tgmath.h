/* NB: Clang has its own <tgmath.h> and doesn't use <tgmath.h> from
   glibc.  */
#if defined _ISOMAC && defined __clang__
# include_next <tgmath.h>
#else
# include <math/tgmath.h>
#endif
