# ldexp.m4 serial 3
dnl Copyright (C) 2010-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_LDEXP],
[
  AC_REQUIRE([gl_MATH_H_DEFAULTS])
  AC_REQUIRE([gl_FUNC_ISNAND]) dnl for ISNAND_LIBM

  AC_REQUIRE([gl_CHECK_LDEXP_NO_LIBM])
  LDEXP_LIBM=
  if test $gl_cv_func_ldexp_no_libm = no; then
    AC_CACHE_CHECK([whether ldexp() can be used with libm],
      [gl_cv_func_ldexp_in_libm],
      [
        saved_LIBS="$LIBS"
        LIBS="$LIBS -lm"
        AC_LINK_IFELSE(
          [AC_LANG_PROGRAM([[#ifndef __NO_MATH_INLINES
                             # define __NO_MATH_INLINES 1 /* for glibc */
                             #endif
                             #include <math.h>
                             double (*funcptr) (double, int) = ldexp;
                             double x;]],
                           [[return ldexp (x, -1) > 0;]])],
          [gl_cv_func_ldexp_in_libm=yes],
          [gl_cv_func_ldexp_in_libm=no])
        LIBS="$saved_LIBS"
      ])
    if test $gl_cv_func_ldexp_in_libm = yes; then
      LDEXP_LIBM=-lm
    fi
  fi

  saved_LIBS="$LIBS"
  LIBS="$LIBS $LDEXP_LIBM"
  gl_FUNC_LDEXP_WORKS
  LIBS="$saved_LIBS"
  case "$gl_cv_func_ldexp_works" in
    *yes) ;;
    *) REPLACE_LDEXP=1 ;;
  esac

  if test $REPLACE_LDEXP = 1; then
    dnl Find libraries needed to link lib/ldexp.c.
    LDEXP_LIBM="$ISNAND_LIBM"
  fi
  AC_SUBST([LDEXP_LIBM])
])

dnl Test whether ldexp() can be used without linking with libm.
dnl Set gl_cv_func_ldexp_no_libm to 'yes' or 'no' accordingly.
AC_DEFUN([gl_CHECK_LDEXP_NO_LIBM],
[
  AC_CACHE_CHECK([whether ldexp() can be used without linking with libm],
    [gl_cv_func_ldexp_no_libm],
    [
      AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([[#ifndef __NO_MATH_INLINES
                           # define __NO_MATH_INLINES 1 /* for glibc */
                           #endif
                           #include <math.h>
                           double (*funcptr) (double, int) = ldexp;
                           double x;]],
                         [[return ldexp (x, -1) > 0;]])],
        [gl_cv_func_ldexp_no_libm=yes],
        [gl_cv_func_ldexp_no_libm=no])
    ])
])

dnl Test whether ldexp() works (this fails on OpenBSD 7.3/mips64).
AC_DEFUN([gl_FUNC_LDEXP_WORKS],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
  AC_CACHE_CHECK([whether ldexp works], [gl_cv_func_ldexp_works],
    [
      AC_RUN_IFELSE(
        [AC_LANG_SOURCE([[
#include <math.h>
int main()
{
  int result = 0;
  {
    volatile double x = 1.9269695883136991774e-308;
    volatile double y = ldexp (x, 0);
    if (y != x)
      result |= 1;
  }
  return result;
}]])],
        [gl_cv_func_ldexp_works=yes],
        [gl_cv_func_ldexp_works=no],
        [case "$host_os" in
           openbsd*)          gl_cv_func_ldexp_works="guessing no" ;;
                              # Guess yes on native Windows.
           mingw* | windows*) gl_cv_func_ldexp_works="guessing yes" ;;
           *)                 gl_cv_func_ldexp_works="guessing yes" ;;
         esac
        ])
    ])
])
