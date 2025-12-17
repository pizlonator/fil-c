dnl Process this file with autoconf to produce a configure script.
# Copyright (C) 2000-2012, 2016, 2019, 2022 Free Software Foundation, Inc.
#
# Author: Nikos Mavrogiannopoulos, Simon Josefsson
#
# This file is part of GnuTLS.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

AC_PREREQ(2.64)

dnl when updating version also update LT_REVISION in m4/hooks.m4
AC_INIT([GnuTLS], [3.8.7], [bugs@gnutls.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4 src/gl/m4 lib/unistring/m4])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([1.12.2 foreign subdir-objects no-dist-gzip dist-xz -Wall -Wno-override])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_HEADERS([config.h])

AC_MSG_RESULT([***
*** Checking for compilation programs...
])

dnl Checks for programs.
PKG_PROG_PKG_CONFIG
AC_PROG_CC
gl_EARLY
ggl_EARLY
unistring_EARLY
AM_PROG_AS
AM_PROG_AR
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_YACC
AC_PROG_SED

AC_USE_SYSTEM_EXTENSIONS

#
# Require C99 support
#    
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" = "no"; then
  AC_MSG_WARN([[Compiler does not support C99. It may not be able to compile the project.]])
fi

AM_CONDITIONAL(CROSS_COMPILING, test "$cross_compiling" = yes)

AX_CODE_COVERAGE

AM_MAINTAINER_MODE([enable])

AM_MISSING_PROG([GPERF], [gperf])
AM_MISSING_PROG([ASN1PARSER], [asn1Parser])
AM_MISSING_PROG([PERL], [perl])

AC_CHECK_PROGS([FAKETIME], [faketime])
AS_IF([test -n "$FAKETIME"], [FAKETIME_F_OPT="-f"; AC_SUBST([FAKETIME_F_OPT])], [
  AC_CHECK_PROGS([DATEFUDGE], [datefudge])
  AS_IF([test -n "$DATEFUDGE"], [
    FAKETIME_F_OPT="-s"
    AC_SUBST([FAKETIME], [$DATEFUDGE])
    AC_SUBST([FAKETIME_F_OPT])
  ])
])

AC_MSG_CHECKING([whether faketime program works])
AC_RUN_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <stdio.h>
      #include <stdlib.h>
      #include <time.h>
    ]],
    [[time_t t = time(NULL);
      struct tm *tmp = localtime(&t);
      char outstr[200];
      if (!tmp) return 0;
      if (!strftime(outstr, sizeof(outstr), "%s", tmp)) return 0;
      puts(outstr);
      return 0;
    ]])],
  [AS_IF([test -n "$FAKETIME" && test `TZ=UTC $FAKETIME $FAKETIME_F_OPT "2006-09-23 00:00:00" ./conftest$EXEEXT` = "1158969600"], [
     ac_cv_faketime_works=yes
   ], [
     ac_cv_faketime_works=no
   ])],
  [ac_cv_faketime_works=no],
  [ac_cv_faketime_works=cross-compiling])

AC_MSG_RESULT([$ac_cv_faketime_works])
AC_SUBST([ac_cv_faketime_works])

AC_ARG_ENABLE(bash-tests,
  AS_HELP_STRING([--disable-bash-tests], [skip some tests that badly need bash]),
    enable_bash_tests=$enableval, enable_bash_tests=yes)
AM_CONDITIONAL(DISABLE_BASH_TESTS, test "$enable_bash_tests" != "yes")

AC_ARG_ENABLE(doc,
  AS_HELP_STRING([--disable-doc], [don't generate any documentation]),
    enable_doc=$enableval, enable_doc=yes)
AM_CONDITIONAL(ENABLE_DOC, test "$enable_doc" != "no")

AC_ARG_ENABLE(manpages,
  AS_HELP_STRING([--enable-manpages], [install manpages even if disable-doc is given]),
    enable_manpages=$enableval,enable_manpages=auto)

if test "${enable_manpages}" = "auto";then
  enable_manpages="${enable_doc}"
fi

AM_CONDITIONAL(ENABLE_MANPAGES, test "$enable_manpages" != "no")

AC_ARG_ENABLE(tools,
  AS_HELP_STRING([--disable-tools], [don't compile any tools]),
    enable_tools=$enableval, enable_tools=yes)
AM_CONDITIONAL(ENABLE_TOOLS, test "$enable_tools" != "no")

# For includes/gnutls/gnutls.h.in.
AC_SUBST(MAJOR_VERSION, `echo $PACKAGE_VERSION | sed 's/\(.*\)\..*\..*/\1/g'`)
AC_SUBST(MINOR_VERSION, `echo $PACKAGE_VERSION | sed 's/.*\.\(.*\)\..*/\1/g'`)
AC_SUBST(PATCH_VERSION, [[`echo $PACKAGE_VERSION | sed 's/.*\..*\.\([0-9]*\).*/\1/g'`]])
AC_SUBST(NUMBER_VERSION, `printf "0x%02x%02x%02x" $MAJOR_VERSION $MINOR_VERSION $PATCH_VERSION`)

dnl C and C++ capabilities
AC_C_INLINE
AC_HEADER_STDC

# For the C++ code
AC_ARG_ENABLE(cxx,
  AS_HELP_STRING([--disable-cxx], [unconditionally disable the C++ library]),
    use_cxx=$enableval, use_cxx=yes)
if test "$use_cxx" != "no"; then
  AC_LANG_PUSH(C++)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])], use_cxx=yes, use_cxx=no)
  AC_LANG_POP(C++)
fi
AM_CONDITIONAL(ENABLE_CXX, test "$use_cxx" != "no")

dnl Detect windows build
use_accel=yes
have_vista_dynamic=yes
case "$host" in
  *android*)
    have_android=yes
    have_elf=yes
  ;;
  *mingw32* | *mingw64*)
    have_win=yes
    AC_DEFINE([_UNICODE], [1], [Defined to 1 for Unicode (wide chars) APIs])
    GNUTLS_LIBS_PRIVATE="${GNUTLS_LIBS_PRIVATE} -ladvapi32 -lcrypt32"
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
      [[#include <windows.h>
       #if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0600
       # error Vista APIs allowed statically
       #endif
      ]],[[;]])],[have_vista_dynamic=yes],[have_vista_dynamic=no])
    AC_ARG_ENABLE(dyn_ncrypt,
      AS_HELP_STRING([--enable-dyn-ncrypt], [use ncrypt dynamically]),
      enable_dyn_ncrypt=$enableval, enable_dyn_ncrypt=$have_vista_dynamic
    )
    if test "x$enable_dyn_ncrypt" = "xyes"; then
      AC_DEFINE([DYN_NCRYPT], 1, [Dynamic use of ncrypt API (win32)])
    else
      GNUTLS_LIBS_PRIVATE="${GNUTLS_LIBS_PRIVATE} -lncrypt"
    fi
  ;;
  *darwin*)
    have_macosx=yes
    save_LDFLAGS="$LDFLAGS"
    dnl Try to use -no_weak_imports if available. This makes sure we
    dnl error out when linking to a function that doesn't exist in the
    dnl intended minimum runtime version.
    LDFLAGS="$LDFLAGS -Wl,-no_weak_imports"
    AC_MSG_CHECKING([whether the linker supports -Wl,-no_weak_imports])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
      [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no); LDFLAGS="$save_LDFLAGS"])
    GNUTLS_LIBS_PRIVATE="${GNUTLS_LIBS_PRIVATE} -framework Security -framework CoreFoundation"
  ;;
  *solaris*)
    have_elf=yes
    use_accel=no
    AC_MSG_WARN([[
***
*** In solaris hardware acceleration is disabled by default due to issues
*** with the assembler. Use --enable-hardware-acceleration to enable it.
*** ]])
  ;;
  *)
    have_elf=yes
  ;;
esac

AM_CONDITIONAL(ANDROID, test "$have_android" = yes)
AM_CONDITIONAL(WINDOWS, test "$have_win" = yes)
AM_CONDITIONAL(MACOSX, test "$have_macosx" = yes)
AM_CONDITIONAL(ELF, test "$have_elf" = yes)

dnl Hardware Acceleration
AC_ARG_ENABLE(hardware-acceleration,
  AS_HELP_STRING([--disable-hardware-acceleration], [unconditionally disable hardware acceleration]),
    use_accel=$enableval)
hw_accel=none


