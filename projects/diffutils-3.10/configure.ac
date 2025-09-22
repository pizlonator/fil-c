# Configure template for GNU Diffutils.

# Copyright (C) 1994-1995, 1998, 2001-2002, 2004, 2006, 2009-2013, 2015-2023
# Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.64])

AC_INIT([GNU diffutils],
        m4_esyscmd([build-aux/git-version-gen .tarball-version]),
        [bug-diffutils@gnu.org])

AC_CONFIG_SRCDIR([src/diff.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip subdir-objects
                       color-tests parallel-tests])
AM_SILENT_RULES([yes]) # make --enable-silent-rules the default.

AC_CONFIG_HEADERS([lib/config.h:lib/config.hin])

AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_C_O
AM_MISSING_PROG([HELP2MAN], [help2man])
AC_PROG_RANLIB
gl_EARLY
gl_USE_SYSTEM_EXTENSIONS
gl_INIT

# Ensure VLAs are not used.
# Note -Wvla is implicitly added by gl_MANYWARN_ALL_GCC
AC_DEFINE([GNULIB_NO_VLA], [1], [Define to 1 to disable use of VLAs])

AC_ARG_ENABLE([gcc-warnings],
  [AS_HELP_STRING([--enable-gcc-warnings],
                  [turn on lots of GCC warnings (for developers)])],
  [case $enableval in
     yes|no) ;;
     *)      AC_MSG_ERROR([bad value $enableval for gcc-warnings option]) ;;
   esac
   gl_gcc_warnings=$enableval],
  [if test -d "$srcdir"/.git; then
     gl_gcc_warnings=yes
   else
     gl_gcc_warnings=no
   fi]
)

if test "$gl_gcc_warnings" = yes; then
  gl_WARN_ADD([-Werror], [WERROR_CFLAGS])
  AC_SUBST([WERROR_CFLAGS])

  nw=
  # This, $nw, is the list of warnings we disable.
  nw="$nw -Winline"                 # system.h's readdir_ignoring_dot_and_dotdot
  nw="$nw -Wstack-protector"        # not worth working around

  gl_MANYWARN_ALL_GCC([ws])
  gl_MANYWARN_COMPLEMENT([ws], [$ws], [$nw])
  for w in $ws; do
    gl_WARN_ADD([$w])
  done
  gl_WARN_ADD([-Wno-sign-compare])     # Too many warnings for now
  gl_WARN_ADD([-Wno-unused-parameter]) # Too many warnings for now
  gl_WARN_ADD([-Wno-format-nonliteral])

  gl_WARN_ADD([-fdiagnostics-show-option])
  gl_WARN_ADD([-funit-at-a-time])
  gl_WARN_ADD([-fno-common])

  AC_SUBST([WARN_CFLAGS])

  AC_DEFINE([GCC_LINT], [1],
    [Define to 1 if the compiler is checking for lint.])
  AC_DEFINE([_FORTIFY_SOURCE], [2],
    [enable compile-time and run-time bounds-checking, and some warnings])
  AC_DEFINE([GNULIB_PORTCHECK], [1], [enable some gnulib portability checks])

  # We use a slightly smaller set of warning options for lib/.
  # Remove the following and save the result in GNULIB_WARN_CFLAGS.
  nw=
  nw="$nw -Wunused-macros"
  nw="$nw -Wsuggest-attribute=pure"
  nw="$nw -Wduplicated-branches"    # Too many false alarms

  gl_MANYWARN_COMPLEMENT([GNULIB_WARN_CFLAGS], [$WARN_CFLAGS], [$nw])
  AC_SUBST([GNULIB_WARN_CFLAGS])
fi

AC_C_INLINE

AC_DEFINE([DEFAULT_EDITOR_PROGRAM], ["ed"],
  [Name of editor program, unless overridden.])

AC_PATH_PROG([PR_PROGRAM], [pr], [""])
AC_DEFINE_UNQUOTED([PR_PROGRAM], ["$PR_PROGRAM"], [Name of "pr" program.])

AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_TYPE_PID_T

AC_CHECK_FUNCS_ONCE([sigaction sigprocmask strcasecoll stricoll])
if test $ac_cv_func_sigprocmask = no; then
  AC_CHECK_FUNCS([sigblock])
fi
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK

# When .tarball-version exists, we're building from a tarball
# and must not make man/*.1 files depend on the generated src/version.c,
# because that would induce a requirement to run the help2man perl script.
# We are not yet prepared to make perl a build-from-tarball requirement.
# Hence, here we detect .tarball-version existence.  When not present,
# we define a variable to be used in man/Makefile.am to induce the
# proper dependency (so that man/*.1 will be rebuilt upon any version change),
# but not when built from a tarball.
AC_SUBST([SRC_VERSION_C])
test -f $srcdir/.tarball-version \
  && SRC_VERSION_C= \
  || SRC_VERSION_C=../src/version.c

AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION([0.19.2])
XGETTEXT="AWK='$AWK' \$(SHELL) \$(top_srcdir)/exgettext $XGETTEXT"

AC_CONFIG_FILES([
  Makefile doc/Makefile
  lib/Makefile
  src/Makefile
  tests/Makefile
  gnulib-tests/Makefile
  man/Makefile
  po/Makefile.in
])
AC_OUTPUT
