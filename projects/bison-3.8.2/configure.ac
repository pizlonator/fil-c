# Configure template for GNU Bison.                   -*-Autoconf-*-
#
# Copyright (C) 2001-2015, 2018-2021 Free Software Foundation, Inc.
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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

AC_PREREQ([2.71])
m4_pattern_forbid([^_?(gl_[A-Z]|AX_|BISON_)])
m4_pattern_allow([^BISON_USE_NLS$])

AC_INIT([GNU Bison],
        m4_esyscmd([build-aux/git-version-gen .tarball-version]),
        [bug-bison@gnu.org],
        [],
        [https://www.gnu.org/software/bison/])
AC_SUBST([PACKAGE_COPYRIGHT_YEAR], [2021])
AC_DEFINE_UNQUOTED([PACKAGE_COPYRIGHT_YEAR], [$PACKAGE_COPYRIGHT_YEAR],
                   [The copyright year for this package])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

# When we move to Automake 1.16, simplify examples/c/reccalc/local.mk.
# Our CI build the packages on Bionic, which has only Automake 1.15.
#
# We want gnits strictness only when rolling a stable release.  For
# release candidates, we use version strings like 2.4.3_rc1, but gnits
# doesn't like that, so we let the underscore disable gnits.  Between
# releases, we want to be able run make dist without being required to
# add a bogus NEWS entry.  In that case, the version string
# automatically contains a dash, which we also let disable gnits.
AM_INIT_AUTOMAKE([1.15 dist-lzip dist-xz nostdinc
                 color-tests parallel-tests
                 silent-rules]
                 m4_bmatch(m4_defn([AC_PACKAGE_VERSION]), [[-_]],
                           [gnu], [gnits]))
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([lib/config.h:lib/config.in.h])

# Checks for the compiler.
AC_PROG_CC
AC_PROG_CXX

# Gnulib (early checks).
gl_EARLY

# We want ostream_printf and hyperlink support.
gl_LIBTEXTSTYLE_OPTIONAL([0.20.5])

# Gnulib uses '#pragma GCC diagnostic push' to silence some
# warnings, but older gcc doesn't support this.
AC_CACHE_CHECK([whether pragma GCC diagnostic push works],
  [lv_cv_gcc_pragma_push_works], [
  save_CFLAGS=$CFLAGS
  CFLAGS='-Wunknown-pragmas -Werror'
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #pragma GCC diagnostic push
    #pragma GCC diagnostic pop
  ]])],
  [lv_cv_gcc_pragma_push_works=yes],
  [lv_cv_gcc_pragma_push_works=no])
  CFLAGS=$save_CFLAGS])

AC_LANG_PUSH([C++])
gl_WARN_ADD([-fno-exceptions], [NO_EXCEPTIONS_CXXFLAGS])
BISON_CXXSTD([98])
BISON_CXXSTD([03])
BISON_CXXSTD([11])
BISON_CXXSTD([14])
BISON_CXXSTD([17])
BISON_CXXSTD([20])
BISON_CXXSTD([2b])
AM_CONDITIONAL([ENABLE_CXX11], [test x"$CXX11_CXXFLAGS" != x])
AM_CONDITIONAL([ENABLE_CXX14], [test x"$CXX14_CXXFLAGS" != x])
AC_LANG_POP([C++])

AC_ARG_ENABLE([gcc-warnings],
[  --enable-gcc-warnings   turn on lots of GCC warnings (not recommended).
                           Also, issue synclines from the examples/ to
                           the corresponding source in the Texinfo doc.],
[case $enable_gcc_warnings in
   yes|no) ;;
   *)  AC_MSG_ERROR([invalid value for --gcc-warnings: $enable_gcc_warnings]);;
 esac],
              [enable_gcc_warnings=no])
