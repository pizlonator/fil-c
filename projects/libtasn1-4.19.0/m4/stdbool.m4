# Check for stdbool.h that conforms to C99.

dnl Copyright (C) 2002-2006, 2009-2022 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

#serial 9

# Prepare for substituting <stdbool.h> if it is not supported.

AC_DEFUN([gl_STDBOOL_H],
[
  AC_REQUIRE([AC_CHECK_HEADER_STDBOOL])
  AC_REQUIRE([AC_CANONICAL_HOST])

  dnl On some platforms, <stdbool.h> does not exist or does not conform to C99.
  dnl On Solaris 10 with CC=cc CXX=CC, <stdbool.h> exists but is not usable
  dnl in C++ mode (and no <cstdbool> exists). In this case, we use our
  dnl replacement, also in C mode (for binary compatibility between C and C++).
  if test "$ac_cv_header_stdbool_h" = yes; then
    case "$host_os" in
      solaris*)
        if test -z "$GCC"; then
          GL_GENERATE_STDBOOL_H=true
        else
          GL_GENERATE_STDBOOL_H=false
        fi
        ;;
      *)
        GL_GENERATE_STDBOOL_H=false
        ;;
    esac
  else
    GL_GENERATE_STDBOOL_H=true
  fi

  if test "$ac_cv_type__Bool" = yes; then
    HAVE__BOOL=1
  else
    HAVE__BOOL=0
  fi
  AC_SUBST([HAVE__BOOL])
])

m4_version_prereq([2.72], [], [

AC_DEFUN([AC_CHECK_HEADER_STDBOOL],
  [AC_CHECK_TYPES([_Bool])
   AC_CACHE_CHECK([for stdbool.h that conforms to C99 or later],
     [ac_cv_header_stdbool_h],
     [AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
           [[#include <stdbool.h>

             /* "true" and "false" should be usable in #if expressions and
                integer constant expressions, and "bool" should be a valid
                type name.

                Although C 1999 requires bool, true, and false to be macros,
                C 2023 and C++ 2011 overrule that, so do not test for that.
                Although C 1999 requires __bool_true_false_are_defined and
                _Bool, C 2023 says they are obsolescent, so do not require
                them.  */

             #if !true
               #error "'true' is not true"
             #endif
             #if true != 1
               #error "'true' is not equal to 1"
             #endif
             char b[true == 1 ? 1 : -1];
             char c[true];

             #if false
               #error "'false' is not false"
             #endif
             #if false != 0
               #error "'false' is not equal to 0"
             #endif
             char d[false == 0 ? 1 : -1];

             enum { e = false, f = true, g = false * true, h = true * 256 };

             char i[(bool) 0.5 == true ? 1 : -1];
             char j[(bool) 0.0 == false ? 1 : -1];
             char k[sizeof (bool) > 0 ? 1 : -1];

             struct sb { bool s: 1; bool t; } s;
             char l[sizeof s.t > 0 ? 1 : -1];

             /* The following fails for
                HP aC++/ANSI C B3910B A.05.55 [Dec 04 2003]. */
             bool m[h];
             char n[sizeof m == h * sizeof m[0] ? 1 : -1];
             char o[-1 - (bool) 0 < 0 ? 1 : -1];
             /* Catch a bug in an HP-UX C compiler.  See
                https://gcc.gnu.org/ml/gcc-patches/2003-12/msg02303.html
                https://lists.gnu.org/r/bug-coreutils/2005-11/msg00161.html
              */
             bool p = true;
             bool *pp = &p;
           ]],
           [[
             bool ps = &s;
             *pp |= p;
             *pp |= ! p;

             /* Refer to every declared value, so they cannot be
                discarded as unused.  */
             return (!a + !b + !c + !d + !e + !f + !g + !h + !i + !j + !k
                     + !l + !m + !n + !o + !p + !pp + !ps);
           ]])],
        [ac_cv_header_stdbool_h=yes],
        [ac_cv_header_stdbool_h=no])])
])# AC_CHECK_HEADER_STDBOOL

]) # m4_version_prereq 2.72
