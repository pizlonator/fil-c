# freadahead.m4 serial 2
dnl Copyright (C) 2012-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_FREADAHEAD],
[
  gl_CHECK_FUNCS_ANDROID([__freadahead], [[#include <stdio_ext.h>]])
])
