# strverscmp.m4
# serial 10
dnl Copyright (C) 2002, 2005-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_STRVERSCMP],
[
  dnl Persuade glibc <string.h> to declare strverscmp().
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])

  AC_REQUIRE([gl_STRING_H_DEFAULTS])
  AC_CHECK_FUNCS([strverscmp])
  if test $ac_cv_func_strverscmp = no; then
    HAVE_STRVERSCMP=0
  else
    AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
    AC_CACHE_CHECK([whether strverscmp works],
      [gl_cv_func_strverscmp_works],
      [dnl Detect musl-1.2.3 and Cygwin 3.4.6 bug.
       AC_RUN_IFELSE(
         [AC_LANG_SOURCE([[
#include <string.h>
int main ()
{
  return strverscmp ("UNKNOWN", "2.2.0") <= 0;
}
         ]])],
         [gl_cv_func_strverscmp_works=yes],
         [gl_cv_func_strverscmp_works=no],
         [case "$host_os" in
                                # Guess yes on glibc systems.
            *-gnu* | gnu*)      gl_cv_func_strverscmp_works="guessing yes" ;;
                                # Guess no on musl systems.
            *-musl* | midipix*) gl_cv_func_strverscmp_works="guessing no" ;;
                                # Guess no on Cygwin.
            cygwin*)            gl_cv_func_strverscmp_works="guessing no" ;;
                                # If we don't know, obey --enable-cross-guesses.
            *)                  gl_cv_func_strverscmp_works="$gl_cross_guess_normal" ;;
          esac
         ])
      ])
    case "$gl_cv_func_strverscmp_works" in
      *yes) ;;
      *) REPLACE_STRVERSCMP=1 ;;
    esac
  fi
])

# Prerequisites of lib/strverscmp.c.
AC_DEFUN([gl_PREREQ_STRVERSCMP], [
  :
])