use_padlock=no
if test "$use_accel" != "no"; then
case $host_cpu in
  armv8 | aarch64)
      hw_accel="aarch64"
      case $host_os in
        *_ilp32)
	  dnl ILP32 not supported in assembler yet
	  hw_accel="none"
        ;;
      esac
  ;;
  i?86 | x86_64 | amd64)
      AC_CHECK_HEADERS(cpuid.h)
      if test "$host_cpu" = "x86_64" || test "$host_cpu" = "amd64"; then
        hw_accel="x86-64"
      else
        hw_accel="x86"
      fi
      use_padlock=yes
  ;;
  *)
  ;;
esac

# Check if the assembler supports -march=all
if test "$hw_accel" = aarch64; then
  AARCH64_CCASFLAGS="-Wa,-march=all"
  AC_MSG_CHECKING([whether the compiler supports -Wa,-march=all])
  : > conftest.s
  if "$CCAS" "$AARCH64_CCASFLAGS" -c conftest.s >/dev/null 2>&1; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
    AARCH64_CCASFLAGS=
  fi
  AC_SUBST(AARCH64_CCASFLAGS)
fi

# check for gcc's __get_cpuid_count functionality
AC_MSG_CHECKING([for __get_cpuid_count])
AC_LINK_IFELSE(
   [AC_LANG_SOURCE([
    #include <cpuid.h>
    int main(void) { unsigned t1; return __get_cpuid_count(7, 0, &t1, &t1, &t1, &t1); }
   ])],
   [AC_DEFINE([HAVE_GET_CPUID_COUNT], [1], [use __get_cpuid_count]) AC_MSG_RESULT([yes])],
   [AC_MSG_RESULT([no])]
)

fi

AC_ARG_ENABLE(tls13-interop,
  AS_HELP_STRING([--disable-tls13-interop], [disable TLS1.3 interoperability testing with openssl]),
    enable_tls13_interop=$enableval, enable_tls13_interop=yes)

dnl Check for iovec type
AC_CHECK_MEMBERS([struct iovec.iov_base],
      [
        AC_SUBST([DEFINE_IOVEC_T], ["#include <sys/uio.h>
typedef struct iovec giovec_t;"])
      ],
      [
        AC_SUBST([DEFINE_IOVEC_T], ["typedef struct {
    void *iov_base;
    size_t iov_len;
} giovec_t;"])
      ],
      [#include <sys/uio.h>
      ])
AM_SUBST_NOTMAKE([DEFINE_IOVEC_T])

dnl Need netinet/tcp.h for TCP_FASTOPEN
AC_CHECK_HEADERS([netinet/tcp.h])
AC_CHECK_HEADERS([stdatomic.h])

dnl This ensures that we link with the right library for atomic operations on Linux SPARC
save_LIBS=$LIBS
AC_SEARCH_LIBS([__atomic_load_4], [atomic], [], [AC_MSG_NOTICE([Could not detect libatomic])])
LIBS=$save_LIBS

AS_IF([test "$ac_cv_search___atomic_load_4" = "none required" || test "$ac_cv_search___atomic_load_4" = "no"],
      [AC_SUBST([LIBATOMIC_LIBS], [])],
      [AC_SUBST([LIBATOMIC_LIBS], [$ac_cv_search___atomic_load_4])])

dnl We use its presence to detect C11 threads
AC_CHECK_HEADERS([threads.h])

AC_CHECK_HEADERS([sanitizer/asan_interface.h])
AC_CHECK_HEADERS([valgrind/memcheck.h])

AC_ARG_ENABLE(padlock,
  AS_HELP_STRING([--disable-padlock], [unconditionally disable padlock acceleration]),
    use_padlock=$enableval)

if test "$use_padlock" != "no"; then
    AC_DEFINE([ENABLE_PADLOCK], 1, [Enable padlock acceleration])
    AC_SUBST([ENABLE_PADLOCK])
fi
AM_CONDITIONAL(ENABLE_PADLOCK, test "$use_padlock" = "yes")
AM_CONDITIONAL(ASM_AARCH64, test x"$hw_accel" = x"aarch64")
AM_CONDITIONAL(ASM_X86_64, test x"$hw_accel" = x"x86-64")
AM_CONDITIONAL(ASM_X86_32, test x"$hw_accel" = x"x86")
AM_CONDITIONAL(ASM_X86, test x"$hw_accel" = x"x86" || test x"$hw_accel" = x"x86-64")
AM_CONDITIONAL(HAVE_GCC_GNU89_INLINE_OPTION, test "$gnu89_inline" = "yes"])
AM_CONDITIONAL(HAVE_GCC, test "$GCC" = "yes")

dnl check for getrandom()
rnd_variant="auto-detect"
AC_MSG_CHECKING([for getrandom])
AC_LINK_IFELSE([AC_LANG_PROGRAM([
	   #include <sys/random.h>],[
		   getrandom(0, 0, 0);
		  ])],
		  [AC_MSG_RESULT(yes)
		   AC_DEFINE([HAVE_GETRANDOM], 1, [Enable the Linux getrandom function])
		   rnd_variant=getrandom],
		  [AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for KERN_ARND])
AC_LINK_IFELSE([AC_LANG_PROGRAM([
	   #include <sys/sysctl.h>
           #ifdef __linux__
           #error 1
           #endif
           static int name[] = {CTL_KERN, KERN_ARND};
	   ],[
		   sysctl(0, 0, 0, 0, 0, 0);
		  ])],
		  [AC_MSG_RESULT(yes)
		   AC_DEFINE([HAVE_KERN_ARND], 1, [Enable the BSD sysctl(KERN_ARND) function])
		   rnd_variant=kern_arnd],
		  [AC_MSG_RESULT(no)])

AM_CONDITIONAL(HAVE_KERN_ARND, test "$rnd_variant" = "kern_arnd")

AC_MSG_CHECKING([for getentropy])
AC_LINK_IFELSE([AC_LANG_PROGRAM([
	   #include <unistd.h>
           #ifdef __APPLE__
           #include <sys/random.h>
           #endif
           #ifdef __linux__
           #error 1
           #endif
	   ],[
		   getentropy(0, 0);
		  ])],
		  [AC_MSG_RESULT(yes)
		   AC_DEFINE([HAVE_GETENTROPY], 1, [Enable the OpenBSD getentropy function])
		   rnd_variant=getentropy],
		  [AC_MSG_RESULT(no)])

AM_CONDITIONAL(HAVE_GETENTROPY, test "$rnd_variant" = "getentropy")

dnl Try the hooks.m4
LIBGNUTLS_HOOKS
LIBGNUTLS_EXTRA_HOOKS

AC_ARG_ENABLE(tests,
  AS_HELP_STRING([--disable-tests], [don't compile or run any tests]),
    enable_tests=$enableval, enable_tests=$enable_tools)
AM_CONDITIONAL(ENABLE_TESTS, test "$enable_tests" != "no")

AC_ARG_ENABLE(fuzzer-target,
  AS_HELP_STRING([--enable-fuzzer-target], [make a library intended for testing - not production]),
    enable_fuzzer_target=$enableval, enable_fuzzer_target=no)
if test "$enable_fuzzer_target" != "no";then
	AC_DEFINE([FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION], 1, [Enable fuzzer target -not for production])
fi

dnl
dnl check for gtk-doc
dnl
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19])
m4_ifdef([AM_GNU_GETTEXT_REQUIRE_VERSION],[
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19])
])

AC_C_BIGENDIAN

