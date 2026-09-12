#define UNSIGNED 1
#define INEXACT 0
#define FUNC __compat_ufromfpl
#include <s_compat_fromfpl_main.c>
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
weak_alias (__compat_ufromfpl, ufromfpl)
#endif
