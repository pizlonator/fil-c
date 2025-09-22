# Check prerequisites for compiling lib/c-stack.c.

# Copyright (C) 2002-2004, 2008-2023 Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# Written by Paul Eggert.

# serial 25

AC_DEFUN([gl_C_STACK],
[
  dnl 'c-stack' needs -lsigsegv if and only if the 'sigsegv' module needs it.
  AC_REQUIRE([gl_SIGSEGV])
  if test $gl_sigsegv_uses_libsigsegv = yes; then
    AC_SUBST([LIBCSTACK], [$LIBSIGSEGV])
    AC_SUBST([LTLIBCSTACK], [$LTLIBSIGSEGV])
  fi
])
