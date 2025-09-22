# getrusage.m4 serial 1
dnl Copyright (C) 2012-2021 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_GETRUSAGE],
[
  AC_REQUIRE([gl_SYS_RESOURCE_H_DEFAULTS])
  AC_CHECK_FUNCS_ONCE([getrusage])
  if test $ac_cv_func_getrusage = no; then
    HAVE_GETRUSAGE=0
  fi
])
