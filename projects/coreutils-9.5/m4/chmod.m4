# chmod.m4 serial 3
dnl Copyright (C) 2004-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN([gl_FUNC_CHMOD],
[
  AC_REQUIRE([gl_SYS_STAT_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST]) dnl for cross-compiles
  AC_CACHE_CHECK([whether chmod works],
    [gl_cv_func_chmod_works],
    [AC_RUN_IFELSE(
       [AC_LANG_PROGRAM(
          [
            AC_INCLUDES_DEFAULT[
            #if defined _WIN32 && !defined __CYGWIN__
             #include <io.h>
            #endif
            #include <errno.h>
            #include <fcntl.h>
            #ifndef S_IRUSR
             #define S_IRUSR 0400
            #endif
            #ifndef S_IWUSR
             #define S_IWUSR 0200
            #endif
            #ifndef S_IRWXU
             #define S_IRWXU 0700
            #endif
            #ifndef S_IRWXG
             #define S_IRWXG 0070
            #endif
            #ifndef S_IRWXO
             #define S_IRWXO 0007
            #endif
          ]GL_MDA_DEFINES],
          [[
            int permissive = S_IRWXU | S_IRWXG | S_IRWXO;
            int desired = S_IRUSR | S_IWUSR;
            int result = 0;
            #define file "conftest.chmod"
            if (open (file, O_CREAT | O_WRONLY | O_TRUNC, permissive) < 0)
              return 1;
            /* Test whether chmod rejects a trailing slash on a non-directory,
               with error ENOTDIR.
               This test fails on AIX 7.2, IRIX 6.5 (no error) and
               native Windows (error EINVAL).  */
            errno = 0;
            if (chmod (file "/", desired) == 0)
              result |= 2;
            else if (errno != ENOTDIR)
              result |= 4;
            return result;
          ]])],
       [gl_cv_func_chmod_works=yes],
       [gl_cv_func_chmod_works=no],
       [case "$host_os" in
          # Guess no on AIX, IRIX, native Windows.
          aix* | irix* | mingw* | windows*)
            gl_cv_func_chmod_works="guessing no" ;;
          # Guess yes on glibc, musl libc, macOS, FreeBSD, NetBSD, OpenBSD, Solaris, Haiku, Cygwin.
          *-gnu* | gnu* | *-musl* | darwin* | freebsd* | midnightbsd* | netbsd* | openbsd* | solaris* | haiku* | cygwin*)
            gl_cv_func_chmod_works="guessing yes" ;;
          # Guess yes on systems that emulate the Linux system calls.
          midipix*)
            gl_cv_func_chmod_works="guessing yes" ;;
          # If we don't know, obey --enable-cross-guesses.
          *)
            gl_cv_func_chmod_works="$gl_cross_guess_normal" ;;
        esac
       ])
     rm -f conftest.chmod
    ])
  case "$gl_cv_func_chmod_works" in
    *yes) ;;
    *) REPLACE_CHMOD=1 ;;
  esac
])

# Prerequisites of lib/chmod.c.
AC_DEFUN([gl_PREREQ_CHMOD],
[
  :
])
