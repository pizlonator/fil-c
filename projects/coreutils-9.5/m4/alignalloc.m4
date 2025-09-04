# alignalloc.m4 serial 1
dnl Copyright 2022-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_ALIGNALLOC],
[
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])
  gl_CHECK_FUNCS_ANDROID([posix_memalign], [[#include <stdlib.h>]])
])
