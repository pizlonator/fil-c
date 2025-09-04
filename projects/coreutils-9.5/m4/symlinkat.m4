# serial 13
# See if we need to provide symlinkat replacement.

dnl Copyright (C) 2009-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Eric Blake.

AC_DEFUN([gl_FUNC_SYMLINKAT],
[
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  AC_REQUIRE([gl_FUNC_OPENAT])
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
  gl_CHECK_FUNCS_ANDROID([symlinkat], [[#include <unistd.h>]])
  if test $ac_cv_func_symlinkat = no; then
    HAVE_SYMLINKAT=0
    case "$gl_cv_onwards_func_symlinkat" in
      future*) REPLACE_SYMLINKAT=1 ;;
    esac
  else
    AC_CACHE_CHECK([whether symlinkat handles trailing slash correctly],
      [gl_cv_func_symlinkat_works],
      [AC_RUN_IFELSE(
         [AC_LANG_PROGRAM(
           [[#include <fcntl.h>
             #include <unistd.h>
           ]],
           [[int result = 0;
             if (!symlinkat ("a", AT_FDCWD, "conftest.link/"))
               result |= 1;
             if (symlinkat ("conftest.f", AT_FDCWD, "conftest.lnk2"))
               result |= 2;
             else if (!symlinkat ("a", AT_FDCWD, "conftest.lnk2/"))
               result |= 4;
             return result;
           ]])],
         [gl_cv_func_symlinkat_works=yes],
         [gl_cv_func_symlinkat_works=no],
         [case "$host_os" in
                             # Guess yes on Linux systems.
            linux-* | linux) gl_cv_func_symlinkat_works="guessing yes" ;;
                             # Guess yes on systems that emulate the Linux system calls.
            midipix*)        gl_cv_func_symlinkat_works="guessing yes" ;;
                             # Guess yes on glibc systems.
            *-gnu* | gnu*)   gl_cv_func_symlinkat_works="guessing yes" ;;
                             # If we don't know, obey --enable-cross-guesses.
            *)               gl_cv_func_symlinkat_works="$gl_cross_guess_normal" ;;
          esac
         ])
      rm -f conftest.f conftest.link conftest.lnk2])
    case "$gl_cv_func_symlinkat_works" in
      *yes) ;;
      *)
        REPLACE_SYMLINKAT=1
        ;;
    esac
  fi
])
