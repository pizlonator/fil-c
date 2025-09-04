# mkdirat.m4 serial 1
dnl Copyright (C) 2004-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Jim Meyering.

AC_DEFUN([gl_FUNC_MKDIRAT],
[
  AC_REQUIRE([gl_SYS_STAT_H_DEFAULTS])
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_CHECK_FUNCS_ONCE([mkdirat])
  if test $ac_cv_func_mkdirat != yes; then
    HAVE_MKDIRAT=0
  fi
])

# Prerequisite of mkdirat's declaration and of lib/mkdirat.c.
AC_DEFUN([gl_PREREQ_MKDIRAT],
[
  AC_REQUIRE([AC_TYPE_MODE_T])
])
