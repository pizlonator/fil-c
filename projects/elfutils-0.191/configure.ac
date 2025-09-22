dnl Process this file with autoconf to produce a configure script.
dnl Configure input file for elfutils.                     -*-autoconf-*-
dnl
dnl Copyright (C) 1996-2019 Red Hat, Inc.
dnl Copyright (C) 2022, 2023 Mark J. Wielaard <mark@klomp.org>
dnl
dnl This file is part of elfutils.
dnl
dnl  This file is free software; you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation; either version 3 of the License, or
dnl  (at your option) any later version.
dnl
dnl  elfutils is distributed in the hope that it will be useful, but
dnl  WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with this program.  If not, see <http://www.gnu.org/licenses/>.
AC_INIT([elfutils],[0.191],[https://sourceware.org/bugzilla],[elfutils],[http://elfutils.org/])

dnl Workaround for older autoconf < 2.64
m4_ifndef([AC_PACKAGE_URL],
         [AC_DEFINE([PACKAGE_URL], ["http://elfutils.org/"],
                    [Define to home page for this package])
          AC_SUBST([PACKAGE_URL], ["http://elfutils.org/"])])

LIBDEBUGINFOD_SONAME=libdebuginfod.so.1
AC_SUBST([LIBDEBUGINFOD_SONAME])

# We want eu- as default program prefix if none was given by the user.
# But if the user explicitly provided --program-prefix="" then pretend
# it wasn't set at all (NONE). We want to test this really early before
# configure has a chance to use the value.

if test "x$program_prefix" = "xNONE"; then
  AC_MSG_NOTICE([No --program-prefix given, using "eu-"])
  program_prefix="eu-"
elif test "x$program_prefix" = "x"; then
  AC_MSG_NOTICE([Using no program-prefix])
  program_prefix=NONE
fi

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_FILES([config/Makefile])

AC_COPYRIGHT([Copyright (C) 1996-2024 The elfutils developers.])
AC_PREREQ(2.63)			dnl Minimum Autoconf version required.

dnl We use GNU make extensions; automake 1.10 defaults to -Wportability.
AM_INIT_AUTOMAKE([gnits 1.11 -Wno-portability dist-bzip2 no-dist-gzip parallel-tests])
AM_MAINTAINER_MODE

AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([libelf/libelf.h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([config.h])

dnl The RPM spec file.  We substitute a few values in the file.
AC_CONFIG_FILES([elfutils.spec:config/elfutils.spec.in])

dnl debuginfo-server client & server parts.
AC_CONFIG_FILES([debuginfod/Makefile debuginfod/debuginfod.h])

AC_CANONICAL_HOST

AC_ARG_ENABLE(deterministic-archives,
[AS_HELP_STRING([--enable-deterministic-archives],
		[ar and ranlib default to -D behavior])], [
if test "${enableval}" = no; then
  default_ar_deterministic=false
else
  default_ar_deterministic=true
fi], [default_ar_deterministic=false])
AC_DEFINE_UNQUOTED(DEFAULT_AR_DETERMINISTIC, $default_ar_deterministic,
		   [Should ar and ranlib use -D behavior by default?])

AC_ARG_ENABLE([thread-safety],
AS_HELP_STRING([--enable-thread-safety],
               [enable thread safety of libraries EXPERIMENTAL]),
               use_locks=$enableval, use_locks=no)
AM_CONDITIONAL(USE_LOCKS, test "$use_locks" = yes)
AS_IF([test "$use_locks" = yes], [AC_DEFINE(USE_LOCKS)])
AS_IF([test "$use_locks" = yes],
      [AC_MSG_WARN([thread-safety is EXPERIMENTAL tests might fail.])])

AH_TEMPLATE([USE_LOCKS], [Defined if libraries should be thread-safe.])

m4_version_prereq([2.70], [AC_PROG_CC], [AC_PROG_CC_C99])
AC_PROG_CXX
AC_PROG_RANLIB
AC_PROG_YACC
AC_PROG_LEX([noyywrap])
# Only available since automake 1.12
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_CHECK_TOOL([READELF], [readelf])
AC_CHECK_TOOL([NM], [nm])

AC_CACHE_CHECK([whether gcc supports __attribute__((visibility()))],
	ac_cv_visibility, [dnl
save_CFLAGS="$CFLAGS"
CFLAGS="$save_CFLAGS -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([dnl
int __attribute__((visibility("hidden")))
foo (int a)
{
  return a;
}])], ac_cv_visibility=yes, ac_cv_visibility=no)
CFLAGS="$save_CFLAGS"])
if test "$ac_cv_visibility" = "yes"; then
	AC_DEFINE([HAVE_VISIBILITY], [1],
		  [Defined if __attribute__((visibility())) is supported])
fi

AC_CACHE_CHECK([whether gcc supports __attribute__((gcc_struct))],
	ac_cv_gcc_struct, [dnl
save_CFLAGS="$CFLAGS"
CFLAGS="$save_CFLAGS -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([dnl
struct test { int x; } __attribute__((gcc_struct));
])], ac_cv_gcc_struct=yes, ac_cv_gcc_struct=no)
CFLAGS="$save_CFLAGS"])
if test "$ac_cv_gcc_struct" = "yes"; then
	AC_DEFINE([HAVE_GCC_STRUCT], [1],
		  [Defined if __attribute__((gcc_struct)) is supported])
fi

AC_CACHE_CHECK([whether gcc supports -fPIC], ac_cv_fpic, [dnl
save_CFLAGS="$CFLAGS"
CFLAGS="$save_CFLAGS -fPIC -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE()], ac_cv_fpic=yes, ac_cv_fpic=no)
CFLAGS="$save_CFLAGS"
])
if test "$ac_cv_fpic" = "yes"; then
	fpic_CFLAGS="-fPIC"
else
	fpic_CFLAGS=""
fi
AC_SUBST([fpic_CFLAGS])

