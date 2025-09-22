dnl Process this file with autoconf to produce a configure script.
# Copyright (C) 2002-2022 Free Software Foundation, Inc.
#
# This file is part of LIBTASN1.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([GNU Libtasn1],
        m4_esyscmd([build-aux/git-version-gen .tarball-version]),
        [help-libtasn1@gnu.org])

# Library code modified:                              REVISION++
# Interfaces changed/added/removed:   CURRENT++       REVISION=0
# Interfaces added:                             AGE++
# Interfaces removed:                           AGE=0
AC_SUBST(LT_CURRENT, 12)
AC_SUBST(LT_REVISION, 3)
AC_SUBST(LT_AGE, 6)

AC_SUBST([MAJOR_VERSION], [`echo $PACKAGE_VERSION|cut -d'.' -f1`])
AC_SUBST([MINOR_VERSION], [`echo $PACKAGE_VERSION|cut -d'.' -f2`])
AC_SUBST([PATCH_VERSION], [`echo $PACKAGE_VERSION.0|cut -d. -f3|cut -d- -f1`])
AC_SUBST([NUMBER_VERSION], [`printf '0x%02x%02x%02x' $MAJOR_VERSION $MINOR_VERSION $PATCH_VERSION`])

AC_MSG_NOTICE([$PACKAGE_NAME $PACKAGE_VERSION $PACKAGE_TARNAME $MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION $NUMBER_VERSION $LT_CURRENT:$LT_REVISION:$LT_AGE $DLL_VERSION])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE([-Wall -Wno-portability])
AM_SILENT_RULES([yes])

AC_PROG_CC
gl_EARLY
sgl_EARLY
AC_PROG_YACC

AC_ARG_ENABLE(doc,
  AS_HELP_STRING([--disable-doc], [don't generate any documentation]),
    enable_doc=$enableval, enable_doc=yes)
AM_CONDITIONAL(ENABLE_DOC, test "$enable_doc" != "no")

AC_CHECK_FUNCS([clock_gettime fmemopen])

AM_MISSING_PROG(HELP2MAN, help2man, $missing_dir)
LT_INIT([win32-dll])
GTK_DOC_CHECK(1.2)

AC_CHECK_SIZEOF(unsigned long int, 4)
AC_CHECK_SIZEOF(unsigned int, 4)

gl_INIT
sgl_INIT

AX_CODE_COVERAGE

AC_ARG_ENABLE([gcc-warnings],
  [AS_HELP_STRING([--disable-gcc-warnings],
    [disable GCC warnings (for developers)])],
  [case $enableval in
     yes|no) ;;
     *)      AC_MSG_ERROR([bad value $enableval for gcc-warnings option]) ;;
   esac
   gl_gcc_warnings=$enableval],
  [gl_gcc_warnings=yes]
)

if test "$gl_gcc_warnings" = yes; then
  gl_MANYWARN_ALL_GCC([warnings])
  nw=
  nw="$nw -Wsystem-headers"       # Don't let system headers trigger warnings
  nw="$nw -Wc++-compat"           # We don't care strongly about C++ compilers
  nw="$nw -Wundef"                # All compiler preprocessors support #if UNDEF
  nw="$nw -Wtraditional"          # All compilers nowadays support ANSI C
  nw="$nw -Wconversion"           # These warnings usually don't point to mistakes.
  nw="$nw -Wpadded"               # Padding internal structs doesn't help.
  nw="$nw -Wunused-macros"        # Lot's of warnings in ASN1.y
  nw="$nw -Woverlength-strings"   # Modern compilers handle strings > 4096 gracefully
  nw="$nw -Wsuggest-attribute=pure" # Asked for several nodes handling functions

  gl_MANYWARN_COMPLEMENT([warnings], [$warnings], [$nw])
  for w in $warnings; do
    gl_WARN_ADD([$w])
  done

  gl_WARN_ADD([-Wno-type-limits]) # Too many warnings from gnulib macros
  gl_WARN_ADD([-Wformat-truncation=1])
  gl_WARN_ADD([-Wimplicit-fallthrough=2])
  gl_WARN_ADD([-Wno-unused-parameter])
  gl_WARN_ADD([-fdiagnostics-show-option])
  gl_WARN_ADD([-fdiagnostics-color=always])      # colors even when using ccache
fi

AC_ARG_ENABLE([fuzzing],
  [AS_HELP_STRING([--enable-fuzzing], [Turn on fuzzing build (for developers)])],
  [enable_fuzzing=yes; AC_SUBST([LIB_FUZZING_ENGINE])], [enable_fuzzing=no; LIB_FUZZING_ENGINE=""])
AM_CONDITIONAL([FUZZING], [test "$enable_fuzzing" = "yes"])

OLD_LIBS=$LIBS
AC_SEARCH_LIBS([dlsym], [dl dld])
FUZZ_LIBS=$LIBS
LIBS=$OLD_LIBS
AC_SUBST([FUZZ_LIBS])

AC_CONFIG_FILES([
  Makefile
  doc/Makefile
  doc/reference/Makefile
  examples/Makefile
  fuzz/Makefile
  lib/Makefile
  lib/gl/Makefile
  lib/includes/libtasn1.h
  lib/libtasn1.pc
  src/Makefile
  src/gl/Makefile
  tests/Makefile
])
AC_OUTPUT

AC_MSG_NOTICE([Summary of build options:

  Version:           ${VERSION}
  Libtool version    $LT_CURRENT:$LT_REVISION:$LT_AGE
  DLL version:       $DLL_VERSION
  Header version:    major $MAJOR_VERSION minor $MINOR_VERSION patch $PATCH_VERSION number $NUMBER_VERSION
  Build/host system: ${build} / ${host}
  Install prefix:    ${prefix}
  Compiler:          ${CC}
  Warning flags:     ${WARN_CFLAGS}
  CFLAGS:            ${CFLAGS} ${CPPFLAGS}
  LDFlags:           ${LDFLAGS}
  Documentation:     ${enable_doc}
  Library types:     Shared=${enable_shared}, Static=${enable_static}
  Valgrind:          $gl_cv_prog_valgrind_works ${VALGRIND}
  Version script:    $have_ld_version_script
  Fuzzing build:     $enable_fuzzing $LIB_FUZZING_ENGINE
])