dnl No fork on MinGW, disable some self-tests until we fix them.
dnl Check clock_gettime and pthread_mutex_lock in libc (avoid linking to other libs)
AC_CHECK_FUNCS([fork setitimer getrusage getpwuid_r nanosleep daemon getpid localtime mmap],,)
dnl Manually check some functions by including headers first. On macOS, you
dnl normally only have the latest SDK available, containing all existing
dnl functions, but having them restricted according to target version in
dnl headers. If we bypass the headers and just try linking (as AC_CHECK_FUNCS
dnl does), we will accidentally detect functions which we shouldn't use. Set
dnl ac_cv_func_* as well, to avoid later AC_CHECK_FUNCS from other included
dnl scripts from overriding it.
AC_MSG_CHECKING([for clock_gettime])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <time.h>], [clock_gettime(0, 0);])],
  [AC_MSG_RESULT(yes); ac_cv_func_clock_gettime=yes
  AC_DEFINE([HAVE_CLOCK_GETTIME], 1, [Define to 1 if you have the `clock_gettime' function.])],
  [AC_MSG_RESULT(no); ac_cv_func_clock_gettime=no])
AC_MSG_CHECKING([for fmemopen])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdio.h>], [fmemopen(0, 0, 0);])],
  [AC_MSG_RESULT(yes); ac_cv_func_fmemopen=yes
  AC_DEFINE([HAVE_FMEMOPEN], 1, [Define to 1 if you have the `fmemopen' function.])],
  [AC_MSG_RESULT(no); ac_cv_func_fmemopen=no])

AM_CONDITIONAL(HAVE_FORK, test "$ac_cv_func_fork" != "no")

AC_CHECK_FUNCS([__register_atfork secure_getenv getauxval],,)

AC_ARG_ENABLE(seccomp-tests,
  AS_HELP_STRING([--enable-seccomp-tests], [unconditionally enable tests with seccomp]),
    seccomp_tests=$enableval, seccomp_tests=no)

AM_CONDITIONAL(HAVE_SECCOMP_TESTS, test "$seccomp_tests" = "yes")

# check for libseccomp - used in test programs
AC_LIB_HAVE_LINKFLAGS(seccomp,, [#include <seccomp.h>  
], [seccomp_init(0);])

# check for libcrypto - used in test programs
AC_LIB_HAVE_LINKFLAGS(crypto,, [#include <openssl/evp.h>  
], [EVP_CIPHER_CTX_init(NULL);])

AM_CONDITIONAL(HAVE_LIBCRYPTO, test "$HAVE_LIBCRYPTO" = "yes")

AC_LIB_HAVE_LINKFLAGS(rt,, [#include <time.h>
#include <signal.h>
], [timer_create (0,0,0);])

if test "$have_win" != "yes";then
  AC_CHECK_FUNCS([pthread_mutex_lock],,)
  if test "$ac_cv_func_pthread_mutex_lock" != "yes";then
    AC_LIB_HAVE_LINKFLAGS(pthread,, [#include <pthread.h>], [pthread_mutex_lock (0);])
  fi
fi

if test "$ac_cv_func_nanosleep" != "yes";then
  AC_LIB_HAVE_LINKFLAGS(rt,, [#include <time.h>], [nanosleep (0, 0);])
  gnutls_needs_librt=yes
fi

if test "$ac_cv_func_clock_gettime" != "yes";then
  AC_LIB_HAVE_LINKFLAGS(rt,, [#include <time.h>], [clock_gettime (0, 0);])
  gnutls_needs_librt=yes
fi

if test "$have_macosx" = "yes";then
  dnl Remove -no_weak_imports from LDFLAGS after testing for function
  dnl availability; keeping it included later breaks code that uses
  dnl FD_SET, which since Xcode 11.4 implicitly adds a call to the
  dnl weakly linked function __darwin_check_fd_set_overflow. We only
  dnl need it above to make sure that we don't detect functions that
  dnl are linked weakly (and can end up null at runtime) unless
  dnl we check for their availability at runtime.
  new_LDFLAGS=""
  for f in $LDFLAGS; do
    if test "$f" != "-Wl,-no_weak_imports"; then
      new_LDFLAGS="$new_LDFLAGS $f"
    fi
  done
  LDFLAGS="$new_LDFLAGS"
fi

AC_ARG_WITH(included-unistring, AS_HELP_STRING([--with-included-unistring],
                                 [disable linking with system libunistring]),
            included_unistring="$withval", 
            included_unistring=no)

if test "$included_unistring" = yes;then
  ac_have_unistring=no
else
  save_LIBS=$LIBS
  AC_SEARCH_LIBS(u8_normalize, unistring, [
    included_unistring=no
    ac_have_unistring=yes
    AC_SUBST([LIBUNISTRING], [$ac_cv_search_u8_normalize])
  ], [
    ac_cv_libunistring=no
    AC_MSG_ERROR([[
  ***
  *** Libunistring was not found. To use the included one, use --with-included-unistring
    ]])
  ])
  LIBS=$save_LIBS
fi

AM_CONDITIONAL(HAVE_LIBUNISTRING, test "$ac_have_unistring" = "yes")

# specify our own suppressions file; this needs to be set before gl_INIT
DEFAULT_VALGRINDFLAGS='-q --error-exitcode=1 --leak-check=full --track-origins=yes --gen-suppressions=all'

gl_VALGRIND_TESTS_DEFAULT_NO

dnl Note that g*l_INIT are run after we check for library capabilities,
dnl to prevent issues from caching lib dependencies. See discussion
dnl in https://bugs.gentoo.org/show_bug.cgi?id=494940 and
dnl https://gnu-autoconf.7623.n7.nabble.com/Correct-way-to-check-for-clock-gettime-td12276.html
gl_INIT
ggl_INIT
unistring_INIT

# disable the extended test suite at tests/suite if asked, or if we are not running in git master
AC_ARG_ENABLE(full-test-suite,
  AS_HELP_STRING([--disable-full-test-suite], [disable running very slow components of test suite]),
    full_test_suite=$enableval, full_test_suite=yes)

# test if we are in git master or in release build. In release
# builds we do not use valgrind.
SUITE_FILE="${srcdir}/tests/suite/prime-check.c"
if test "$full_test_suite" = yes && test ! -f "$SUITE_FILE";then
	full_test_suite=no
fi

AM_CONDITIONAL(WANT_TEST_SUITE, test "$full_test_suite" = "yes")

# parts of the extended test suite use Python
AM_PATH_PYTHON([3.6],, [:])

AC_ARG_ENABLE(oldgnutls-interop,
  AS_HELP_STRING([--enable-oldgnutls-interop], [enable interoperability testing with old gnutls version]),
    enable_oldgnutls_interop=$enableval, enable_oldgnutls_interop=no)

if test "$enable_oldgnutls_interop" != "no" && test "$full_test_suite" != yes;then
	AC_MSG_ERROR([cannot --enable-oldgnutls-interop without --enable-full-test-suite])
fi

AM_CONDITIONAL(ENABLE_OLDGNUTLS_INTEROP, test "$enable_oldgnutls_interop" != "no")

# check for libev - used in full test suite
AC_LIB_HAVE_LINKFLAGS(ev,, [#include <ev.h>], [ev_run(0,0);])
if test "$ac_cv_libev" = yes; then
        AC_SUBST([LIBEV_LIBS], [$LIBEV])
elif test "$full_test_suite" = yes;then
        AC_MSG_ERROR([[***
*** libev4 was not found.
***]])
fi

dnl GCC warnings to enable

AC_ARG_ENABLE([gcc-warnings],
  [AS_HELP_STRING([--disable-gcc-warnings],
		  [turn off lots of GCC warnings (for developers)])],
  [case $enableval in
     yes|no) ;;
     *)      AC_MSG_ERROR([bad value $enableval for gcc-warnings option]) ;;
   esac
   gl_gcc_warnings=$enableval],
  [gl_gcc_warnings=yes]
)

if test "$gl_gcc_warnings" = yes; then
  gl_WARN_ADD([-Wtype-limits], [WSTACK_CFLAGS])

  nw="$nw -Wsystem-headers"         # Don't let system headers trigger warnings
  nw="$nw -Wc++-compat"             # We don't care about C++ compilers
  nw="$nw -Wundef"                  # Warns on '#if GNULIB_FOO' etc in gnulib
  nw="$nw -Wtraditional"            # Warns on #elif which we use often
  nw="$nw -Wpadded"                 # Our structs are not padded
  nw="$nw -Wtraditional-conversion" # Too many warnings for now
  nw="$nw -Wswitch-default"         # Too many warnings for now
  nw="$nw -Wformat-y2k"             # Too many warnings for now
  nw="$nw -Woverlength-strings"     # We use some in tests/
  nw="$nw -Wvla"                    # There is no point to avoid C99 variable length arrays
  nw="$nw -Wformat-nonliteral"      # Incompatible with gettext _()
  nw="$nw -Wformat-signedness"      # Too many to handle
  nw="$nw -Wstrict-overflow"
  nw="$nw -Wmissing-noreturn"
  nw="$nw -Winline"                 # Too compiler dependent
  nw="$nw -Wsuggest-attribute=pure" # Is it worth using attributes?
  nw="$nw -Wsuggest-attribute=const" # Is it worth using attributes?
  nw="$nw -Wsuggest-attribute=noreturn" # Is it worth using attributes?
  nw="$nw -Wstack-protector"        # Some functions cannot be protected
  nw="$nw -Wunsafe-loop-optimizations" # Warnings with no point
  nw="$nw -Wredundant-decls"        # Some files cannot be compiled with that (gl_fd_to_handle)
  nw="$nw -Wtype-limits"            # Too many warnings in gnulib macros
  nw="$nw -Warith-conversion"       # Too compiler dependent
  nw="$nw -Wmissing-variable-declarations" # asn1Parser provides no way to embed declarations in generated code

  gl_MANYWARN_ALL_GCC([ws])
  gl_MANYWARN_COMPLEMENT(ws, [$ws], [$nw])
  for w in $ws; do
    gl_WARN_ADD([$w])
  done
  gl_WARN_ADD([-Wno-missing-field-initializers]) # We need this one
  gl_WARN_ADD([-Wno-unused-parameter]) # Too many warnings for now
  gl_WARN_ADD([-Wno-format-truncation]) # Many warnings with no point
  gl_WARN_ADD([-Wimplicit-fallthrough=2])
  gl_WARN_ADD([-Wabi=11])
  gl_WARN_ADD([-fdiagnostics-show-option])
fi

if test "$gl_cv_opt_valgrind_tests" != no; then
  gl_WARN_ADD([-fno-builtin-strcmp])
fi

AC_SUBST([WERROR_CFLAGS])
AC_SUBST([WSTACK_CFLAGS])
AC_SUBST([WARN_CFLAGS])

dnl Programs for compilation or development
AC_PROG_LN_S
LT_INIT([disable-static,win32-dll,shared])



AC_LIB_HAVE_LINKFLAGS(dl,, [#include <dlfcn.h>], [dladdr (0, 0);])

AC_ARG_ENABLE(fips140-mode,
  AS_HELP_STRING([--enable-fips140-mode], [enable FIPS140-2 mode]),
    enable_fips=$enableval, enable_fips=no)
AM_CONDITIONAL(ENABLE_FIPS140, test "$enable_fips" = "yes")
if [ test "$enable_fips" = "yes" ];then
  if test "x$HAVE_LIBDL" = "xyes";then
    AC_DEFINE([ENABLE_FIPS140], 1, [Enable FIPS140-2 mode])
    AC_SUBST([FIPS140_LIBS], $LIBDL)
    AC_ARG_WITH(fips140-key, AS_HELP_STRING([--with-fips140-key],
                                 [specify the FIPS140 HMAC key for integrity]),
            fips_key="$withval", 
            fips_key="orboDeJITITejsirpADONivirpUkvarP")

    AC_DEFINE_UNQUOTED([FIPS_KEY], ["$fips_key"], [The FIPS140-2 integrity key])

    AC_ARG_WITH(fips140-module-name, AS_HELP_STRING([--with-fips140-module-name],
                                 [specify the FIPS140 module name]),
            fips_module_name="$withval",
            fips_module_name=none)

    if test "x$fips_module_name" != xnone; then
       AC_DEFINE_UNQUOTED([FIPS_MODULE_NAME], ["$fips_module_name"], [The FIPS140 module name])
    fi

    AC_ARG_WITH(fips140-module-version, AS_HELP_STRING([--with-fips140-module-version],
                                 [specify the FIPS140 module version]),
            fips_module_version="$withval",
            fips_module_version=none)

    if test "x$fips_module_version" != xnone; then
       AC_DEFINE_UNQUOTED([FIPS_MODULE_VERSION], ["$fips_module_version"], [The FIPS140 module version])
    fi

    AC_CHECK_FUNCS(dl_iterate_phdr)
  else
    enable_fips=no
    AC_MSG_WARN([[
*** 
*** This system is not supported in FIPS140 mode.
*** libdl and dladdr() are required.
*** ]])
  fi
fi

AC_ARG_ENABLE(strict-x509,
  AS_HELP_STRING([--enable-strict-x509], [enable stricter sanity checks for x509 certificates]),
    enable_strict_x509=$enableval, enable_strict_x509=no)
if test "$enable_strict_x509" != "no"; then
	AC_DEFINE([STRICT_X509], 1, [Enable stricter sanity checks for x509 certificates])
fi

AC_ARG_WITH([pkcs12-iter-count],
	[AS_HELP_STRING([--with-pkcs12-iter-count],
	  [specify iteration count for PKCS\#12 key derivation @<:@default=600000@:>@])],
	[pkcs12_iter_count="$withval"],
        [pkcs12_iter_count=600000])

AC_DEFINE_UNQUOTED([PKCS12_ITER_COUNT], [$pkcs12_iter_count],
	[The iteration count for PKCS\#12 key derivation])
AC_SUBST([PKCS12_ITER_COUNT], [$pkcs12_iter_count])

PKG_CHECK_MODULES(CMOCKA, [cmocka >= 1.0.1], [with_cmocka=yes], [with_cmocka=no])
AM_CONDITIONAL(HAVE_CMOCKA, test "$with_cmocka" != "no")

AC_ARG_WITH(idn, AS_HELP_STRING([--without-idn],
                                [disable support for IDNA]),
            try_libidn2="$withval", 
            try_libidn2=yes)

idna_support=no
with_libidn2=no

if test "$try_libidn2" = yes;then
    PKG_CHECK_MODULES(LIBIDN2, [libidn2 >= 2.0.0], [with_libidn2=yes], [with_libidn2=no])
    if test "${with_libidn2}" = "yes";then
      idna_support="IDNA 2008 (libidn2)"
      AC_DEFINE([HAVE_LIBIDN2], 1, [Define if IDNA 2008 support is enabled.])
      if test "x$GNUTLS_REQUIRES_PRIVATE" = "x"; then
        GNUTLS_REQUIRES_PRIVATE="Requires.private: libidn2"
      else
        GNUTLS_REQUIRES_PRIVATE="${GNUTLS_REQUIRES_PRIVATE}, libidn2"
      fi
    else
      AC_MSG_WARN(*** LIBIDN2 was not found. You will not be able to use IDN2008 support)
    fi
fi

AM_CONDITIONAL(HAVE_LIBIDN2, test "$with_libidn2" != "no")

if test "x$have_vista_dynamic" = "xno"; then
  AC_CHECK_TYPES([BCRYPT_ALG_HANDLE],[
    GNUTLS_LIBS_PRIVATE="${GNUTLS_LIBS_PRIVATE} -lbcrypt"
  ],[],[#include <windows.h>
  #include <bcrypt.h>])
fi
AM_CONDITIONAL(HAVE_BCRYPT, test "$ac_cv_type_BCRYPT_ALG_HANDLE" = "yes")

AC_ARG_ENABLE(non-suiteb-curves,
  AS_HELP_STRING([--disable-non-suiteb-curves], [disable curves not in SuiteB]),
    enable_non_suiteb=$enableval, enable_non_suiteb=yes)

if test "$enable_non_suiteb" = "yes";then
	dnl nettle_secp_192r1 is not really a function
	AC_CHECK_LIB(hogweed, nettle_get_secp_192r1, enable_non_suiteb=yes, enable_non_suiteb=no, [$HOGWEED_LIBS $NETTLE_LIBS])

	if test "$enable_non_suiteb" = "yes";then
		AC_DEFINE([ENABLE_NON_SUITEB_CURVES], 1, [Enable all curves])
	fi
fi
AM_CONDITIONAL(ENABLE_NON_SUITEB_CURVES, test "$enable_non_suiteb" = "yes")

AC_ARG_ENABLE(dsa,
  AS_HELP_STRING([--disable-dsa], [disable DSA]),
  enable_dsa=$enableval, enable_dsa=yes)
if test x$enable_dsa != xno; then
    AC_DEFINE([ENABLE_DSA], 1, [Define if DSA is enabled.])
fi
AM_CONDITIONAL([ENABLE_DSA], [test x$enable_dsa != xno])

# We MUST require a Nettle version that has rsa_sec_decrypt now.
save_LIBS=$LIBS
LIBS="$LIBS $HOGWEED_LIBS $NETTLE_LIBS $GMP_LIBS"
AC_CHECK_FUNCS(nettle_rsa_sec_decrypt,
    [],
    [AC_MSG_ERROR([Nettle lacks the required rsa_sec_decrypt function])]
)
LIBS=$save_LIBS

# Check for GOST28147
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_gost28147_set_key)
LIBS=$save_LIBS

# Check for Streebog support
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_streebog512_update)
LIBS=$save_LIBS

# Check for Magma
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_magma_set_key)
LIBS=$save_LIBS

# Check for Kuznyechik
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_kuznyechik_set_key)
LIBS=$save_LIBS

# Check for CMAC MAGMA support
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_cmac_magma_update)
LIBS=$save_LIBS

# Check for CMAC KUZNYECHIK support
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_cmac_kuznyechik_update)
LIBS=$save_LIBS

# Check for SIV-GCM
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_siv_gcm_encrypt_message)
LIBS=$save_LIBS
AM_CONDITIONAL([NEED_SIV_GCM], [test "$ac_cv_func_nettle_siv_gcm_encrypt_message" != yes])

# Check for RSA-OAEP
save_LIBS=$LIBS
LIBS="$LIBS $HOGWEED_LIBS $NETTLE_LIBS $GMP_LIBS"
AC_CHECK_FUNCS(nettle_rsa_oaep_sha256_encrypt)
LIBS=$save_LIBS
AM_CONDITIONAL([NEED_RSA_OAEP], [test "$ac_cv_func_nettle_rsa_oaep_sha256_encrypt" != yes])

# Check for specialized AES-CBC encryption
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_cbc_aes128_encrypt)
LIBS=$save_LIBS

# Check for incremental SHAKE
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
AC_CHECK_FUNCS(nettle_sha3_128_shake_output)
LIBS=$save_LIBS
AM_CONDITIONAL([NEED_SHAKE_OUTPUT], [test "$ac_cv_func_nettle_sha3_128_shake_output" != yes])

# Check sonames of the linked libraries needed for FIPS selftests.
save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $GMP_CFLAGS"
save_LIBS=$LIBS
LIBS="$LIBS $GMP_LIBS"
LIBGNUTLS_CHECK_SONAME([gmp], [AC_LANG_PROGRAM([
               #include <gmp.h>],[
               mpz_t n;
               mpz_init(n);])])
LIBS="$save_LIBS"
CFLAGS="$save_CFLAGS"

save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $NETTLE_CFLAGS"
save_LIBS=$LIBS
LIBS="$LIBS $NETTLE_LIBS"
LIBGNUTLS_CHECK_SONAME([nettle], [AC_LANG_PROGRAM([
               #include <nettle/sha2.h>],[
               struct sha256_ctx ctx;
               sha256_init(&ctx);])])
LIBS="$save_LIBS"
CFLAGS="$save_CFLAGS"

save_CFLAGS=$CFLAGS
# <nettle/bignum.h> includes <gmp.h>
CFLAGS="$CFLAGS $HOGWEED_CFLAGS $GMP_CFLAGS"
save_LIBS=$LIBS
LIBS="$LIBS $HOGWEED_LIBS"
LIBGNUTLS_CHECK_SONAME([hogweed], [AC_LANG_PROGRAM([
               #include <nettle/rsa.h>],[
               struct rsa_private_key priv;
               nettle_rsa_private_key_init(&priv);])])
LIBS="$save_LIBS"
CFLAGS="$save_CFLAGS"

gnutls_so=libgnutls.so.`expr "$LT_CURRENT" - "$LT_AGE"`
AC_DEFINE_UNQUOTED([GNUTLS_LIBRARY_SONAME], ["$gnutls_so"], [The soname of gnutls library])
AC_SUBST([gnutls_so])

AC_MSG_CHECKING([whether to build libdane])
AC_ARG_ENABLE(libdane,
    AS_HELP_STRING([--disable-libdane],
                   [disable the built of libdane]),
    enable_dane=$enableval, enable_dane=yes)
AC_MSG_RESULT($enable_dane)

if test "$enable_dane" != "no"; then
    LIBS="$oldlibs -lunbound"
    AC_MSG_CHECKING([for unbound library])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([
		   #include <unbound.h>],[
		   struct ub_ctx* ctx;
		   ctx = ub_ctx_create();])],
		  [AC_MSG_RESULT(yes)
		   AC_SUBST([UNBOUND_LIBS], [-lunbound])
		   AC_SUBST([UNBOUND_CFLAGS], [])
		   AC_DEFINE([HAVE_DANE], 1, [Enable the DANE library])
		   enable_dane=yes],
		  [AC_MSG_RESULT(no)
		   AC_MSG_WARN([[
*** 
*** libunbound was not found. Libdane will not be built.
*** ]])
		  enable_dane=no])
    LIBS="$oldlibs"
fi

AM_CONDITIONAL(ENABLE_DANE, test "$enable_dane" = "yes")

AC_ARG_WITH(unbound-root-key-file, AS_HELP_STRING([--with-unbound-root-key-file],
                                 [specify the unbound root key file]),
            unbound_root_key_file="$withval", 
if test "$have_win" = yes; then
    unbound_root_key_file="C:\\Program Files\\Unbound\\root.key"
else
    if test -f /var/lib/unbound/root.key;then
	unbound_root_key_file="/var/lib/unbound/root.key"
    else
        if test -f /usr/share/dns/root.key;then
	    unbound_root_key_file="/usr/share/dns/root.key"
        else
            unbound_root_key_file="/etc/unbound/root.key"
        fi
    fi
fi
)

AC_DEFINE_UNQUOTED([UNBOUND_ROOT_KEY_FILE],
  ["$unbound_root_key_file"], [The DNSSEC root key file])

system_config_file="/etc/gnutls/config"
AC_ARG_WITH(system-priority-file, AS_HELP_STRING([--with-system-priority-file],
                                 [specify the system-wide config file (set empty to disable)]),
            system_config_file="$withval"
)

AM_CONDITIONAL(DISABLE_SYSTEM_CONFIG, test -z "${system_config_file}")

if test -z "${system_config_file}";then
	AC_DEFINE([DISABLE_SYSTEM_CONFIG], 1, [Whether to disable system configuration])
fi

AC_DEFINE_UNQUOTED([SYSTEM_PRIORITY_FILE],
  ["$system_config_file"], [The system-wide gnutls configuration file])

AC_ARG_WITH(default-priority-string, AS_HELP_STRING([--with-default-priority-string],
                                 [specify the default priority string used by gnutls_set_default_priority (default is NORMAL)]),
            prio_string="$withval", 
            prio_string="NORMAL")

AC_DEFINE_UNQUOTED([DEFAULT_PRIORITY_STRING], ["$prio_string"], [The default priority string])

dnl Check for p11-kit
P11_KIT_MINIMUM=0.23.1
AC_ARG_WITH(p11-kit,
	AS_HELP_STRING([--without-p11-kit],
		[Build without p11-kit and PKCS#11 support]))
if test "$with_p11_kit" != "no"; then
	PKG_CHECK_MODULES(P11_KIT, [p11-kit-1 >= $P11_KIT_MINIMUM], [with_p11_kit=yes], [with_p11_kit=no])
	if test "$with_p11_kit" != "no";then
		AC_DEFINE([ENABLE_PKCS11], 1, [Build PKCS#11 support])
		if test "x$GNUTLS_REQUIRES_PRIVATE" = "x"; then
			GNUTLS_REQUIRES_PRIVATE="Requires.private: p11-kit-1"
		else
			GNUTLS_REQUIRES_PRIVATE="${GNUTLS_REQUIRES_PRIVATE}, p11-kit-1"
		fi
	else
	        with_p11_kit=no
	        AC_MSG_ERROR([[
*** 
*** p11-kit >= $P11_KIT_MINIMUM was not found. To disable PKCS #11 support 
*** use --without-p11-kit, otherwise you may get p11-kit from
*** https://p11-glue.freedesktop.org/p11-kit.html
*** ]])
	fi
fi

AM_CONDITIONAL(P11KIT_0_23_11_API, $PKG_CONFIG --atleast-version=0.23.11 p11-kit-1)

AM_CONDITIONAL(ENABLE_PKCS11, test "$with_p11_kit" != "no")

save_LIBS=$LIBS
LIBS="$LIBS -lm"
LIBGNUTLS_CHECK_SONAME([m], [AC_LANG_PROGRAM([
               #include <math.h>],[
               trunc (0);])])
LIBS="$save_LIBS"
CFLAGS="$save_CFLAGS"

AC_RUN_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <dlfcn.h>
      #include <stdlib.h>
    ]],
    [[void *handle = dlopen("$M_LIBRARY_SONAME", RTLD_LAZY | RTLD_GLOBAL);
      return handle != NULL ? 0 : 1;
    ]])],
  [ac_cv_dlopen_soname_works=yes],
  [ac_cv_dlopen_soname_works=no],
  [ac_cv_dlopen_soname_works=cross-compiling])

AM_CONDITIONAL([ENABLE_DLOPEN], [test "$ac_cv_dlopen_soname_works" = yes])
need_ltlibdl=no

AC_ARG_WITH(tpm2,
	AS_HELP_STRING([--without-tpm2],
		[Disable TPM2 support.]),
	[with_tpm2=$withval], [with_tpm2=auto])
AS_IF([test "$with_tpm2" != no], [
	PKG_CHECK_MODULES(TSS2_ESYS, [tss2-esys],
		[have_tss2_esys=yes], [have_tss2_esys=no])
	PKG_CHECK_MODULES(TSS2_MU, [tss2-mu],
		[have_tss2_mu=yes], [have_tss2_mu=no])
	PKG_CHECK_MODULES(TSS2_TCTILDR, [tss2-tctildr],
		[have_tss2_tctildr=yes], [have_tss2_tctildr=no])
])
AS_IF([test "$have_tss2_esys" = yes && test "$have_tss2_mu" = yes && test "$have_tss2_tctildr" = yes], [
	tss2lib="tss2-esys tss2-mu tss2-tctildr"
	AC_DEFINE([HAVE_TSS2], 1, [Have TSS2])
	with_tpm2=yes
	need_ltlibdl=yes
],
      [test "$with_tpm2" = yes], [
	AC_MSG_ERROR([[
***
*** TPM2 support was requested but the required libraries were not found.
*** To disable TPM2 support use --without-tpm2, otherwise you may get tpm2-tss from
*** https://github.com/tpm2-software/tpm2-tss
*** ]])
])

AM_CONDITIONAL(ENABLE_TPM2, test -n "$tss2lib")

AS_IF([test "$ac_cv_dlopen_soname_works" = yes], [
    save_LIBS=$LIBS
    LIBS="$LIBS $TSS2_ESYS_LIBS"
    LIBGNUTLS_CHECK_SONAME([tss2-esys], [AC_LANG_PROGRAM([
                   #include <tss2/tss2_esys.h>],[
                   Esys_Initialize (0, 0, 0);])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"

    save_LIBS=$LIBS
    LIBS="$LIBS $TSS2_MU_LIBS"
    LIBGNUTLS_CHECK_SONAME([tss2-mu], [AC_LANG_PROGRAM([
                   #include <tss2/tss2_mu.h>],[
                   Tss2_MU_INT8_Marshal (0, 0, 0, 0);])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"

    save_LIBS=$LIBS
    LIBS="$LIBS $TSS2_TCTILDR_LIBS"
    LIBGNUTLS_CHECK_SONAME([tss2-tctildr], [AC_LANG_PROGRAM([
                   #include <tss2/tss2_tctildr.h>],[
                   Tss2_TctiLdr_Initialize (0, 0);])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"
])

AC_ARG_WITH(tpm,
	AS_HELP_STRING([--without-tpm],
		[Disable TPM (trousers) support.]),
		[with_tpm=$withval], [with_tpm=yes])
if test "$with_tpm" != "no"; then
    LIBS="$oldlibs -ltspi"
    AC_MSG_CHECKING([for tss library])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([
		   #include <trousers/tss.h>
		   #include <trousers/trousers.h>],[
		   int err = Tspi_Context_Create((void *)0);
		   Trspi_Error_String(err);])],
		  [AC_MSG_RESULT(yes)
		   AC_SUBST([TSS_LIBS], [-ltspi])
		   AC_SUBST([TSS_CFLAGS], [])
		   AC_DEFINE([HAVE_TROUSERS], 1, [Enable TPM])
		   with_tpm=yes,
		   need_ltlibdl=yes],
		  [AC_MSG_RESULT(no)
		   AC_MSG_WARN([[
*** 
*** trousers was not found. TPM support will be disabled.
*** ]])
		  with_tpm=no])
    LIBS="$oldlibs"
else
    with_tpm=no
fi

AM_CONDITIONAL(ENABLE_TROUSERS, test "$with_tpm" != "no")


for l in /usr/lib64 /usr/lib /lib64 /lib /usr/lib/x86_64-linux-gnu/; do
    if test -f "${l}/libtspi.so.1";then
        default_trousers_lib="${l}/libtspi.so.1"
        break
    fi
done

AC_ARG_WITH(trousers-lib, AS_HELP_STRING([--with-trousers-lib=LIB],
                                 [set the location of the trousers library]),
            ac_trousers_lib=$withval, ac_trousers_lib=$default_trousers_lib)

if test "$with_tpm" != "no" && test -z "$ac_trousers_lib"; then
    AC_MSG_ERROR([[
  ***
  *** unable to find trousers library, please specify with --with-trousers-lib=<lib file>
  ***
  ]])
fi

AC_DEFINE_UNQUOTED([TROUSERS_LIB], ["$ac_trousers_lib"], [the location of the trousers library])
AC_SUBST(TROUSERS_LIB)


# For minitasn1.
AC_CHECK_SIZEOF(unsigned long int, 4)
AC_CHECK_SIZEOF(unsigned int, 4)
AC_CHECK_SIZEOF(time_t, 4)

AC_ARG_WITH(zlib, AS_HELP_STRING([--without-zlib],
                                 [disable zlib compression support]),
            ac_zlib=$withval, ac_zlib=yes)
if test x$ac_zlib != xno; then
    PKG_CHECK_EXISTS(zlib, ZLIB_HAS_PKGCONFIG=y, ZLIB_HAS_PKGCONFIG=n)
    if test "$ZLIB_HAS_PKGCONFIG" = "y" ; then
        PKG_CHECK_MODULES(ZLIB, [zlib])
	if test "x$GNUTLS_REQUIRES_PRIVATE" = x; then
	    GNUTLS_REQUIRES_PRIVATE="Requires.private: zlib"
	else
	    GNUTLS_REQUIRES_PRIVATE="$GNUTLS_REQUIRES_PRIVATE, zlib"
	fi
	ac_zlib=yes
    else
	AC_LIB_HAVE_LINKFLAGS(z,, [#include <zlib.h>], [compress (0, 0, 0, 0);])
	if test x$ac_cv_libz != xyes; then
	    AC_MSG_WARN([[
*** 
*** ZLIB was not found. You will not be able to use ZLIB compression.
*** ]])
        fi
	ac_zlib=$ac_cv_libz
	ZLIB_LIBS=$LIBZ
	LIBZ_PC=$LIBZ
    fi
fi
if test x$ac_zlib != xno; then
    AC_DEFINE([HAVE_LIBZ], 1, [Define if ZLIB compression is enabled.])
    need_ltlibdl=yes
fi
AM_CONDITIONAL(HAVE_ZLIB, test "$ac_zlib" = "yes")
AC_SUBST(LIBZ_PC)

AS_IF([test "$ac_cv_dlopen_soname_works" = yes], [
    save_LIBS=$LIBS
    LIBS="$LIBS $ZLIB_LIBS"
    LIBGNUTLS_CHECK_SONAME([z], [AC_LANG_PROGRAM([
                   #include <zlib.h>],[
                   compress (0, 0, 0, 0);])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"
])

AC_ARG_WITH(brotli,
	    AS_HELP_STRING([--without-brotli], [disable brotli compression support]),
	    ac_brotli=$withval, ac_brotli=yes)
AC_MSG_CHECKING([whether to include brotli compression support])
if test x$ac_brotli != xno; then
    AC_MSG_RESULT(yes)
    PKG_CHECK_MODULES(LIBBROTLIENC, [libbrotlienc >= 1.0.0], [with_libbrotlienc=yes], [with_libbrotlienc=no])
    PKG_CHECK_MODULES(LIBBROTLIDEC, [libbrotlidec >= 1.0.0], [with_libbrotlidec=yes], [with_libbrotlidec=no])
    if test "${with_libbrotlienc}" = "yes" && test "${with_libbrotlidec}" = "yes"; then
	AC_DEFINE([HAVE_LIBBROTLI], 1, [Define if BROTLI compression is enabled.])
	if test "x$GNUTLS_REQUIRES_PRIVATE" = "x"; then
	    GNUTLS_REQUIRES_PRIVATE="Requires.private: libbrotlienc, libbrotlidec"
	else
	    GNUTLS_REQUIRES_PRIVATE="${GNUTLS_REQUIRES_PRIVATE}, libbrotlienc, libbrotlidec"
	fi
	need_ltlibdl=yes
    else
	AC_MSG_WARN(*** LIBBROTLI was not found. You will not be able to use BROTLI compression.)
    fi
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(HAVE_LIBBROTLI, test "$with_libbrotlienc" != "no" && test "$with_libbrotlidec" != "no")

AS_IF([test "$ac_cv_dlopen_soname_works" = yes], [
    save_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS $LIBBROTLIENC_CFLAGS"
    save_LIBS=$LIBS
    LIBS="$LIBS $LIBBROTLIENC_LIBS"
    LIBGNUTLS_CHECK_SONAME([brotlienc], [AC_LANG_PROGRAM([
           #include <brotli/encode.h>],[
           BrotliEncoderVersion();])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"

    save_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS $LIBBROTLIDEC_CFLAGS"
    save_LIBS=$LIBS
    LIBS="$LIBS $LIBBROTLIDEC_LIBS"
    LIBGNUTLS_CHECK_SONAME([brotlidec], [AC_LANG_PROGRAM([
           #include <brotli/decode.h>],[
           BrotliDecoderVersion();])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"
])

AC_ARG_WITH(zstd,
	    AS_HELP_STRING([--without-zstd], [disable zstd compression support]),
	    ac_zstd=$withval, ac_zstd=yes)
AC_MSG_CHECKING([whether to include zstd compression support])
if test x$ac_zstd != xno; then
    AC_MSG_RESULT(yes)
    PKG_CHECK_MODULES(LIBZSTD, [libzstd >= 1.3.0], [with_libzstd=yes], [with_libzstd=no])
    if test "${with_libzstd}" = "yes"; then
	AC_DEFINE([HAVE_LIBZSTD], 1, [Define if ZSTD compression is enabled.])
	if test "x$GNUTLS_REQUIRES_PRIVATE" = "x"; then
	    GNUTLS_REQUIRES_PRIVATE="Requires.private: libzstd"
	else
	    GNUTLS_REQUIRES_PRIVATE="${GNUTLS_REQUIRES_PRIVATE}, libzstd"
	fi
	need_ltlibdl=yes
    else
	AC_MSG_WARN(*** LIBZSTD was not found. You will not be able to use ZSTD compression.)
    fi
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(HAVE_LIBZSTD, test "$with_libzstd" != "no")

AS_IF([test "$ac_cv_dlopen_soname_works" = yes], [
    save_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS $LIBZSTD_CFLAGS"
    save_LIBS=$LIBS
    LIBS="$LIBS $LIBZSTD_LIBS"
    LIBGNUTLS_CHECK_SONAME([zstd], [AC_LANG_PROGRAM([
           #include <zstd.h>],[
           ZSTD_versionNumber();])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"
])

AC_ARG_WITH(liboqs,
	AS_HELP_STRING([--with-liboqs],
		[Enable liboqs support.]),
	[with_liboqs=$withval], [with_liboqs=no])
AS_IF([test "$with_liboqs" != no], [PKG_CHECK_MODULES(LIBOQS, [liboqs >= 0.10.1], [have_liboqs=yes], [have_liboqs=no])])
AS_IF([test "$have_liboqs" = yes], [AC_DEFINE([HAVE_LIBOQS], 1, [Have liboqs])],
      [test "$with_liboqs" = yes], [AC_MSG_ERROR([[
***
*** liboqs support was requested but the required libraries were not found.
*** ]])])

save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $LIBOQS_CFLAGS"
AC_CHECK_DECLS([OQS_SHA3_set_callbacks])
CFLAGS="$save_CFLAGS"

# liboqs 0.10.1 didn't expose OQS_SHA3_set_callbacks from the header
# file, so extra treatment is needed:
# https://github.com/open-quantum-safe/liboqs/pull/1832
AM_CONDITIONAL(NEED_LIBOQS_SHA3_OPS_H, test "$ac_cv_have_decl_OQS_SHA3_set_callbacks" != yes)

AM_CONDITIONAL(ENABLE_LIBOQS, test "$have_liboqs" = yes)

AS_IF([test "$ac_cv_dlopen_soname_works" = yes], [
    save_CFLAGS=$CFLAGS
    CFLAGS="$CFLAGS $LIBOQS_CFLAGS"
    save_LIBS=$LIBS
    LIBS="$LIBS $LIBOQS_LIBS"
    LIBGNUTLS_CHECK_SONAME([oqs], [AC_LANG_PROGRAM([
                   #include <oqs/oqs.h>],[
                   OQS_version ();])])
    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"
])

AM_CONDITIONAL(NEED_LTLIBDL, test "$need_ltlibdl" = yes)

# export for use in scripts
AC_SUBST(ac_cv_sizeof_time_t)

AC_SUBST(GNUTLS_LIBS_PRIVATE)
AC_SUBST(GNUTLS_REQUIRES_PRIVATE)


AC_ARG_WITH([default-trust-store-pkcs11],
  [AS_HELP_STRING([--with-default-trust-store-pkcs11=URI],
    [use the given pkcs11 uri as default trust store])])

if test "x$with_default_trust_store_pkcs11" != x; then
  if test "x$with_p11_kit" = xno; then
    AC_MSG_ERROR([cannot use pkcs11 store without p11-kit])
  fi
  AC_DEFINE_UNQUOTED([DEFAULT_TRUST_STORE_PKCS11],
    ["$with_default_trust_store_pkcs11"], [use the given pkcs11 uri as default trust store])
fi

AM_CONDITIONAL([HAVE_PKCS11_TRUST_STORE], [test -n "${with_default_trust_store_pkcs11}"])

AC_ARG_WITH([default-trust-store-dir],
  [AS_HELP_STRING([--with-default-trust-store-dir=DIR],
    [use the given directory as default trust store])])

if test "x$with_default_trust_store_dir" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_TRUST_STORE_DIR],
    ["$with_default_trust_store_dir"], [use the given directory as default trust store])
fi

dnl auto detect https://lists.gnu.org/archive/html/help-gnutls/2012-05/msg00004.html
AC_ARG_WITH([default-trust-store-file],
  [AS_HELP_STRING([--with-default-trust-store-file=FILE],
    [use the given file default trust store])], with_default_trust_store_file="$withval",
  [if test "$build" = "$host" && test x$with_default_trust_store_pkcs11 = x && test x$with_default_trust_store_dir = x && test x$have_macosx = x;then
  for i in \
    /etc/ssl/ca-bundle.pem \
    /etc/ssl/certs/ca-certificates.crt \
    /etc/pki/tls/cert.pem \
    /usr/local/share/certs/ca-root-nss.crt \
    /etc/ssl/cert.pem
    do
    if test -e "$i"; then
      with_default_trust_store_file="$i"
      break
    fi
  done
  fi]
)

if test "$with_default_trust_store_file" = "no";then
  with_default_trust_store_file=""
fi

AC_ARG_WITH([default-crl-file],
  [AS_HELP_STRING([--with-default-crl-file=FILE],
    [use the given CRL file as default])])

AC_ARG_WITH([default-blocklist-file],
  [AS_HELP_STRING([--with-default-blocklist-file=FILE],
    [use the given certificate blocklist file as default])])

AC_ARG_WITH([default-blacklist-file],
  [AS_HELP_STRING([--with-default-blacklist-file=FILE],
    [use --with-default-blocklist-file instead])],
  [AC_MSG_ERROR([use --with-default-blocklist-file instead])])

if test "x$with_default_trust_store_file" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_TRUST_STORE_FILE],
    ["$with_default_trust_store_file"], [use the given file default trust store])
fi

if test "x$with_default_crl_file" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_CRL_FILE],
    ["$with_default_crl_file"], [use the given CRL file])
fi

if test "x$with_default_blocklist_file" != x; then
  AC_DEFINE_UNQUOTED([DEFAULT_BLOCKLIST_FILE],
    ["$with_default_blocklist_file"], [use the given certificate blocklist file])
fi

LIBGNUTLS_LIBS="-L${libdir} -lgnutls $LIBS"
LIBGNUTLS_CFLAGS="-I${includedir}"
AC_SUBST(LIBGNUTLS_LIBS)
AC_SUBST(LIBGNUTLS_CFLAGS)

AM_CONDITIONAL(NEEDS_LIBRT, test "$gnutls_needs_librt" = "yes")

AC_DEFINE([GNUTLS_INTERNAL_BUILD], 1, [We allow temporarily usage of deprecated functions - until they are removed.])

hw_features=
if test "$enable_cryptodev" = yes; then
	hw_features="$hw_features cryptodev"
fi
if test "$enable_afalg" = yes; then
	hw_features="$hw_features afalg"
fi
if test "$use_padlock" = yes; then
	hw_features="$hw_features padlock"
fi
if test "$with_p11_kit" = yes; then
	hw_features="$hw_features pkcs11"
fi
if test "$with_tpm" = yes; then
	hw_features="$hw_features tpm"
fi
if test "$have_tpm2" = yes; then
	hw_features="$hw_features tpm2"
fi
if test "$enable_ktls" = yes; then
	hw_features="$hw_features ktls"
fi
hw_features=`eval 'expr "$hw_features" : " \(.*\)"'`
AC_DEFINE_UNQUOTED([HW_FEATURES], ["$hw_features"],
	[The enabled hardware features])

tls_features=
if test "$ac_enable_ssl3" = yes; then
	tls_features="$tls_features ssl3"
fi
if test "$ac_enable_ssl2" = yes; then
	tls_features="$tls_features ssl2-compat"
fi
if test "$ac_enable_srtp" = yes; then
	tls_features="$tls_features srtp"
fi
if test "$ac_enable_alpn" = yes; then
	tls_features="$tls_features alpn"
fi
if test "$ac_enable_ocsp" = yes; then
	tls_features="$tls_features ocsp"
fi
if test "$ac_enable_srp" = yes; then
	tls_features="$tls_features srp"
fi
if test "$ac_enable_psk" = yes; then
	tls_features="$tls_features psk"
fi
if test "$ac_enable_dhe" = yes; then
	tls_features="$tls_features dhe"
fi
if test "$ac_enable_ecdhe" = yes; then
	tls_features="$tls_features ecdhe"
fi
if test "$ac_enable_anon" = yes; then
	tls_features="$tls_features auth-anon"
fi
if test "$ac_enable_heartbeat" = yes; then
	tls_features="$tls_features heartbeat"
fi
tls_features=`eval 'expr "$tls_features" : " \(.*\)"'`
AC_DEFINE_UNQUOTED([TLS_FEATURES], ["$tls_features"],
	[The enabled TLS features])

AC_DEFINE([fread_file], [_gnutls_fread_file], [static lib rename])
AC_DEFINE([read_file], [_gnutls_read_file], [static lib rename])

dnl configuration options for config file parsing (inih)
AC_DEFINE([INI_MAX_LINE], 2048, [inih maximum line size])
AC_DEFINE([INI_ALLOW_INLINE_COMMENTS], 1, [whether to allowin inline comments])
AC_DEFINE([INI_STOP_ON_FIRST_ERROR], 1, [whether to stop on first error])
AC_DEFINE_UNQUOTED([INI_INLINE_COMMENT_PREFIXES], [";#"], [The inline comment prefixes])
AC_DEFINE_UNQUOTED([INI_START_COMMENT_PREFIXES], [";#"], [The comment prefixes])

AC_CONFIG_FILES([
  Makefile
  doc/Makefile
  doc/credentials/Makefile
  doc/credentials/srp/Makefile
  doc/credentials/x509/Makefile
  doc/doxygen/Doxyfile
  doc/examples/Makefile
  doc/latex/Makefile
  doc/manpages/Makefile
  doc/reference/Makefile
  doc/reference/version.xml
  doc/scripts/Makefile
  extra/Makefile
  extra/includes/Makefile
  libdane/Makefile
  libdane/includes/Makefile
  libdane/gnutls-dane.pc
  gl/Makefile
  lib/Makefile
  lib/accelerated/Makefile
  lib/accelerated/x86/Makefile
  lib/accelerated/aarch64/Makefile
  lib/algorithms/Makefile
  lib/auth/Makefile
  lib/ext/Makefile
  lib/extras/Makefile
  lib/gnutls.pc
  lib/includes/Makefile
  lib/includes/gnutls/gnutls.h
  lib/liboqs/Makefile
  lib/minitasn1/Makefile
  lib/nettle/Makefile
  lib/x509/Makefile
  lib/unistring/Makefile
  po/Makefile.in
  src/Makefile
  src/gl/Makefile
  src/gl/tests/Makefile
  tests/Makefile
  tests/windows/Makefile
  tests/cert-tests/Makefile
  tests/slow/Makefile
  tests/suite/Makefile
  fuzz/Makefile
])

AC_OUTPUT

dnl  Warning flags:        errors: ${WERROR_CFLAGS} warnings: ${WARN_CFLAGS}
AC_MSG_NOTICE([summary of build options:

  version:              ${VERSION} shared $LT_CURRENT:$LT_REVISION:$LT_AGE
  Host/Target system:   ${host}
  Build system:         ${build}
  Install prefix:       ${prefix}
  Compiler:             ${CC}
  Valgrind:             $opt_valgrind_tests ${VALGRIND}
  CFlags:               ${CFLAGS}
  Library types:        Shared=${enable_shared}, Static=${enable_static}
  Local libtasn1:       ${included_libtasn1}
  Local unistring:      ${included_unistring}
  Use nettle-mini:      ${mini_nettle}
  Documentation:        ${enable_doc} (manpages: ${enable_manpages})
])

AC_MSG_NOTICE([External hardware support:

  /dev/crypto:          $enable_cryptodev
  AF_ALG support:       $enable_afalg
  Hardware accel:       $hw_accel
  Padlock accel:        $use_padlock
  Random gen. variant:  $rnd_variant
  PKCS#11 support:      $with_p11_kit
  TPM support:          $with_tpm
  TPM2 support:         $with_tpm2
  KTLS support:         $enable_ktls
])

if test -n "$ac_trousers_lib" && test "$with_tpm" != "no";then
AC_MSG_NOTICE([
  TPM library:          $ac_trousers_lib
])
fi

if test "$with_tpm2" != "no";then
AC_MSG_NOTICE([
  TPM2 library:         $tss2lib
])
fi

AC_MSG_NOTICE([Optional features:
(note that included applications might not compile properly
if features are disabled)

  SSL3.0 support:       $ac_enable_ssl3
  SSL2.0 client hello:  $ac_enable_ssl2
  Allow SHA1 sign:      $ac_allow_sha1
  DTLS-SRTP support:    $ac_enable_srtp
  ALPN support:         $ac_enable_alpn
  OCSP support:         $ac_enable_ocsp
  SRP support:          $ac_enable_srp
  PSK support:          $ac_enable_psk
  DHE support:          $ac_enable_dhe
  ECDHE support:        $ac_enable_ecdhe
  GOST support:         $ac_enable_gost
  Anon auth support:    $ac_enable_anon
  Heartbeat support:    $ac_enable_heartbeat
  IDNA support:         $idna_support
  Non-SuiteB curves:    $enable_non_suiteb
  FIPS140 mode:         $enable_fips
  Strict DER time:	$ac_strict_der_time
])

AC_MSG_NOTICE([Optional libraries:

  C++ library:          $use_cxx
  DANE library:         $enable_dane
  OpenSSL compat:       $enable_openssl
])

AC_MSG_NOTICE([System files:

  Trust store pkcs11:   $with_default_trust_store_pkcs11
  Trust store dir:      $with_default_trust_store_dir
  Trust store file:     $with_default_trust_store_file
  Blocklist file:       $with_default_blocklist_file
  CRL file:             $with_default_crl_file
  Configuration file:   $system_config_file
  DNSSEC root key file: $unbound_root_key_file
])

if test ! -f "$unbound_root_key_file"; then
AC_MSG_WARN([[
*** 
*** The DNSSEC root key file in $unbound_root_key_file was not found. 
*** This file is needed for the verification of DNSSEC responses.
*** Use the command: unbound-anchor -a "$unbound_root_key_file"
*** to generate or update it.
*** ]])
fi

if test "${enable_static}" != no;then
AC_MSG_WARN([[
*** GnuTLS will be build as a static library. That means that library
*** constructors for gnutls_global_init will not be made available to
*** linking applications. If you are building that library for arbitrary
*** applications to link, do not enable static linking.
]])
fi

if test "$enable_fuzzer_target" != "no";then
AC_MSG_WARN([[
*** This version of the library is for fuzzying purposes and is intentionally broken!
]])
fi
