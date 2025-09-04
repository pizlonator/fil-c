# sm3.m4 serial 2
dnl Copyright (C) 2017-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_SM3],
[
  dnl Prerequisites of lib/sm3.h.
  AC_REQUIRE([AC_C_RESTRICT])

  dnl Prerequisites of lib/sm3.c.
  AC_REQUIRE([gl_BIGENDIAN])

  dnl Currently openssl still doesn't support sm3.
  m4_divert_once([DEFAULTS], [LIB_CRYPTO=])
  AC_SUBST([LIB_CRYPTO])
])
