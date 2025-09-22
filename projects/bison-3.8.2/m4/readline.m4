# readline.m4 serial 10
dnl Copyright (C) 2005-2006, 2009-2021 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Simon Josefsson, with help from Bruno Haible and Oskar
dnl Liljeblad.

AC_DEFUN([gl_FUNC_READLINE],
[
  dnl Prerequisites of AC_LIB_LINKFLAGS_BODY.
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])

  dnl Search for libreadline and define LIBREADLINE, LTLIBREADLINE and
  dnl INCREADLINE accordingly.
  AC_LIB_LINKFLAGS_BODY([readline])

  dnl Add $INCREADLINE to CPPFLAGS before performing the following checks,
  dnl because if the user has installed libreadline and not disabled its use
  dnl via --without-libreadline-prefix, he wants to use it. The AC_LINK_IFELSE
  dnl will then succeed.
  am_save_CPPFLAGS="$CPPFLAGS"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INCREADLINE])

  AC_CACHE_CHECK([for readline], [gl_cv_lib_readline], [
    gl_cv_lib_readline=no
    am_save_LIBS="$LIBS"
    dnl On some systems, -lreadline doesn't link without an additional
    dnl -lncurses or -ltermcap.
    dnl Try -lncurses before -ltermcap, because libtermcap is insecure
    dnl by design and obsolete since 1994. Try -lcurses last, because
    dnl libcurses is unusable on some old Unices.
    for extra_lib in "" ncurses termcap curses; do
      LIBS="$am_save_LIBS $LIBREADLINE"
      if test -n "$extra_lib"; then
        LIBS="$LIBS -l$extra_lib"
      fi
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdio.h>
#include <readline/readline.h>]],
          [[readline((char*)0);]])],
        [if test -n "$extra_lib"; then
           gl_cv_lib_readline="yes, requires -l$extra_lib"
         else
           gl_cv_lib_readline="yes"
         fi
        ])
      if test "$gl_cv_lib_readline" != no; then
        break
      fi
    done
    LIBS="$am_save_LIBS"
  ])

  if test "$gl_cv_lib_readline" != no; then
    AC_DEFINE([HAVE_READLINE], [1], [Define if you have the readline library.])
    extra_lib=`echo "$gl_cv_lib_readline" | sed -n -e 's/yes, requires //p'`
    if test -n "$extra_lib"; then
      LIBREADLINE="$LIBREADLINE $extra_lib"
      LTLIBREADLINE="$LTLIBREADLINE $extra_lib"
    fi
    AC_MSG_CHECKING([how to link with libreadline])
    AC_MSG_RESULT([$LIBREADLINE])
  else
    dnl If $LIBREADLINE didn't lead to a usable library, we don't
    dnl need $INCREADLINE either.
    CPPFLAGS="$am_save_CPPFLAGS"
    LIBREADLINE=
    LTLIBREADLINE=
  fi
  AC_SUBST([LIBREADLINE])
  AC_SUBST([LTLIBREADLINE])

  AC_CHECK_HEADERS([readline/readline.h readline/history.h])
])

# Prerequisites of lib/readline.c.
AC_DEFUN([gl_PREREQ_READLINE], [
  :
])
