/* m68k provides an optimized __ieee752_fmod.  */
#include <math-svid-compat.h>
#ifdef SHARED
# undef SHLIB_COMPAT
# define SHLIB_COMPAT(a, b, c) 1
# undef LIBM_SVID_COMPAT
# define LIBM_SVID_COMPAT 1
# undef compat_symbol
# define compat_symbol(a, b, c, d)
#include <math/w_fmod_compat.c>
libm_alias_double (__fmod_compat, fmod)
#else
#include <math-type-macros-double.h>
#include <w_fmod_template.c>
#endif
