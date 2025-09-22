# atoll.m4 serial 3
dnl Copyright (C) 2008-2023 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_ATOLL],
[
  AC_REQUIRE([gl_STDLIB_H_DEFAULTS])
  AC_CHECK_FUNCS([atoll])
  if test $ac_cv_func_atoll = no; then
    HAVE_ATOLL=0
  fi
])

# Prerequisites of lib/atoll.c.
AC_DEFUN([gl_PREREQ_ATOLL], [
  :
])