AC_CACHE_CHECK([whether gcc supports -fPIE], ac_cv_fpie, [dnl
save_CFLAGS="$CFLAGS"
CFLAGS="$save_CFLAGS -fPIE -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE()], ac_cv_fpie=yes, ac_cv_fpie=no)
CFLAGS="$save_CFLAGS"
])
if test "$ac_cv_fpie" = "yes"; then
	fpie_CFLAGS="-fPIE"
else
	fpie_CFLAGS=""
fi
AC_SUBST([fpie_CFLAGS])

dso_LDFLAGS="-shared"

NO_UNDEFINED=-Wl,--no-undefined
AC_ARG_ENABLE([sanitize-memory],
              AS_HELP_STRING([--enable-sanitize-memory],
                             [Use clang memory sanitizer]),
                             [use_msan=$enableval], [use_msan=no])
if test "$use_msan" = yes; then
  old_CFLAGS="$CFLAGS"
  old_CXXFLAGS="$CXXFLAGS"
  old_LDFLAGS="$LDFLAGS"
  # -fsanitize=memory is not compatible with -D_FORTIFY_SOURCE, -Wl,-z,defs and --no-undefined
  CFLAGS="$CFLAGS -fsanitize=memory -fsanitize-memory-track-origins -D_FORTIFY_SOURCE=0"
  CXXFLAGS="$CXXFLAGS -fsanitize=memory -fsanitize-memory-track-origins -D_FORTIFY_SOURCE=0"
  LDFLAGS="-shared"
  AC_LINK_IFELSE([AC_LANG_SOURCE([int main (int argc, char **argv) { return 0; }])], use_msan=yes, use_msan=no)
  AS_IF([test "x$use_msan" = xyes],
        ac_cv_zdefs=no NO_UNDEFINED=,
        AC_MSG_WARN([clang memory sanitizer not available])
        CFLAGS="$old_CFLAGS" CXXFLAGS="$old_CXXFLAGS")
  LDFLAGS="$old_LDFLAGS"
fi
AC_SUBST(NO_UNDEFINED)
AM_CONDITIONAL(USE_MEMORY_SANITIZER, test "$use_msan" = yes)

ZDEFS_LDFLAGS="-Wl,-z,defs"
AC_CACHE_CHECK([whether gcc supports $ZDEFS_LDFLAGS], ac_cv_zdefs, [dnl
save_LDFLAGS="$LDFLAGS"
LDFLAGS="$ZDEFS_LDFLAGS $save_LDFLAGS"
AC_LINK_IFELSE([AC_LANG_PROGRAM()], ac_cv_zdefs=yes, ac_cv_zdefs=no)
LDFLAGS="$save_LDFLAGS"
])
if test "$ac_cv_zdefs" = "yes"; then
	dso_LDFLAGS="$dso_LDFLAGS $ZDEFS_LDFLAGS"
fi

# We really want build-ids. Warn and force generating them if gcc was
# configure without --enable-linker-build-id
AC_CACHE_CHECK([whether the compiler generates build-ids], ac_cv_buildid, [dnl
AC_LINK_IFELSE([AC_LANG_PROGRAM()],[ac_cv_buildid=yes; $READELF -n conftest$EXEEXT | grep -q NT_GNU_BUILD_ID || ac_cv_buildid=no],AC_MSG_FAILURE([unexpected compile failure]))])
if test "$ac_cv_buildid" = "no"; then
	AC_MSG_WARN([compiler doesn't generate build-id by default])
	LDFLAGS="$LDFLAGS -Wl,--build-id"
fi

ZRELRO_LDFLAGS="-Wl,-z,relro"
AC_CACHE_CHECK([whether gcc supports $ZRELRO_LDFLAGS], ac_cv_zrelro, [dnl
save_LDFLAGS="$LDFLAGS"
LDFLAGS="$ZRELRO_LDFLAGS $save_LDFLAGS"
AC_LINK_IFELSE([AC_LANG_PROGRAM()], ac_cv_zrelro=yes, ac_cv_zrelro=no)
LDFLAGS="$save_LDFLAGS"
])
if test "$ac_cv_zrelro" = "yes"; then
	dso_LDFLAGS="$dso_LDFLAGS $ZRELRO_LDFLAGS"
fi

AC_SUBST([dso_LDFLAGS])

AC_CACHE_CHECK([for __thread support], ac_cv_tls, [dnl
# Use the same flags that we use for our DSOs, so the test is representative.
# Some old compiler/linker/libc combinations fail some ways and not others.
save_CFLAGS="$CFLAGS"
save_LDFLAGS="$LDFLAGS"
CFLAGS="$fpic_CFLAGS $CFLAGS"
LDFLAGS="$dso_LDFLAGS $LDFLAGS"
AC_LINK_IFELSE([dnl
AC_LANG_PROGRAM([[#include <stdlib.h>
#undef __thread
static __thread int a; int foo (int b) { return a + b; }]],
		[[exit (foo (0));]])],
	       ac_cv_tls=yes, ac_cv_tls=no)
CFLAGS="$save_CFLAGS"
LDFLAGS="$save_LDFLAGS"])
AS_IF([test "x$ac_cv_tls" != xyes],
      AC_MSG_ERROR([__thread support required]))

dnl Before 4.9 gcc doesn't ship stdatomic.h, but the necessary atomics are
dnl available by (at least) 4.7. So if the system doesn't have a stdatomic.h we
dnl fall back on one copied from FreeBSD that handles the difference.
AC_CACHE_CHECK([whether gcc provides stdatomic.h], ac_cv_has_stdatomic,
  [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <stdatomic.h>]])],
		     ac_cv_has_stdatomic=yes, ac_cv_has_stdatomic=no)])
AM_CONDITIONAL(HAVE_STDATOMIC_H, test "x$ac_cv_has_stdatomic" = xyes)
AS_IF([test "x$ac_cv_has_stdatomic" = xyes], [AC_DEFINE(HAVE_STDATOMIC_H)])

