# sys_utsname_h.m4 serial 11
dnl Copyright (C) 2009-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Bruno Haible.

AC_DEFUN_ONCE([gl_SYS_UTSNAME_H],
[
  dnl Ensure to expand the default settings once only, before all statements
  dnl that occur in other macros.
  AC_REQUIRE([gl_SYS_UTSNAME_H_DEFAULTS])

  dnl <sys/utsname.h> is always overridden, because of GNULIB_POSIXCHECK.
  gl_CHECK_NEXT_HEADERS([sys/utsname.h])
  if test $ac_cv_header_sys_utsname_h != yes; then
    HAVE_SYS_UTSNAME_H=0
    HAVE_STRUCT_UTSNAME=0
  else
    HAVE_SYS_UTSNAME_H=1
    AC_CHECK_TYPES([struct utsname], [], [HAVE_STRUCT_UTSNAME=0], [[
/* Minix 3.1.8 has a bug: <stddef.h> must be included before
   <sys/utsname.h>.  */
#include <stddef.h>
#include <sys/utsname.h>
      ]])
  fi
  AC_SUBST([HAVE_SYS_UTSNAME_H])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use.
  gl_WARN_ON_USE_PREPARE([[
    /* Minix 3.1.8 has a bug: <stddef.h> must be included before
       <sys/utsname.h>.  */
    #include <stddef.h>
    #include <sys/utsname.h>
    ]], [uname])
])

# gl_SYS_UTSNAME_MODULE_INDICATOR([modulename])
# sets the shell variable that indicates the presence of the given module
# to a C preprocessor expression that will evaluate to 1.
# This macro invocation must not occur in macros that are AC_REQUIREd.
AC_DEFUN([gl_SYS_UTSNAME_MODULE_INDICATOR],
[
  dnl Ensure to expand the default settings once only.
  gl_SYS_UTSNAME_H_REQUIRE_DEFAULTS
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
])

# Initializes the default values for AC_SUBSTed shell variables.
# This macro must not be AC_REQUIREd.  It must only be invoked, and only
# outside of macros or in macros that are not AC_REQUIREd.
AC_DEFUN([gl_SYS_UTSNAME_H_REQUIRE_DEFAULTS],
[
  m4_defun(GL_MODULE_INDICATOR_PREFIX[_SYS_UTSNAME_H_MODULE_INDICATOR_DEFAULTS], [
    gl_MODULE_INDICATOR_INIT_VARIABLE([GNULIB_UNAME])
  ])
  m4_require(GL_MODULE_INDICATOR_PREFIX[_SYS_UTSNAME_H_MODULE_INDICATOR_DEFAULTS])
  AC_REQUIRE([gl_SYS_UTSNAME_H_DEFAULTS])
])

AC_DEFUN([gl_SYS_UTSNAME_H_DEFAULTS],
[
  dnl Assume proper GNU behavior unless another module says otherwise.
  HAVE_UNAME=1;           AC_SUBST([HAVE_UNAME])
  HAVE_STRUCT_UTSNAME=1;  AC_SUBST([HAVE_STRUCT_UTSNAME])
])
