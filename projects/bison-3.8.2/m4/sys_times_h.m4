# Configure a replacement for <sys/times.h>.
# serial 11

# Copyright (C) 2008-2021 Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# Written by Simon Josefsson.

AC_DEFUN_ONCE([gl_SYS_TIMES_H],
[
  AC_REQUIRE([gl_SYS_TIMES_H_DEFAULTS])

  dnl <sys/times.h> is always overridden, because of GNULIB_POSIXCHECK.
  gl_CHECK_NEXT_HEADERS([sys/times.h])
  if test $ac_cv_header_sys_times_h = yes; then
    HAVE_SYS_TIMES_H=1
    AC_CHECK_TYPES([struct tms], [], [HAVE_STRUCT_TMS=0], [[
#include <sys/times.h>
      ]])
  else
    HAVE_SYS_TIMES_H=0
    HAVE_STRUCT_TMS=0
  fi
  AC_SUBST([HAVE_SYS_TIMES_H])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use.
  gl_WARN_ON_USE_PREPARE([[
/* Some systems have incomplete headers.  */
#if !(defined __GLIBC__ && !defined __UCLIBC__)
# include <time.h>
#endif
#include <sys/times.h>
    ]], [times])
])

# gl_SYS_TIMES_MODULE_INDICATOR([modulename])
# sets the shell variable that indicates the presence of the given module
# to a C preprocessor expression that will evaluate to 1.
# This macro invocation must not occur in macros that are AC_REQUIREd.
AC_DEFUN([gl_SYS_TIMES_MODULE_INDICATOR],
[
  dnl Ensure to expand the default settings once only.
  gl_SYS_TIMES_H_REQUIRE_DEFAULTS
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
])

# Initializes the default values for AC_SUBSTed shell variables.
# This macro must not be AC_REQUIREd.  It must only be invoked, and only
# outside of macros or in macros that are not AC_REQUIREd.
AC_DEFUN([gl_SYS_TIMES_H_REQUIRE_DEFAULTS],
[
  m4_defun(GL_MODULE_INDICATOR_PREFIX[_SYS_TIMES_H_MODULE_INDICATOR_DEFAULTS], [
    gl_MODULE_INDICATOR_INIT_VARIABLE([GNULIB_TIMES])
  ])
  m4_require(GL_MODULE_INDICATOR_PREFIX[_SYS_TIMES_H_MODULE_INDICATOR_DEFAULTS])
  AC_REQUIRE([gl_SYS_TIMES_H_DEFAULTS])
])

AC_DEFUN([gl_SYS_TIMES_H_DEFAULTS],
[
  HAVE_STRUCT_TMS=1;  AC_SUBST([HAVE_STRUCT_TMS])
  HAVE_TIMES=1;       AC_SUBST([HAVE_TIMES])
])