AM_CONDITIONAL([ENABLE_GCC_WARNINGS], [test "$enable_gcc_warnings" = yes])
if test "$enable_gcc_warnings" = yes; then
  # -Wno-tautological-constant-out-of-range-compare for Clang 3.3 and
  # 3.4 on GNU/Linux that choke on intprops.h's INT_MULTIPLY_WRAPV,
  # etc.
  #
  # ICC: -wr188
  #
  # 1669  warnings warnings_default =
  # ../src/complain.c(318): error #188: enumerated type mixed with another type
  # 1670      Wconflicts_sr | Wconflicts_rr | Wdeprecated | Wother;
  # 1671      ^
  # 1672
  # 1673../src/complain.c(393): error #188: enumerated type mixed with another type
  # 1674          warnings w = 1 << wbit;
  # 1675                       ^
  #
  # ICC: -wr3179
  #
  # char const *usefulness
  #   = rule_useless_in_grammar_p (&rules[r]) ? "useless-in-grammar"
  #   : rule_useless_in_parser_p (&rules[r])  ? "useless-in-parser"
  #   :                                         "useful";
  #
  # gives
  #
  # error #3179: deprecated conversion of string literal to char* (should be const char*)
  #
  # ICC: -wr2259 (that's in C, in spite of what the error messages which seems to be about C++).
  # error #2259: non-pointer conversion from "int" to "yybool={signed char}" may lose significant bits
  #   yybool yynormal YY_ATTRIBUTE_UNUSED = yystackp->yysplitPoint == YY_NULLPTR;
  #                                         ^
  warn_common='-Wall -Wextra
    -Wcast-align -Wchar-subscripts
    -fparse-all-comments -Wdocumentation
    -Wformat -Wimplicit-fallthrough -Wmismatched-dealloc -Wnull-dereference
    -Wno-sign-compare -Wno-tautological-constant-out-of-range-compare
    -Wpointer-arith -Wshadow -Wstrict-aliasing
    -Wwrite-strings
    -wr188 -wr2259 -wr3179'
  warn_c='-Wbad-function-cast
    -Wmissing-prototypes
    -Wstrict-prototypes'
  warn_cxx='-Wextra-semi -Wnoexcept -Wold-style-cast -Wundefined-func-template
    -Wweak-vtables'
  # Warnings for the test suite only.
  #
  # -fno-color-diagnostics: Clang's use of colors in the error
  # messages is confusing the tests looking at the compiler's output
  # (e.g., synclines.at).
  #
  # -Wno-keyword-macro: We use the "#define private public" dirty
  # trick in the test suite to check some private implementation
  # details for lalr1.cc.
  warn_tests='-Wundef -pedantic -Wconversion
    -Wdeprecated -Wsign-compare -Wsign-conversion
    -Wtautological-constant-out-of-range-compare
    -fno-color-diagnostics
    -Wno-keyword-macro'


  AC_LANG_PUSH([C])
  # Clang supports many of GCC's -W options, but only issues warnings
  # on the ones it does not recognize.  In that case, gl_WARN_ADD
  # thinks the option is supported, and unknown options are then added
  # to CFLAGS.  But then, when -Werror is added in the test suite for
  # instance, the warning about the unknown option turns into an
  # error.
  #
  # This should be addressed by gnulib's gl_WARN_ADD, but in the
  # meanwhile, turn warnings about unknown options into errors in
  # CFLAGS, and restore CFLAGS after the tests.
  save_CFLAGS=$CFLAGS
  gl_WARN_ADD([-Werror=unknown-warning-option], [CFLAGS])
  # Accept this warning only if it is not too touchy (e.g., clang 3.3
  # and 3.4).
  gl_WARN_ADD([-Wunreachable-code], [WARN_CFLAGS],
              [AC_LANG_PROGRAM([],
              [[if (sizeof (long) < sizeof (int)) return 1;]])])
  for i in $warn_common $warn_c;
  do
    gl_WARN_ADD([$i], [WARN_CFLAGS])
  done
  gl_WARN_ADD([-Werror], [WERROR_CFLAGS])

  # Warnings for the test suite, and maybe for bison if GCC is modern
  # enough.
  test $lv_cv_gcc_pragma_push_works = yes &&
    AS_VAR_APPEND([WARN_CFLAGS], [" $WARN_CFLAGS_TEST"])

  # Warnings for the test suite only.
  for i in $warn_tests -Wincompatible-pointer-types;
  do
    gl_WARN_ADD([$i], [WARN_CFLAGS_TEST])
  done
  CFLAGS=$save_CFLAGS
  AC_LANG_POP([C])


  AC_LANG_PUSH([C++])
  save_CXXFLAGS=$CXXFLAGS
  gl_WARN_ADD([-Werror=unknown-warning-option], [CXXFLAGS])
  for i in $warn_common $warn_cxx;
  do
    gl_WARN_ADD([$i], [WARN_CXXFLAGS])
  done
  # Accept this warning only if it is not too touchy (e.g., clang 3.3
  # and 3.4).
  gl_WARN_ADD([-Wunreachable-code], [WARN_CXXFLAGS],
              [AC_LANG_PROGRAM([],
              [[if (sizeof (long) < sizeof (int)) return 1;]])])
  gl_WARN_ADD([-Wzero-as-null-pointer-constant], [WARN_CXXFLAGS],
              [AC_LANG_PROGRAM([], [nullptr])])
  # Before GCC6, the pragmas don't work well enough to neutralize
  # this warning.
  gl_WARN_ADD([-Wuseless-cast], [WARN_CXXFLAGS],
              [AC_LANG_PROGRAM([], [
              #if defined __GNUC__ && ! defined __ICC && ! defined __clang__ && __GNUC__ < 6
              syntax error
              #endif
              ])])
  gl_WARN_ADD([-Werror], [WERROR_CXXFLAGS])
  # Warnings for the test suite only.
  for i in $warn_tests;
  do
    gl_WARN_ADD([$i], [WARN_CXXFLAGS_TEST])
  done
  # Too many compilers complain about Flex generated code.
  gl_WARN_ADD([-Wno-error], [FLEX_SCANNER_CXXFLAGS])
  # Clang++ deprecates compiling C.
  gl_WARN_ADD([-Wno-deprecated], [WNO_DEPRECATED_CXXFLAGS])
  CXXFLAGS=$save_CXXFLAGS
  AC_LANG_POP([C++])
