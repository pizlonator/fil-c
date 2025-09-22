# termios_h.m4 serial 7
dnl Copyright (C) 2010-2021 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN_ONCE([gl_TERMIOS_H],
[
  dnl Ensure to expand the default settings once only, before all statements
  dnl that occur in other macros.
  AC_REQUIRE([gl_TERMIOS_H_DEFAULTS])

  gl_CHECK_NEXT_HEADERS([termios.h])
  if test $ac_cv_header_termios_h != yes; then
    HAVE_TERMIOS_H=0
  fi

  dnl Ensure the type pid_t gets defined.
  AC_REQUIRE([AC_TYPE_PID_T])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use, and which is not
  dnl guaranteed by C89.
  gl_WARN_ON_USE_PREPARE([[#include <termios.h>]],
    [tcgetsid])
])

# gl_TERMIOS_MODULE_INDICATOR([modulename])
# sets the shell variable that indicates the presence of the given module
# to a C preprocessor expression that will evaluate to 1.
# This macro invocation must not occur in macros that are AC_REQUIREd.
AC_DEFUN([gl_TERMIOS_MODULE_INDICATOR],
[
  dnl Ensure to expand the default settings once only.
  gl_TERMIOS_H_REQUIRE_DEFAULTS
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
  dnl Define it also as a C macro, for the benefit of the unit tests.
  gl_MODULE_INDICATOR_FOR_TESTS([$1])
])

# Initializes the default values for AC_SUBSTed shell variables.
# This macro must not be AC_REQUIREd.  It must only be invoked, and only
# outside of macros or in macros that are not AC_REQUIREd.
AC_DEFUN([gl_TERMIOS_H_REQUIRE_DEFAULTS],
[
  m4_defun(GL_MODULE_INDICATOR_PREFIX[_TERMIOS_H_MODULE_INDICATOR_DEFAULTS], [
    gl_MODULE_INDICATOR_INIT_VARIABLE([GNULIB_TCGETSID])
  ])
  m4_require(GL_MODULE_INDICATOR_PREFIX[_TERMIOS_H_MODULE_INDICATOR_DEFAULTS])
  AC_REQUIRE([gl_TERMIOS_H_DEFAULTS])
])

AC_DEFUN([gl_TERMIOS_H_DEFAULTS],
[
  dnl Assume proper GNU behavior unless another module says otherwise.
  HAVE_DECL_TCGETSID=1;   AC_SUBST([HAVE_DECL_TCGETSID])
  HAVE_TERMIOS_H=1;       AC_SUBST([HAVE_TERMIOS_H])
])
