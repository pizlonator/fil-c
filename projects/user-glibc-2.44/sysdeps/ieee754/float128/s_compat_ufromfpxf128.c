#define UNSIGNED 1
#define INEXACT 1
#define FUNC __compat_ufromfpxf128
#include <float128_private.h>
#include "../ldbl-128/s_compat_fromfpl_main.c"
#if SHLIB_COMPAT (libm, GLIBC_2_25, GLIBC_2_43)
# define CONCATX(x, y) x ## y
# define CONCAT(x, y) CONCATX (x, y)
# define UNIQUE_ALIAS(name) CONCAT (name, __COUNTER__)
# define do_symbol(orig_name, name, aliasname)			\
  strong_alias (orig_name, name)				\
  compat_symbol (libm, name, aliasname,				\
		 CONCAT (FIRST_VERSION_libm_, aliasname))
# undef weak_alias
# define weak_alias(name, aliasname)			\
  do_symbol (name, UNIQUE_ALIAS (name), aliasname);
# undef libm_alias_float128_other_r_ldbl
# define libm_alias_float128_other_r_ldbl(from, to, r)		\
  weak_alias (from ## f128 ## r, __ ## to ## ieee128 ## r)
libm_alias_float128 (__compat_ufromfpx, ufromfpx)
#endif
