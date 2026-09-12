#define UNSIGNED 1
#define INEXACT 1
#define FUNC __ufromfpxl
#include <s_fromfpl_main.c>
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
#endif
weak_alias (__ufromfpxl, ufromfpxl)