AH_TEMPLATE([HAVE_STDATOMIC_H], [Define to 1 if `stdatomic.h` is provided by the
                                 system, 0 otherwise.])

dnl This test must come as early as possible after the compiler configuration
dnl tests, because the choice of the file model can (in principle) affect
dnl whether functions and headers are available, whether they work, etc.
AC_SYS_LARGEFILE

dnl Older glibc had a broken fts that didn't work with Large File Systems.
dnl We want the version that can handler LFS, but include workaround if we
dnl get a bad one. Add define to CFLAGS (not AC_DEFINE it) since we need to
dnl check it before including config.h (which might define _FILE_OFFSET_BITS).
AC_CACHE_CHECK([whether fts.h is bad when included (with LFS)], ac_cv_bad_fts,
  [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <fts.h>]])],
		     ac_cv_bad_fts=no, ac_cv_bad_fts=yes)])
AS_IF([test "x$ac_cv_bad_fts" = "xyes"],
      [CFLAGS="$CFLAGS -DBAD_FTS=1" CXXFLAGS="$CXXFLAGS -DBAD_FTS=1"])

# See if we can add -D_FORTIFY_SOURCE=2 or =3. Don't do it if it is already
# (differently) defined or if it generates warnings/errors because we
# don't use the right optimisation level (string.h will warn about that).
AC_MSG_CHECKING([whether to add -D_FORTIFY_SOURCE=2 or =3 to CFLAGS])
case "$CFLAGS" in
  *-D_FORTIFY_SOURCE=*)
    AC_MSG_RESULT([no, already there])
    ;;
  *)
    save_CFLAGS="$CFLAGS"
    # Try 3 first.
    CFLAGS="-D_FORTIFY_SOURCE=3 $save_CFLAGS -Werror"
    fortified_cflags=""
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
      #include <string.h>
      int main() { return 0; }
    ]])], [ AC_MSG_RESULT([yes -D_FORTIFY_SOURCE=3])
            fortified_cflags="-D_FORTIFY_SOURCE=3" ], [])

    # If that didn't work, try 2.
    if test -z "$fortified_cflags"; then
      CFLAGS="-D_FORTIFY_SOURCE=2 $save_CFLAGS -Werror"
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
        #include <string.h>
        int main() { return 0; }
      ]])], [ AC_MSG_RESULT([yes -D_FORTIFY_SOURCE=2])
              fortified_cflags="-D_FORTIFY_SOURCE=2" ],
              [ AC_MSG_RESULT([no, cannot be used])])
    fi
    CFLAGS="$fortified_cflags $save_CFLAGS"
    CXXFLAGS="$fortified_cflags $CXXFLAGS"
    ;;
esac

dnl enable debugging of branch prediction.
AC_ARG_ENABLE([debugpred],
AS_HELP_STRING([--enable-debugpred],[build binaries with support to debug branch prediction]),
[use_debugpred=$enableval], [use_debugpred=no])
case $use_debugpred in
 yes) use_debugpred_val=1 ;;
 *)   use_debugpred_val=0 ;;
esac
AC_SUBST([DEBUGPRED], $use_debugpred_val)

dnl Enable gprof support.
AC_ARG_ENABLE([gprof],
AS_HELP_STRING([--enable-gprof],[build binaries with gprof support]), [use_gprof=$enableval], [use_gprof=no])
if test "$use_gprof" = yes; then
  CFLAGS="$CFLAGS -pg"
  LDFLAGS="$LDFLAGS -pg"
fi
AM_CONDITIONAL(GPROF, test "$use_gprof" = yes)

# Enable gcov support.
AC_ARG_ENABLE([gcov],
AS_HELP_STRING([--enable-gcov],[build binaries with gcov support]), [use_gcov=$enableval], [use_gcov=no])
if test "$use_gcov" = yes; then
  CFLAGS="$CFLAGS -fprofile-arcs -ftest-coverage"
  CXXFLAGS="$CXXFLAGS -fprofile-arcs -ftest-coverage"
  LDFLAGS="$LDFLAGS -fprofile-arcs"
  AC_CHECK_PROG([GCOV], [gcov], [gcov])
  AC_CHECK_PROG([LCOV], [lcov], [lcov])
  AC_CHECK_PROG([GENHTML], [genhtml], [genhtml])
fi
AM_CONDITIONAL(GCOV, test "$use_gcov" = yes)

AC_ARG_ENABLE([sanitize-undefined],
              AS_HELP_STRING([--enable-sanitize-undefined],
                             [Use gcc undefined behaviour sanitizer]),
                             [use_undefined=$enableval], [use_undefined=no])
if test "$use_undefined" = yes; then
  old_CFLAGS="$CFLAGS"
  old_CXXFLAGS="$CXXFLAGS"
  # We explicitly use unaligned access when possible (see ALLOW_UNALIGNED)
  # We want to fail immediately on first error, don't try to recover.
  CFLAGS="$CFLAGS -fsanitize=undefined -fno-sanitize-recover"
  CXXFLAGS="$CXXFLAGS -fsanitize=undefined -fno-sanitize-recover"
  AC_LINK_IFELSE([AC_LANG_SOURCE([int main (int argc, char **argv) { return 0; }])], use_undefined=yes, use_undefined=no)
  AS_IF([test "x$use_undefined" != xyes],
        AC_MSG_WARN([gcc undefined behaviour sanitizer not available])
        CFLAGS="$old_CFLAGS" CXXFLAGS="$old_CXXFLAGS")
fi
case $use_undefined in
 yes) check_undefined_val=1 ;;
 *)   check_undefined_val=0 ;;
esac
AC_DEFINE_UNQUOTED(CHECK_UNDEFINED, $check_undefined_val,
		   [Building with -fsanitize=undefined or not])

AC_ARG_ENABLE([sanitize-address],
              AS_HELP_STRING([--enable-sanitize-address],
                             [Use gcc address sanitizer]),
                             [use_address=$enableval], [use_address=no])
