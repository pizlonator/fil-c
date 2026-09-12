#define UNSIGNED 0
#define INEXACT 1
#define FUNC __fromfpxf128
#include <float128_private.h>
#include "../ldbl-128/s_fromfpl_main.c"
#ifdef SHARED
# define CONCATX(x, y) x ## y
# define CONCAT(x, y) CONCATX (x, y)
# define UNIQUE_ALIAS(name) CONCAT (name, __COUNTER__)
# define do_symbol(orig_name, name, aliasname)		\
  strong_alias (orig_name, name)			\
  versioned_symbol (libm, name, aliasname, GLIBC_2_43)
# undef weak_alias
# define weak_alias(name, aliasname)			\
  do_symbol (name, UNIQUE_ALIAS (name), aliasname);
# undef libm_alias_float128_other_r_ldbl
# define libm_alias_float128_other_r_ldbl(from, to, r)		\
  weak_alias (from ## f128 ## r, __ ## to ## ieee128 ## r)
#endif
libm_alias_float128 (__fromfpx, fromfpx)
