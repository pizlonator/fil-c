# serial 4
# See if we need to provide renameat replacement.

dnl Copyright (C) 2009-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Eric Blake.

AC_DEFUN([gl_FUNC_RENAMEAT],
[
  AC_REQUIRE([gl_FUNC_OPENAT])
  AC_REQUIRE([gl_FUNC_RENAME])
  AC_REQUIRE([gl_STDIO_H_DEFAULTS])
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_CHECK_HEADERS([linux/fs.h])
  AC_CHECK_FUNCS_ONCE([renameat])
  if test $ac_cv_func_renameat = no; then
    HAVE_RENAMEAT=0
  elif test $REPLACE_RENAME = 1; then
    dnl Solaris 9 and 10 have the same bugs in renameat as in rename.
    REPLACE_RENAMEAT=1
  fi
  gl_CHECK_FUNCS_ANDROID([renameat2], [[#include <stdio.h>]])
])