if test "$use_address" = yes; then
  old_CFLAGS="$CFLAGS"
  old_CXXFLAGS="$CXXFLAGS"
  old_LDFLAGS="$LDFLAGS"
  # We want to fail immediately on first error, don't try to recover.
  CFLAGS="$CFLAGS -fsanitize=address -fno-sanitize-recover"
  CXXFLAGS="$CXXFLAGS -fsanitize=address -fno-sanitize-recover"
  # Some compilers don't handle -fsanatize=address correctly with --no-undefined
  LDFLAGS="-Wl,-z,defs -shared"
  AC_LINK_IFELSE([AC_LANG_SOURCE([int main (int argc, char **argv) { return 0; }])], use_address=yes, use_address=no)
  AS_IF([test "x$use_address" != xyes],
        AC_MSG_WARN([gcc address sanitizer not available])
        CFLAGS="$old_CFLAGS" CXXFLAGS="$old_CXXFLAGS")
  LDFLAGS="$old_LDFLAGS"
fi
AM_CONDITIONAL(USE_ADDRESS_SANITIZER, test "$use_address" = yes)

AC_ARG_ENABLE([valgrind],
AS_HELP_STRING([--enable-valgrind],[run all tests under valgrind]),
[use_valgrind=$enableval], [use_valgrind=no])
if test "$use_valgrind" = yes; then
  if test "$use_address" = yes; then
    AC_MSG_ERROR([cannot enable valgrind and sanitize address together])
  fi
  AC_CHECK_PROG(HAVE_VALGRIND, valgrind, yes, no)
  if test "$HAVE_VALGRIND" = "no"; then
    AC_MSG_ERROR([valgrind not found])
  fi
fi
AM_CONDITIONAL(USE_VALGRIND, test "$use_valgrind" = yes)

AC_ARG_WITH([valgrind],
AS_HELP_STRING([--with-valgrind],[include directory for Valgrind headers]),
[with_valgrind_headers=$withval], [with_valgrind_headers=no])
if test "x$with_valgrind_headers" != xno; then
    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -I$with_valgrind_headers"
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
      #include <valgrind/valgrind.h>
      int main() { return 0; }
    ]])], [ HAVE_VALGRIND_HEADERS="yes"
            CFLAGS="$save_CFLAGS -I$with_valgrind_headers" ],
          [ AC_MSG_ERROR([invalid valgrind include directory: $with_valgrind_headers]) ])
fi

AC_ARG_ENABLE([valgrind-annotations],
AS_HELP_STRING([--enable-valgrind-annotations],[insert extra annotations for better valgrind support]),
[use_vg_annotations=$enableval], [use_vg_annotations=no])
if test "$use_vg_annotations" = yes; then
    if test "x$HAVE_VALGRIND_HEADERS" != "xyes"; then
      AC_MSG_CHECKING([whether Valgrind headers are available])
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
        #include <valgrind/valgrind.h>
        int main() { return 0; }
      ]])], [ AC_MSG_RESULT([yes]) ],
            [ AC_MSG_ERROR([valgrind annotations requested but no headers are available]) ])
    fi
fi
AM_CONDITIONAL(USE_VG_ANNOTATIONS, test "$use_vg_annotations" = yes)

AC_ARG_ENABLE([install-elfh],
AS_HELP_STRING([--enable-install-elfh],[install elf.h in include dir]),
               [install_elfh=$enableval], [install_elfh=no])
AM_CONDITIONAL(INSTALL_ELFH, test "$install_elfh" = yes)

AM_CONDITIONAL(BUILD_STATIC, [dnl
test "$use_gprof" = yes -o "$use_gcov" = yes])

AC_ARG_ENABLE([tests-rpath],
AS_HELP_STRING([--enable-tests-rpath],[build $ORIGIN-using rpath into tests]),
	       [tests_use_rpath=$enableval], [tests_use_rpath=no])
AM_CONDITIONAL(TESTS_RPATH, test "$tests_use_rpath" = yes)

dnl zlib is mandatory.
save_LIBS="$LIBS"
LIBS=
eu_ZIPLIB(zlib,ZLIB,z,gzdirect,gzip)
AS_IF([test "x$with_zlib" = xno], [AC_MSG_ERROR([zlib not found but is required])])
LIBS="$save_LIBS"

dnl Test for bzlib and xz/lzma/zstd, gives BZLIB/LZMALIB/ZSTD .am
dnl conditional and config.h USE_BZLIB/USE_LZMALIB/USE_ZSTD #define.
save_LIBS="$LIBS"
LIBS=
eu_ZIPLIB(bzlib,BZLIB,bz2,BZ2_bzdopen,bzip2)
# We need this since bzip2 doesn't have a pkgconfig file.
BZ2_LIB="$LIBS"
AC_SUBST([BZ2_LIB])
eu_ZIPLIB(lzma,LZMA,lzma,lzma_auto_decoder,[LZMA (xz)])
AS_IF([test "x$with_lzma" = xyes], [LIBLZMA="liblzma"], [LIBLZMA=""])
AC_SUBST([LIBLZMA])
eu_ZIPLIB(zstd,ZSTD,zstd,ZSTD_decompress,[ZSTD (zst)])
AS_IF([test "x$with_zstd" = xyes], [LIBZSTD="libzstd"], [LIBLZSTD=""])
AC_SUBST([LIBZSTD])
zstd_LIBS="$LIBS"
AC_SUBST([zstd_LIBS])
zip_LIBS="$LIBS"
LIBS="$save_LIBS"
AC_SUBST([zip_LIBS])

dnl zstd compression support requires libzstd 1.4.0+
AS_IF([test "x$with_zstd" = xyes], [
      PKG_PROG_PKG_CONFIG
      PKG_CHECK_MODULES([ZSTD_COMPRESS],[libzstd >= 1.4.0],
                        [with_zstd_compress="yes"],[with_zstd_compress="no"])],
      [with_zstd_compress="no"])
