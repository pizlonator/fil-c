# bison-check-compile-flag.m4

# serial 1

# Copyright (C) 2018-2021 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.



# BISON_CHECK_COMPILER_FLAG(FLAG,
#                           [ACTION-SUCCESS], [ACTION-FAILURE],
#                           [EXTRA-FLAGS])
# -------------------------------------------------------------

# Based on Autoconf Macro Archive's AX_CHECK_COMPILE_FLAG, but using
# AC_LINK_IFELSE, not just AC_COMPILE_IFELSE to capture failures such
# as:
#
#  ./c++.at:401:  $PREPARSER ./list
#  stderr:
#  ./list: error while loading shared libraries: libc++.so.1: cannot open shared object file: No such file or directory
#
# because we thought '-stdlib=c++' works, as it passes with AC_COMPILE_IFELSE.
#
# Not using AX_CHECK_LINK_FLAG, because its message is inappropriate:
#
#  checking whether the linker accepts -std=c++11... yes
#  checking whether the linker accepts -std=c++14... yes
#  checking whether the linker accepts -std=c++17... no
AC_DEFUN([BISON_CHECK_COMPILER_FLAG],
[AC_PREREQ(2.59)dnl for _AC_LANG_PREFIX
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
  AC_LINK_IFELSE([m4_default([$5], [AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_IF([test x"AS_VAR_GET(CACHEVAR)" = xyes], [$2], [$3])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl BISON_CHECK_COMPILER_FLAGS
