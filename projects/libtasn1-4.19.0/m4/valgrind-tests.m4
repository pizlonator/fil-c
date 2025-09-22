# valgrind-tests.m4 serial 8
dnl Copyright (C) 2008-2022 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Simon Josefsson

# gl_VALGRIND_TESTS()
# -------------------
# Check if valgrind is available.

# Sets VALGRIND to command line (including options) to invoke valgrind
# with, may be used directly in autoconf, makefiles or shell scripts.

# Sets LOG_VALGRIND, suitable for use with LOG_COMPILER, that in
# Makefile will expand to command line to invoke self-tests with,
# i.e., LOG_VALGRIND = $(VALGRIND) $(DEFAULT_VALGRINDFLAGS)
# $(VALGRINDFLAGS) $(AM_VALGRINDFLAGS).

# Whether to look for valgrind and set the variables can be influenced
# by calling gl_VALGRIND_TESTS_DEFAULT_NO in configure.ac.
# Regardless, the user can change the choice through the options
# --enable-valgrind-tests or --disable-valgrind-tests.

# You may modify the VALGRIND, DEFAULT_VALGRINDFLAGS and VALGRINDFLAGS
# variables before calling this function to override defaults.  Either
# as developer from configure.ac or user on the ./configure command
# line.  You may set the AM_VALGRINDFLAGS in Makefile.am to provide a
# per-directory additional flag.

AC_DEFUN([gl_VALGRIND_TESTS_DEFAULT_NO],
[
  gl_valgrind_tests_default=no
])

AC_DEFUN_ONCE([gl_VALGRIND_TESTS],
[
  AC_ARG_ENABLE([valgrind-tests],
    AS_HELP_STRING([--disable-valgrind-tests],
                   [don't try to run self tests under valgrind]),
    [opt_valgrind_tests=$enableval], [opt_valgrind_tests=${gl_valgrind_tests_default:-yes}])

  # Run self-tests under valgrind?
  if test "$opt_valgrind_tests" = "yes" && test "$cross_compiling" = no; then
    AC_CHECK_PROGS([VALGRIND], [valgrind])

    # VALGRIND_PROGRAM contains the tool found by AC_CHECK_PROGS.  For
    # backwards compatibility, the VALGRIND variable is later modified
    # to also include all enabled options.  However the new variable
    # LOG_VALGRIND needs to be able to refer to the valgrind tool
    # without options, hence it uses this variable.
    AC_SUBST([VALGRIND_PROGRAM], [$VALGRIND])

    AC_SUBST([DEFAULT_VALGRINDFLAGS])
    if test -z "$DEFAULT_VALGRINDFLAGS"; then
      DEFAULT_VALGRINDFLAGS="-q --error-exitcode=1 --leak-check=full"
    fi
    AC_ARG_VAR([VALGRINDFLAGS], [Additional flags for Valgrind])

    if test -n "$VALGRIND"; then
      AC_CACHE_CHECK([for valgrind options for tests],
        [gl_cv_opt_valgrind_tests],
        [AS_IF([$VALGRIND $DEFAULT_VALGRINDFLAGS $VALGRINDFLAGS true],
               [gl_cv_opt_valgrind_tests="$DEFAULT_VALGRINDFLAGS $VALGRINDFLAGS"],
               [gl_cv_opt_valgrind_tests=no])
        ])
      if test "$gl_cv_opt_valgrind_tests" != no; then
        VALGRIND="$VALGRIND $gl_cv_opt_valgrind_tests"
      fi
    fi

    if test -n "$VALGRIND"; then
      dnl On Ubuntu 16.04, /usr/bin/valgrind works only on 64-bit executables
      dnl but fails on 32-bit executables (with exit code 1) and on x86_64-x32
      dnl executables (with exit code 126).
      AC_CACHE_CHECK([whether valgrind works on executables produced by the compiler],
        [gl_cv_prog_valgrind_works],
        [AC_RUN_IFELSE(
           [AC_LANG_SOURCE([[int main () { return 0; }]])],
           [$VALGRIND $gl_cv_opt_valgrind_tests ./conftest$ac_exeext 2>/dev/null
            if test $? = 0; then
              gl_cv_prog_valgrind_works=yes
            else
              gl_cv_prog_valgrind_works=no
            fi
           ],
           [gl_cv_prog_valgrind_works=no])
        ])
    fi

    AC_SUBST([AM_VALGRINDFLAGS])
    AC_SUBST([LOG_VALGRIND], ["\$(VALGRIND_PROGRAM) \$(DEFAULT_VALGRINDFLAGS) \$(VALGRINDFLAGS) \$(AM_VALGRINDFLAGS)"])

    if test "$gl_cv_prog_valgrind_works" != yes; then
      DEFAULT_VALGRINDFLAGS=
      LOG_VALGRIND=
      VALGRIND=
      VALGRINDFLAGS=
      VALGRIND_PROGRAM=
    fi
  fi
])
