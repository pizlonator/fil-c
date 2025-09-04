# posix_memalign.m4 serial 3
dnl Copyright (C) 2020-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_POSIX_MEMALIGN],
[
  AC_REQUIRE([gl_STDLIB_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles

  dnl Persuade glibc <stdlib.h> to declare posix_memalign().
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])

  gl_CHECK_FUNCS_ANDROID([posix_memalign], [[#include <stdlib.h>]])
  if test $ac_cv_func_posix_memalign = yes; then
    dnl On OpenBSD 6.1, posix_memalign (&p, 32, 2406) returns a pointer
    dnl that is not a multiple of 32.
    AC_CACHE_CHECK([whether posix_memalign works for large alignments],
      [gl_cv_func_posix_memalign_works],
      [AC_RUN_IFELSE(
         [AC_LANG_PROGRAM(
            [[#include <stdlib.h>
            ]],
            [[void *p;
              if (posix_memalign (&p, 32, 2406) == 0)
                if (((unsigned long)p % 32) != 0)
                  return 1;
              return 0;
            ]])
         ],
         [gl_cv_func_posix_memalign_works=yes],
         [gl_cv_func_posix_memalign_works=no],
         [case "$host_os" in
                      # Guess no on OpenBSD.
            openbsd*) gl_cv_func_posix_memalign_works="guessing no" ;;
                      # If we don't know, obey --enable-cross-guesses.
            *)        gl_cv_func_posix_memalign_works="$gl_cross_guess_normal" ;;
          esac
         ])
      ])
    case "$gl_cv_func_posix_memalign_works" in
      *yes) ;;
      *) REPLACE_POSIX_MEMALIGN=1 ;;
    esac
  else
    dnl The system does not have posix_memalign.
    HAVE_POSIX_MEMALIGN=0
    case "$gl_cv_onwards_func_posix_memalign" in
      future*) REPLACE_POSIX_MEMALIGN=1 ;;
    esac
  fi
])
