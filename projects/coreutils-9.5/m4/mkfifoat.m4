# serial 10
# See if we need to provide mkfifoat/mknodat replacement.

dnl Copyright (C) 2009-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Written by Eric Blake.

AC_DEFUN([gl_FUNC_MKFIFOAT],
[
  AC_REQUIRE([gl_SYS_STAT_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles

  dnl Persuade glibc <sys/stat.h> to declare mkfifoat() and mknodat().
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

  AC_REQUIRE([gl_FUNC_OPENAT])

  gl_CHECK_FUNCS_ANDROID_MACOS([mknodat], [[#include <sys/stat.h>]])
  gl_CHECK_FUNCS_ANDROID_MACOS([mkfifoat], [[#include <sys/stat.h>]])
  if test $ac_cv_func_mkfifoat = yes; then
    dnl Check for AIX 7.2 bug with trailing slash.
    AC_CACHE_CHECK([whether mkfifoat rejects trailing slashes],
      [gl_cv_func_mkfifoat_works],
      [rm -f conftest.tmp
       AC_RUN_IFELSE(
         [AC_LANG_PROGRAM(
            [[#include <fcntl.h>
              #include <sys/stat.h>
            ]],
            [[int result = 0;
              if (!mkfifoat (AT_FDCWD, "conftest.tmp/", 0600))
                result |= 1;
              return result;
            ]])
         ],
         [gl_cv_func_mkfifoat_works=yes],
         [gl_cv_func_mkfifoat_works=no],
         [case "$host_os" in
                             # Guess yes on Linux systems.
            linux-* | linux) gl_cv_func_mkfifoat_works="guessing yes" ;;
                             # Guess yes on glibc systems.
            *-gnu* | gnu*)   gl_cv_func_mkfifoat_works="guessing yes" ;;
                             # Guess no on AIX systems.
            aix*)            gl_cv_func_mkfifoat_works="guessing no" ;;
                             # If we don't know, obey --enable-cross-guesses.
            *)               gl_cv_func_mkfifoat_works="$gl_cross_guess_normal" ;;
          esac
         ])
       rm -f conftest.tmp
      ])
    case "$gl_cv_func_mkfifoat_works" in
      *yes) ;;
      *)
        REPLACE_MKFIFOAT=1
        dnl mknodat() has the same bug as mkfifoat().
        REPLACE_MKNODAT=1
        ;;
    esac
  else
    # No known system has mkfifoat but not mknodat
    HAVE_MKFIFOAT=0
    case "$gl_cv_onwards_func_mkfifoat" in
      future*) REPLACE_MKFIFOAT=1 ;;
    esac
    HAVE_MKNODAT=0
    case "$gl_cv_onwards_func_mknodat" in
      future*) REPLACE_MKNODAT=1 ;;
    esac
  fi
])