fi


BISON_TEST_FOR_WORKING_C_COMPILER
BISON_C_COMPILER_POSIXLY_CORRECT
BISON_TEST_FOR_WORKING_CXX_COMPILER
BISON_CXX_COMPILER_POSIXLY_CORRECT

# D.
AC_CHECK_PROGS([DC], [dmd])
AC_CHECK_PROGS([DCFLAGS], [-g])
AM_CONDITIONAL([ENABLE_D], [test x"$DC" != x])

# Java.
gt_JAVACOMP([1.7], [1.7])
gt_JAVAEXEC
AM_CONDITIONAL([ENABLE_JAVA], [test x"$CONF_JAVAC" != x && test x"$CONF_JAVA" != x])


AC_ARG_ENABLE([yacc],
  [AS_HELP_STRING([--disable-yacc],
     [do not build a yacc command or an -ly library])],
  , [enable_yacc=yes])
AM_CONDITIONAL([ENABLE_YACC], [test "$enable_yacc" = yes])
AC_CONFIG_FILES([src/yacc], [chmod +x src/yacc])

# Checks for programs.
AM_MISSING_PROG([DOT], [dot])
AC_PROG_LEX
if ! "$LEX_IS_FLEX" || test "X$LEX" = X:; then
  AC_MSG_WARN([bypassing lex because flex is required])
  LEX=:
fi
AM_CONDITIONAL([FLEX_WORKS], [$LEX_IS_FLEX && $FLEX_SUPPORTS_HEADER_OPT])
AM_CONDITIONAL([FLEX_CXX_WORKS],
  [$LEX_IS_FLEX && $FLEX_SUPPORTS_HEADER_OPT && test $bison_cv_cxx_works = yes])