AM_CONDITIONAL(USE_ZSTD_COMPRESS, test "x$with_zstd_compress" = "xyes")
AS_IF([test "x$with_zstd_compress" = "xyes"],
      [AC_DEFINE([USE_ZSTD_COMPRESS], [1], [zstd compression support])])

AC_CHECK_DECLS([memrchr, rawmemchr],[],[],
               [#define _GNU_SOURCE
                #include <string.h>])
AC_CHECK_DECLS([powerof2],[],[],[#include <sys/param.h>])
AC_CHECK_DECLS([mempcpy],[],[],
               [#define _GNU_SOURCE
                #include <string.h>])
AC_CHECK_DECLS([reallocarray],[],[],
               [#define _GNU_SOURCE
                #include <stdlib.h>])

AC_CHECK_FUNCS([process_vm_readv mremap])

AS_IF([test "x$ac_cv_func_mremap" = "xno"],
      [AC_MSG_WARN([elf_update needs mremap to support ELF_C_RDWR_MMAP])])

AC_CHECK_HEADERS([error.h])
AC_CHECK_HEADERS([err.h])

dnl for debuginfod concurrency heuristics
AC_CHECK_HEADERS([sched.h])
AC_CHECK_FUNCS([sched_getaffinity])
AC_CHECK_HEADERS([sys/resource.h])
AC_CHECK_FUNCS([getrlimit])

old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -D_GNU_SOURCE"
AC_FUNC_STRERROR_R()
CFLAGS="$old_CFLAGS"

AC_ARG_ENABLE([demangler],
AS_HELP_STRING([--disable-demangler],
	       [Disable libstdc++ demangle support]),
	       [], [enable_demangler=yes])
AS_IF([test "x$enable_demangler" = xyes],
AC_CHECK_LIB([stdc++], [__cxa_demangle], [dnl
AC_DEFINE([USE_DEMANGLE], [1], [Defined if demangling is enabled])])
AM_CONDITIONAL(DEMANGLE, test "x$ac_cv_lib_stdcpp___cxa_demangle" = "xyes")
AS_IF([test "x$ac_cv_lib_stdcpp___cxa_demangle" = "xyes"],
      [enable_demangler=yes],
      [AC_MSG_ERROR([[__cxa_demangle not found in libstdc++, use --disable-demangler to disable demangler support.]])]),
AM_CONDITIONAL(DEMANGLE, false))

AC_ARG_ENABLE([textrelcheck],
AS_HELP_STRING([--disable-textrelcheck],
               [Disable textrelcheck being a fatal error]))
AM_CONDITIONAL(FATAL_TEXTREL, [test "x$enable_textrelcheck" != "xno"])
AS_IF([test "x$enable_textrelcheck" != "xno"],
      [enable_textrelcheck=yes],[enable_textrelcheck=no])

AC_ARG_ENABLE([symbol-versioning],
AS_HELP_STRING([--disable-symbol-versioning],
               [Disable symbol versioning in shared objects]))

AC_CACHE_CHECK([whether symbol versioning is supported], ac_cv_symbol_versioning, [dnl
AC_COMPILE_IFELSE([AC_LANG_SOURCE([dnl
#define NEW_VERSION(name, version) \
  asm (".symver " #name "," #name "@@@" #version);
int foo(int x) { return x + 1; }
NEW_VERSION (foo, ELFUTILS_12.12)
])], ac_cv_symbol_versioning=yes, ac_cv_symbol_versioning=no)])
if test "$ac_cv_symbol_versioning" = "no"; then
    if test "x$enable_symbol_versioning" != "xno"; then
        AC_MSG_ERROR([Symbol versioning is not supported.
                      Use --disable-symbol-versioning to build without.])
    fi
fi

AM_CONDITIONAL(SYMBOL_VERSIONING, [test "x$enable_symbol_versioning" != "xno"])
AS_IF([test "x$enable_symbol_versioning" = "xno"],
      [AC_MSG_WARN([Disabling symbol versioning breaks ABI compatibility.])
       enable_symbol_versioning=no],[enable_symbol_versioning=yes])

AC_CACHE_CHECK([whether gcc accepts -Wstack-usage], ac_cv_stack_usage, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wstack-usage=262144 -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_stack_usage=yes, ac_cv_stack_usage=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(ADD_STACK_USAGE_WARNING, [test "x$ac_cv_stack_usage" != "xno"])

# -Wlogical-op was too fragile in the past, make sure we get a sane one.
AC_CACHE_CHECK([whether gcc has a sane -Wlogical-op], ac_cv_logical_op, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wlogical-op -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE(
	[#define FLAG 1
	int f (int r, int f) { return (r && (FLAG || (FLAG & f))); }])],
		  ac_cv_logical_op=yes, ac_cv_logical_op=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(SANE_LOGICAL_OP_WARNING,
	       [test "x$ac_cv_logical_op" != "xno"])

# -Wduplicated-cond was added by GCC6
AC_CACHE_CHECK([whether gcc accepts -Wduplicated-cond], ac_cv_duplicated_cond, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wduplicated-cond -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_duplicated_cond=yes, ac_cv_duplicated_cond=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(HAVE_DUPLICATED_COND_WARNING,
	       [test "x$ac_cv_duplicated_cond" != "xno"])

# -Wnull-dereference was added by GCC6
AC_CACHE_CHECK([whether gcc accepts -Wnull-dereference], ac_cv_null_dereference, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wnull-dereference -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_null_dereference=yes, ac_cv_null_dereference=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(HAVE_NULL_DEREFERENCE_WARNING,
	       [test "x$ac_cv_null_dereference" != "xno"])

# -Wimplicit-fallthrough was added by GCC7
AC_CACHE_CHECK([whether gcc accepts -Wimplicit-fallthrough], ac_cv_implicit_fallthrough, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wimplicit-fallthrough -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_implicit_fallthrough=yes, ac_cv_implicit_fallthrough=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(HAVE_IMPLICIT_FALLTHROUGH_WARNING,
	       [test "x$ac_cv_implicit_fallthrough" != "xno"])

# Check whether the compiler additionally accepts -Wimplicit-fallthrough=5
# GCC accepts this and 5 means "don't parse any fallthrough comments and
# only accept the fallthrough attribute"
AC_CACHE_CHECK([whether the compiler accepts -Wimplicit-fallthrough=5], ac_cv_implicit_fallthrough_5, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wimplicit-fallthrough=5 -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_implicit_fallthrough_5=yes, ac_cv_implicit_fallthrough_5=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(HAVE_IMPLICIT_FALLTHROUGH_5_WARNING,
	       [test "x$ac_cv_implicit_fallthrough_5" != "xno"])

# Assume the fallthrough attribute is supported if -Wimplict-fallthrough is supported
if test "$ac_cv_implicit_fallthrough" = "yes"; then
	AC_DEFINE([HAVE_FALLTHROUGH], [1],
		  [Defined if __attribute__((fallthrough)) is supported])
fi

AC_CACHE_CHECK([whether the compiler accepts -Wtrampolines], ac_cv_trampolines, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wtrampolines -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_trampolines=yes, ac_cv_trampolines=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(HAVE_TRAMPOLINES_WARNING,
	       [test "x$ac_cv_trampolines" != "xno"])

AC_CACHE_CHECK([whether the compiler accepts -Wno-packed-not-aligned], ac_cv_no_packed_not_aligned, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-packed-not-aligned -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_no_packed_not_aligned=yes, ac_cv_no_packed_not_aligned=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(HAVE_NO_PACKED_NOT_ALIGNED_WARNING,
	       [test "x$ac_cv_no_packed_not_aligned" != "xno"])

AC_CACHE_CHECK([whether the compiler accepts -Wuse-after-free=3], ac_cv_use_after_free3, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wuse-after-free=3 -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
		  ac_cv_use_after_free3=yes, ac_cv_use_after_free3=no)
CFLAGS="$old_CFLAGS"])
AM_CONDITIONAL(HAVE_USE_AFTER_FREE3_WARNING,
	       [test "x$ac_cv_use_after_free3" != "xno"])

AC_CACHE_CHECK([whether the compiler accepts -fno-addrsig], ac_cv_fno_addrsig, [dnl
old_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fno-addrsig -Werror"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([])],
                  ac_cv_fno_addrsig=yes, ac_cv_fno_addrsig=no)
CFLAGS="$old_CFLAGS"])
AS_IF([test "x$ac_cv_fno_addrsig" = "xyes"], CFLAGS="$CFLAGS -fno-addrsig")

saved_LIBS="$LIBS"
AC_SEARCH_LIBS([argp_parse], [argp])
LIBS="$saved_LIBS"
case "$ac_cv_search_argp_parse" in
        no) AC_MSG_FAILURE([failed to find argp_parse]) ;;
        -l*) argp_LDADD="$ac_cv_search_argp_parse" ;;
        *) argp_LDADD= ;;
esac
AC_SUBST([argp_LDADD])

saved_LIBS="$LIBS"
AC_SEARCH_LIBS([fts_close], [fts])
LIBS="$saved_LIBS"
case "$ac_cv_search_fts_close" in
        no) AC_MSG_FAILURE([failed to find fts_close]) ;;
        -l*) fts_LIBS="$ac_cv_search_fts_close" ;;
        *) fts_LIBS= ;;
esac
AC_SUBST([fts_LIBS])

saved_LIBS="$LIBS"
AC_SEARCH_LIBS([_obstack_free], [obstack])
LIBS="$saved_LIBS"
case "$ac_cv_search__obstack_free" in
        no) AC_MSG_FAILURE([failed to find _obstack_free]) ;;
        -l*) obstack_LIBS="$ac_cv_search__obstack_free" ;;
        *) obstack_LIBS= ;;
esac
AC_SUBST([obstack_LIBS])

dnl The directories with content.

dnl Documentation.
AC_CONFIG_FILES([doc/Makefile])

dnl Support library.
AC_CONFIG_FILES([lib/Makefile])

dnl ELF library.
AC_CONFIG_FILES([libelf/Makefile])

dnl Higher-level ELF support library.
AC_CONFIG_FILES([libebl/Makefile])

dnl DWARF-ELF Lower-level Functions support library.
AC_CONFIG_FILES([libdwelf/Makefile])

dnl DWARF library.
AC_CONFIG_FILES([libdw/Makefile])

dnl Higher-level DWARF support library.
AC_CONFIG_FILES([libdwfl/Makefile])

dnl CPU handling library.
AC_CONFIG_FILES([libcpu/Makefile])

dnl Assembler library.
AC_CONFIG_FILES([libasm/Makefile])

dnl CPU-specific backend libraries.
AC_CONFIG_FILES([backends/Makefile])

dnl Tools.
AC_CONFIG_FILES([src/Makefile po/Makefile.in])

dnl Test suite.
AC_CONFIG_FILES([tests/Makefile])

dnl pkgconfig files
AC_CONFIG_FILES([config/libelf.pc config/libdw.pc config/libdebuginfod.pc])

dnl As long as "git grep 'PRI[diouxX]' po" reports matches in
dnl translatable strings, we must use need-formatstring-macros here.
AM_GNU_GETTEXT([external], [need-formatstring-macros])

dnl AM_GNU_GETTEXT_VERSION is still needed for old versions
dnl of autoreconf that do not recognize AM_GNU_GETTEXT_REQUIRE_VERSION.
dnl 0.19.6 is the first version of gettext that provides
dnl AM_GNU_GETTEXT_REQUIRE_VERSION support.
AM_GNU_GETTEXT_VERSION([0.19.6])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.6])

dnl Appended to the config.h file.
dnl We hide all kinds of configuration magic in lib/eu-config.h.
AH_BOTTOM([#include <eu-config.h>])

dnl Version compatibility header.
AC_CONFIG_FILES([version.h:config/version.h.in])
AC_SUBST([eu_version])

# 1.234<whatever> -> 1234<whatever>
case "$PACKAGE_VERSION" in
[[0-9]].*) eu_version=`echo "$PACKAGE_VERSION" | sed 's@\.@@'` ;;
*)     	   AC_MSG_ERROR([confused by version number '$PACKAGE_VERSION']) ;;
esac
case "$eu_version" in
*.*)
  # 1234.567 -> "1234", "567"
  eu_extra_version="${eu_version#*.}"
  eu_version="${eu_version%%.*}"
  case "$eu_extra_version" in
  [[0-9]][[0-9]][[0-9]]) ;;
  [[0-9]][[0-9]])	eu_extra_version="${eu_extra_version}0" ;;
  [[0-9]])	   	eu_extra_version="${eu_extra_version}00" ;;
  *) AC_MSG_ERROR([confused by version number '$PACKAGE_VERSION']) ;;
  esac
  ;;
