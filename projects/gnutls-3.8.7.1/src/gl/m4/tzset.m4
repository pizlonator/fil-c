# tzset.m4
# serial 18
dnl Copyright (C) 2003, 2007, 2009-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Paul Eggert and Jim Meyering.

AC_DEFUN([gl_FUNC_TZSET],
[
  AC_REQUIRE([gl_TIME_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST])
  case "$host_os" in
    mingw* | windows*) REPLACE_TZSET=1 ;;
  esac
])