AC_PROG_YACC
AC_PROG_RANLIB
AC_PROG_GNU_M4
AC_DEFINE_UNQUOTED([M4], ["$M4"], [Define to the GNU M4 executable name.])
AC_DEFINE_UNQUOTED([M4_GNU_OPTION], ["$M4_GNU"], [Define to "-g" if GNU M4
supports -g, otherwise to "".])
AC_PATH_PROG([PERL], [perl])
AM_MISSING_PROG([HELP2MAN], [help2man])
AC_PATH_PROG([XSLTPROC], [xsltproc])
AC_SUBST([XSLTPROC])

# Checks for header files.
AC_CHECK_HEADERS_ONCE([locale.h])

# Checks for compiler characteristics.
AC_C_INLINE

# Gnulib (later checks).  Putting them here rather than right after
# gl_EARLY avoids some redundant checks.
gl_INIT

# Checks for library functions.
AC_CHECK_FUNCS_ONCE([setlocale])

# Gettext.
# We use gnulib, which is only guaranteed to work properly with the
# latest Gettext.
AM_GNU_GETTEXT([external], [need-ngettext])
AM_GNU_GETTEXT_VERSION([0.19])
BISON_I18N
AC_CONFIG_FILES([gnulib-po/Makefile.in])

# Internationalized parsers.
AC_CONFIG_FILES([runtime-po/Makefile.in])
# Autoconf macros for packages using internationalized parsers.
aclocaldir='${datadir}/aclocal'
AC_SUBST([aclocaldir])

# Create the benchmark script.
AC_CONFIG_FILES([etc/bench.pl], [chmod +x etc/bench.pl])

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/atlocal])
AC_CONFIG_FILES([tests/bison], [chmod +x tests/bison])

AC_CHECK_PROGS([VALGRIND], [valgrind])
# Use something simpler that $host_os to select our suppression file.
uname=`uname`
case $VALGRIND:$uname in
  '':*) ;;
  *:Darwin)
    # See README-hacking.md.
    VALGRIND=;;
  *:*)
    suppfile=build-aux/$uname.valgrind
    if test -f "$srcdir/$suppfile"; then
      AC_SUBST([VALGRIND_OPTS_SUPPRESSION],
               ["--suppressions=\$(abs_top_srcdir)/$suppfile"])
    fi
    ;;
esac
AC_MSG_CHECKING([Valgrind suppression file])
AC_MSG_RESULT([$suppfile])

# Whether we cannot run the compiled bison.
AM_CONDITIONAL([CROSS_COMPILING], [test "$cross_compiling" = yes])

# Build bistromathic if we have the lib and both readline/readline.h
# and readline/history.h.
AM_CONDITIONAL([ENABLE_BISTROMATHIC],
[test "$gl_cv_lib_readline" != no &&
  test "$ac_cv_header_readline_readline_h" = yes &&
  test "$ac_cv_header_readline_history_h" = yes])

AM_MISSING_PROG([AUTOM4TE], [autom4te])
# Needed by tests/atlocal.in.
AC_SUBST([GCC])

AC_CONFIG_FILES([Makefile
                 po/Makefile.in
                 doc/yacc.1])

# Fix LIBOBJS to give the Makefile the right file names.  Otherwise
# compilation works, but with unexpected file names, so clean rules don't
# remove the actual files and distcheck fails.
AC_CONFIG_COMMANDS_PRE([
  case $am__api_version in
    1.14*|1.15*) gl_LIBOBJS=`echo "$gl_LIBOBJS" | sed -e 's, lib/, lib/lib_libbison_a-,g'`;;
    *)           gl_LIBOBJS=`echo "$gl_LIBOBJS" | sed -e 's, lib/, lib/libbison_a-,g'`;;
  esac
])

AC_OUTPUT