*)
  eu_extra_version=000
  ;;
esac

case "$eu_version" in
      0[[0-9]][[0-9]][[0-9]]) eu_version="${eu_version#0}$eu_extra_version" ;;
[[0-9]][[0-9]][[0-9]][[0-9]]) eu_version="${eu_version}$eu_extra_version" ;;
[[0-9]][[0-9]][[0-9]])	      eu_version="${eu_version}0$eu_extra_version" ;;
[[0-9]][[0-9]])	  	      eu_version="${eu_version}00$eu_extra_version";;
*) AC_MSG_ERROR([confused by version number '$PACKAGE_VERSION']) ;;
esac

# Round up to the next release API (x.y) version.
eu_version=$(( (eu_version + 999) / 1000 ))

AC_CHECK_SIZEOF(long)

# On aarch64 before glibc 2.20 we would get the kernel user_pt_regs instead
# of the user_regs_struct from sys/user.h. They are structurally the same
# but we get either one or the other.
AC_CHECK_TYPE([struct user_regs_struct],
              [sys_user_has_user_regs=yes], [sys_user_has_user_regs=no],
              [[#include <sys/ptrace.h>]
               [#include <sys/time.h>]
               [#include <sys/user.h>]])
if test "$sys_user_has_user_regs" = "yes"; then
  AC_DEFINE(HAVE_SYS_USER_REGS, 1,
            [Define to 1 if <sys/user.h> defines struct user_regs_struct])
fi

# On a 64-bit host where can can use $CC -m32, we'll run two sets of tests.
utrace_BIARCH
CC_BIARCH="$CC $utrace_biarch"
AC_SUBST([CC_BIARCH])

# In maintainer mode we really need flex and bison.
# Otherwise we really need a release dir with maintainer files generated.
if test "x$enable_maintainer_mode" = xyes; then
  AC_CHECK_PROG(HAVE_FLEX, flex, yes, no)
  if test "$HAVE_FLEX" = "no"; then
    AC_MSG_ERROR([flex needed in maintainer mode])
  fi
  AC_CHECK_PROG(HAVE_BISON, bison, yes, no)
  if test "$HAVE_BISON" = "no"; then
    AC_MSG_ERROR([bison needed in maintainer mode])
  fi
  AC_CHECK_PROG(HAVE_GAWK, gawk, yes, no)
  if test "$HAVE_GAWK" = "no"; then
    AC_MSG_ERROR([gawk needed in maintainer mode])
  fi
else
  if test ! -f ${srcdir}/libdw/known-dwarf.h; then
    AC_MSG_ERROR([No libdw/known-dwarf.h. configure --enable-maintainer-mode])
  fi
fi

# The testfiles are all compressed, we need bunzip2 when running make check
AC_CHECK_PROG(HAVE_BUNZIP2, bunzip2, yes, no)
if test "$HAVE_BUNZIP2" = "no"; then
  AC_MSG_WARN([No bunzip2, needed to run make check])
fi

# For tests that need to use zstd compression
AC_CHECK_PROG(HAVE_ZSTD, zstd, yes, no)
AM_CONDITIONAL([HAVE_ZSTD],[test "x$HAVE_ZSTD" = "xyes"])

# For tests that need to use C++11
AX_CXX_COMPILE_STDCXX(11, noext, optional)
AS_IF([test "x$HAVE_CXX11" = "x1"], [HAVE_CXX11=yes], [HAVE_CXX11=no])
AM_CONDITIONAL([HAVE_CXX11],[test "x$HAVE_CXX11" = "xyes"])

# Look for libcurl for libdebuginfod minimum version as per rhel7.
AC_ARG_ENABLE([libdebuginfod],AS_HELP_STRING([--enable-libdebuginfod], [Build debuginfod client library (can be =dummy)]))
AS_IF([test "x$enable_libdebuginfod" != "xno"], [
    if test "x$enable_libdebuginfod" != "xdummy"; then
      AC_MSG_NOTICE([checking libdebuginfod dependencies, --disable-libdebuginfod or --enable-libdebuginfo=dummy to skip])
      enable_libdebuginfod=yes # presume success
      PKG_PROG_PKG_CONFIG
      PKG_CHECK_MODULES([libcurl],[libcurl >= 7.29.0],[],[enable_libdebuginfod=no])
      if test "x$enable_libdebuginfod" = "xno"; then
        AC_MSG_ERROR([dependencies not found, use --disable-libdebuginfod to disable or --enable-libdebuginfod=dummy to build a (bootstrap) dummy library.])
      fi
    else
      AC_MSG_NOTICE([building (bootstrap) dummy libdebuginfo library])
    fi
])

AC_CHECK_LIB(pthread, pthread_setname_np, [
                      AC_DEFINE([HAVE_PTHREAD_SETNAME_NP],[1],[Enable pthread_setname_np])])

AS_IF([test "x$enable_libdebuginfod" = "xyes" || test "x$enable_libdebuginfod" = "xdummy"],
      [AC_DEFINE([ENABLE_LIBDEBUGINFOD], [1], [Enable libdebuginfod])])
AS_IF([test "x$enable_libdebuginfod" = "xdummy"],
      [AC_DEFINE([DUMMY_LIBDEBUGINFOD], [1], [Build dummy libdebuginfod])])
AM_CONDITIONAL([LIBDEBUGINFOD],[test "x$enable_libdebuginfod" = "xyes" || test "x$enable_libdebuginfod" = "xdummy"])
AM_CONDITIONAL([DUMMY_LIBDEBUGINFOD],[test "x$enable_libdebuginfod" = "xdummy"])
AC_CHECK_HEADERS([execinfo.h])

# Look for libmicrohttpd, libarchive, sqlite for debuginfo server and srcfiles tool
# minimum versions as per rhel7.
AC_ARG_ENABLE([debuginfod],AS_HELP_STRING([--enable-debuginfod], [Build debuginfod server]))
AS_IF([test "x$enable_debuginfod" != "xno"], [
    if test "x$HAVE_CXX11" = "xno"; then
      AC_MSG_ERROR([the compiler does not support C++11, use --disable-debuginfod to disable.])
    fi
    AC_MSG_NOTICE([checking debuginfod dependencies, --disable-debuginfod to skip])
    if test "x$enable_libdebuginfod" = "xno"; then
      AC_MSG_ERROR([need libdebuginfod (or dummy), use --disable-debuginfod to disable.])
    fi
    enable_debuginfod=yes # presume success
    AC_DEFINE([HAVE_LIBARCHIVE], [1], [Define to 1 if libarchive is available]) # presume success
    PKG_PROG_PKG_CONFIG
    PKG_CHECK_MODULES([libmicrohttpd],[libmicrohttpd >= 0.9.33],[],[enable_debuginfod=no])
    PKG_CHECK_MODULES([oldlibmicrohttpd],[libmicrohttpd < 0.9.51],[old_libmicrohttpd=yes],[old_libmicrohttpd=no])
    PKG_CHECK_MODULES([sqlite3],[sqlite3 >= 3.7.17],[],[enable_debuginfod=no])
    PKG_CHECK_MODULES([libarchive],[libarchive >= 3.1.2],[],[enable_debuginfod=no], AC_DEFINE([HAVE_LIBARCHIVE], [0], [Define to 0 if libarchive is not available]))
    if test "x$enable_debuginfod" = "xno"; then
      AC_MSG_ERROR([dependencies not found, use --disable-debuginfod to disable.])
    fi
])

AS_IF([test "x$enable_debuginfod" != "xno"],AC_DEFINE([ENABLE_DEBUGINFOD],[1],[Build debuginfod]))
AM_CONDITIONAL([DEBUGINFOD],[test "x$enable_debuginfod" = "xyes"])
AM_CONDITIONAL([OLD_LIBMICROHTTPD],[test "x$old_libmicrohttpd" = "xyes"])

dnl for /etc/profile.d/elfutils.{csh,sh}
default_debuginfod_urls=""
AC_ARG_ENABLE(debuginfod-urls,
            [AS_HELP_STRING([--enable-debuginfod-urls@<:@=URLS@:>@],[add URLS to profile.d DEBUGINFOD_URLS])],
            [if test "x${enableval}" = "xyes";
             then default_debuginfod_urls="https://debuginfod.elfutils.org/";
             elif test "x${enableval}" != "xno"; then
             default_debuginfod_urls="${enableval}";
             fi],
            [default_debuginfod_urls=""])
AC_SUBST(DEBUGINFOD_URLS, $default_debuginfod_urls)                
AC_CONFIG_FILES([config/profile.sh config/profile.csh])

AC_OUTPUT

AC_MSG_NOTICE([
=====================================================================
        elfutils: ${PACKAGE_VERSION} (eu_version: ${eu_version})
=====================================================================

    Prefix                             : ${prefix}
    Program prefix ("eu-" recommended) : ${program_prefix}
    Source code location               : ${srcdir}
    Maintainer mode                    : ${enable_maintainer_mode}
    build arch                         : ${ac_cv_build}

    CFLAGS=${CFLAGS}
    CXXFLAGS=${CXXFLAGS}

  RECOMMENDED FEATURES (should all be yes)
    gzip support                       : ${with_zlib}
    bzip2 support                      : ${with_bzlib}
    lzma/xz support                    : ${with_lzma}
    zstd support                       : ${with_zstd}
    zstd compression support           : ${with_zstd_compress}
    libstdc++ demangle support         : ${enable_demangler}
    File textrel check                 : ${enable_textrelcheck}
    Symbol versioning                  : ${enable_symbol_versioning}

  NOT RECOMMENDED FEATURES (should all be no)
    Experimental thread safety         : ${use_locks}
    install elf.h                      : ${install_elfh}

  OTHER FEATURES
    Deterministic archives by default  : ${default_ar_deterministic}
    Native language support            : ${USE_NLS}
    Extra Valgrind annotations         : ${use_vg_annotations}
    libdebuginfod client support       : ${enable_libdebuginfod}
    Debuginfod server support          : ${enable_debuginfod}
    Default DEBUGINFOD_URLS            : ${default_debuginfod_urls}

  EXTRA TEST FEATURES (used with make check)
    have bunzip2 installed (required)  : ${HAVE_BUNZIP2}
    have zstd installed                : ${HAVE_ZSTD}
    C++11                              : ${HAVE_CXX11}
    debug branch prediction            : ${use_debugpred}
    gprof support                      : ${use_gprof}
    gcov support                       : ${use_gcov}
    run all tests under valgrind       : ${use_valgrind}
    gcc undefined behaviour sanitizer  : ${use_undefined}
    gcc address sanitizer              : ${use_address}
    clang memory sanitizer             : ${use_msan}
    use rpath in tests                 : ${tests_use_rpath}
    test biarch                        : ${utrace_cv_cc_biarch}
])

if test "$install_elfh" = yes; then
  if test "${prefix}" = "/usr/local" -o "${prefix}" = "/usr"; then
    AC_MSG_WARN([installing elf.h in ${includedir} might conflict with glibc/system elf.h])
  fi
fi
