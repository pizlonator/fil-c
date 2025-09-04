# tmpfile.m4 serial 4
# Copyright (C) 2007, 2009-2024 Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# Written by Ben Pfaff.

# Check whether to use a replacement tmpfile() function.

# The native Windows tmpfile function always tries to put the temporary
# file in the root directory.  (This behaviour is even documented in
# Microsoft's documentation!)  This often fails for ordinary users who
# don't have the permissions to write in the root directory.
#
# We can't test for tmpfile even at runtime, since our test program
# might be running with privileges that allow it to write to the root
# directory, even though tmpfile wouldn't work in general.  Instead,
# just test for a Windows platform (excluding Cygwin).

# On Android 4.3, tmpfile() always returns NULL, even if TMPDIR is set
# to a writable directory.

AC_DEFUN([gl_FUNC_TMPFILE], [
  AC_REQUIRE([gl_STDIO_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
  AC_CACHE_CHECK([whether tmpfile works],
    [gl_cv_func_tmpfile_works],
    [AC_EGREP_CPP([choke me], [
#if defined _WIN32 && !defined __CYGWIN__
choke me
#endif
       ],
       [gl_cv_func_tmpfile_works=no],
       [AC_RUN_IFELSE(
          [AC_LANG_SOURCE([[
#include <stdio.h>
#include <stdlib.h>
int
main (void)
{
  return tmpfile () == NULL;
}]])],
          [gl_cv_func_tmpfile_works=yes],
          [gl_cv_func_tmpfile_works=no],
          [case "$host_os" in
                              # Guess no on Android.
             linux*-android*) gl_cv_func_tmpfile_works="guessing no" ;;
                              # Guess yes otherwise.
             *)               gl_cv_func_tmpfile_works="guessing yes" ;;
           esac
          ])
       ])
    ])
  case "$gl_cv_func_tmpfile_works" in
    *yes) ;;
    *) REPLACE_TMPFILE=1 ;;
  esac
])

# Prerequisites of lib/tmpfile.c.
AC_DEFUN([gl_PREREQ_TMPFILE], [:])
