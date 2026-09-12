dnl Process this file with autoconf to produce a configure script.
dnl Note we do not use AC_PREREQ here!  See aclocal.m4 for what we use instead.
AC_INIT([GNU C Library], [(see version.h)], [https://sourceware.org/bugzilla/],
  [glibc], [https://www.gnu.org/software/glibc/])
AC_CONFIG_SRCDIR([include/features.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([scripts])

ACX_PKGVERSION([GNU libc])
ACX_BUGURL([https://www.gnu.org/software/libc/bugs.html])
AC_DEFINE_UNQUOTED([PKGVERSION], ["$PKGVERSION"],
		   [Package description])
AC_DEFINE_UNQUOTED([REPORT_BUGS_TO], ["$REPORT_BUGS_TO"],
		   [Bug reporting address])

# Glibc should not depend on any header files
AC_DEFUN([_AC_INCLUDES_DEFAULT_REQUIREMENTS],
  [m4_divert_text([DEFAULTS],
    [ac_includes_default='/* none */'])])

# We require GCC, and by default use its preprocessor.  Override AC_PROG_CPP
# here to work around the Autoconf issue discussed in
# <https://sourceware.org/ml/libc-alpha/2013-01/msg00721.html>.
AC_DEFUN([AC_PROG_CPP],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_ARG_VAR([CPP],      [C preprocessor])dnl
_AC_ARG_VAR_CPPFLAGS()dnl
# On Suns, sometimes $CPP names a directory.
if test -n "$CPP" && test -d "$CPP"; then
  CPP=
fi
if test -z "$CPP"; then
  CPP="$CC -E"
fi
AC_SUBST(CPP)dnl
])# AC_PROG_CPP

# We require GCC.  Override _AC_PROG_CC_C89 here to work around the Autoconf
# issue discussed in
# <https://sourceware.org/ml/libc-alpha/2013-01/msg00757.html>.
AC_DEFUN([_AC_PROG_CC_C89], [[$1]])

dnl This is here so we can set $subdirs directly based on configure fragments.
AC_CONFIG_SUBDIRS()

AC_CANONICAL_HOST

AC_PROG_CC
AC_ARG_VAR([TEST_CC],
	   [C compiler for testing])
if test -z "$TEST_CC"; then
  TEST_CC="$CC"
fi
if test $host != $build; then
  AC_CHECK_PROGS(BUILD_CC, gcc cc)
fi
AC_SUBST(cross_compiling)
AC_PROG_CPP

# This will get text that should go into config.make.
config_vars=

AC_ARG_ENABLE([static-c++-tests],
	      AS_HELP_STRING([--disable-static-c++-tests],
			     [disable static C++ tests@<:@default=no@:>@]),
	      [static_cxx_tests=$enableval],
	      [static_cxx_tests=yes])
LIBC_CONFIG_VAR([static-cxx-tests], [$static_cxx_tests])

AC_ARG_ENABLE([static-c++-link-check],
	      AS_HELP_STRING([--disable-static-c++-link-check],
			     [disable static C++ link check @<:@default=no@:>@]),
	      [static_cxx_link_check=$enableval],
	      [static_cxx_link_check=yes])

# We need the C++ compiler for testing and libsupport.
AC_PROG_CXX
AC_ARG_VAR([TEST_CXX],
	   [C++ compiler for testing])
saved_CXX="$CXX"
if test -z "$TEST_CXX"; then
  TEST_CXX="$CXX"
else
  CXX="$TEST_CXX"
fi
# It's useless to us if it can't link programs (e.g. missing -lstdc++).
AC_CACHE_CHECK([whether $CXX can link programs], libc_cv_cxx_link_ok, [dnl
AC_LANG_PUSH([C++])
# Default, dynamic case.
AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
	       [libc_cv_cxx_link_ok=yes],
	       [libc_cv_cxx_link_ok=no])
if test $static_cxx_link_check$static_cxx_tests = yesyes; then
  # Static case.
  old_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -static"
  AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <iostream>

int
main()
{
  std::cout << "Hello, world!";
  return 0;
}
])],
		 [],
		 [libc_cv_cxx_link_ok=no])
  LDFLAGS="$old_LDFLAGS"
fi
AC_LANG_POP([C++])])
CXX="$saved_CXX"
AS_IF([test $libc_cv_cxx_link_ok != yes], [CXX=; TEST_CXX=])

if test "`cd $srcdir; pwd -P`" = "`pwd -P`"; then
  AC_MSG_ERROR([you must configure in a separate build directory])
fi

# Check for a --with-gd argument and set libgd-LDFLAGS in config.make.
AC_ARG_WITH([gd],
	    AS_HELP_STRING([--with-gd=DIR],
			   [find libgd include dir and library with prefix DIR]),
	    [dnl
case "$with_gd" in
yes|''|no) ;;
*) libgd_include="-I$withval/include"
   libgd_ldflags="-L$withval/lib" ;;
esac
])
AC_ARG_WITH([gd-include],
	    AS_HELP_STRING([--with-gd-include=DIR],
			   [find libgd include files in DIR]),
	    [dnl
case "$with_gd_include" in
''|no) ;;
*) libgd_include="-I$withval" ;;
esac
])
AC_ARG_WITH([gd-lib],
	    AS_HELP_STRING([--with-gd-lib=DIR],
			   [find libgd library files in DIR]),
	    [dnl
case "$with_gd_lib" in
''|no) ;;
*) libgd_ldflags="-L$withval" ;;
esac
])

if test -n "$libgd_include"; then
  config_vars="$config_vars
CFLAGS-memusagestat.c = $libgd_include"
fi
if test -n "$libgd_ldflags"; then
  config_vars="$config_vars
libgd-LDFLAGS = $libgd_ldflags"
fi

dnl Arguments to specify presence of other packages/features.
AC_ARG_WITH([binutils],
	    AS_HELP_STRING([--with-binutils=PATH],
			   [specify location of binutils (as and ld)]),
	    [path_binutils=$withval],
	    [path_binutils=''])
AC_ARG_WITH([selinux],
	    AS_HELP_STRING([--with-selinux],
			   [if building with SELinux support]),
	    [with_selinux=$withval],
	    [with_selinux=auto])

AC_ARG_WITH([headers],
	    AS_HELP_STRING([--with-headers=PATH],
			   [location of system headers to use
			    (for example /usr/src/linux/include)
			    @<:@default=compiler default@:>@]),
	    [sysheaders=$withval],
	    [sysheaders=''])
AC_SUBST(sysheaders)

dnl Additional build flags injection.
AC_ARG_WITH([nonshared-cflags],
	    AS_HELP_STRING([--with-nonshared-cflags=CFLAGS],
			   [build nonshared libraries with additional CFLAGS]),
	    [extra_nonshared_cflags=$withval],
	    [extra_nonshared_cflags=])
AC_SUBST(extra_nonshared_cflags)
AC_ARG_WITH([rtld-early-cflags],
	    AS_HELP_STRING([--with-rtld-early-cflags=CFLAGS],
			   [build early initialization with additional CFLAGS]),
	    [rtld_early_cflags=$withval],
	    [rtld_early_cflags=])
AC_SUBST(rtld_early_cflags)

AC_ARG_WITH([timeoutfactor],
	    AS_HELP_STRING([--with-timeoutfactor=NUM],
			   [specify an integer to scale the timeout]),
	    [timeoutfactor=$withval],
	    [timeoutfactor=1])
AC_DEFINE_UNQUOTED(TIMEOUTFACTOR, $timeoutfactor)

man_pages_version=6.9.1

AC_ARG_WITH([man-pages],
	    AS_HELP_STRING([--with-man-pages=VERSION],
			   [tie manual to a specific man-pages version]),
	    [man_pages_version=$withval],
	    [])
AC_SUBST(man_pages_version)

AC_ARG_ENABLE([sanity-checks],
	      AS_HELP_STRING([--disable-sanity-checks],
			     [really do not use threads (should not be used except in special situations) @<:@default=yes@:>@]),
	      [enable_sanity=$enableval],
	      [enable_sanity=yes])

AC_ARG_ENABLE([shared],
	      AS_HELP_STRING([--enable-shared],
			     [build shared library @<:@default=yes if GNU ld@:>@]),
	      [shared=$enableval],
	      [shared=yes])
AC_ARG_ENABLE([profile],
	      AS_HELP_STRING([--enable-profile],
			     [build profiled library @<:@default=no@:>@]),
	      [profile=$enableval],
	      [profile=no])
AC_ARG_ENABLE([default-pie],
	      AS_HELP_STRING([--disable-default-pie],
			     [Do not build glibc programs and the testsuite as PIE @<:@default=no@:>@]),
	      [default_pie=$enableval],
	      [default_pie=yes])
AC_ARG_ENABLE([timezone-tools],
	      AS_HELP_STRING([--disable-timezone-tools],
			     [do not install timezone tools @<:@default=install@:>@]),
	      [enable_timezone_tools=$enableval],
	      [enable_timezone_tools=yes])
AC_SUBST(enable_timezone_tools)

AC_ARG_ENABLE([hardcoded-path-in-tests],
	      AS_HELP_STRING([--enable-hardcoded-path-in-tests],
			     [hardcode newly built glibc path in tests @<:@default=no@:>@]),
	      [hardcoded_path_in_tests=$enableval],
	      [hardcoded_path_in_tests=no])
AC_SUBST(hardcoded_path_in_tests)

AC_ARG_ENABLE([hidden-plt],
	      AS_HELP_STRING([--disable-hidden-plt],
			     [do not hide internal function calls to avoid PLT]),
	      [hidden=$enableval],
	      [hidden=yes])
if test "x$hidden" = xno; then
  AC_DEFINE(NO_HIDDEN)
fi

AC_ARG_ENABLE([bind-now],
	      AS_HELP_STRING([--enable-bind-now],
			     [disable lazy relocations in DSOs]),
	      [bindnow=$enableval],
	      [bindnow=no])
AC_SUBST(bindnow)
if test "x$bindnow" = xyes; then
  AC_DEFINE(BIND_NOW)
fi

dnl Build glibc with -fstack-protector, -fstack-protector-all, or
dnl -fstack-protector-strong.
AC_ARG_ENABLE([stack-protector],
	      AS_HELP_STRING([--enable-stack-protector=@<:@yes|no|all|strong@:>@],
			     [Use -fstack-protector[-all|-strong] to detect glibc buffer overflows]),
	      [enable_stack_protector=$enableval],
	      [enable_stack_protector=no])
case "$enable_stack_protector" in
all|yes|no|strong) ;;
*) AC_MSG_ERROR([Not a valid argument for --enable-stack-protector: "$enable_stack_protector"]);;
esac

AC_ARG_ENABLE([force-install],
	      AS_HELP_STRING([--disable-force-install],
			     [don't force installation of files from this package, even if they are older than the installed files]),
	      [force_install=$enableval],
	      [force_install=yes])
AC_SUBST(force_install)

AC_ARG_ENABLE([maintainer-mode],
	      AS_HELP_STRING([--enable-maintainer-mode],
			     [enable make rules and dependencies not useful (and sometimes confusing) to the casual installer]),
	      [maintainer=$enableval],
	      [maintainer=no])

dnl On some platforms we allow dropping compatibility with all kernel
dnl versions.
AC_ARG_ENABLE([kernel],
	      AS_HELP_STRING([--enable-kernel=VERSION],
			     [compile for compatibility with kernel not older than VERSION]),
	      [minimum_kernel=$enableval],
	      [])
dnl Prevent unreasonable values.
if test "$minimum_kernel" = yes || test "$minimum_kernel" = no; then
  # Better nothing than this.
  minimum_kernel=""
else
  if test "$minimum_kernel" = current; then
    minimum_kernel=`uname -r 2>/dev/null` || minimum_kernel=
  fi
fi

AC_ARG_ENABLE([werror],
	      AS_HELP_STRING([--disable-werror],
			     [do not build with -Werror]),
	      [enable_werror=$enableval],
	      [enable_werror=yes])
AC_SUBST(enable_werror)

AC_ARG_ENABLE([multi-arch],
	      AS_HELP_STRING([--enable-multi-arch],
			     [enable single DSO with optimizations for multiple architectures]),
	      [multi_arch=$enableval],
	      [multi_arch=default])

AC_ARG_ENABLE([systemtap],
              [AS_HELP_STRING([--enable-systemtap],
	       [enable systemtap static probe points @<:@default=no@:>@])],
              [systemtap=$enableval],
	      [systemtap=no])
if test "x$systemtap" != xno; then
  AC_CACHE_CHECK([for systemtap static probe support], libc_cv_sdt, [dnl
  old_CFLAGS="$CFLAGS"
  CFLAGS="-std=gnu11 $CFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#include <sys/sdt.h>
void foo (int i, void *p)
{
  asm ("" STAP_PROBE_ASM (foo, bar, STAP_PROBE_ASM_TEMPLATE (2)) ""
       :: STAP_PROBE_ASM_OPERANDS (2, i, p));
}]])], [libc_cv_sdt=yes], [libc_cv_sdt=no])
  CFLAGS="$old_CFLAGS"])
  if test $libc_cv_sdt = yes; then
    AC_DEFINE([USE_STAP_PROBE])
  elif test "x$systemtap" != xauto; then
    AC_MSG_FAILURE([systemtap support needs sys/sdt.h with asm support])
  fi
fi

AC_ARG_ENABLE([build-nscd],
	      [AS_HELP_STRING([--disable-build-nscd],
	       [disable building and installing the nscd daemon])],
	      [build_nscd=$enableval],
	      [build_nscd=default])
AC_SUBST(build_nscd)

# Note the use of $use_nscd is near the bottom of the file.
AC_ARG_ENABLE([nscd],
	      [AS_HELP_STRING([--disable-nscd],
	       [library functions will not contact the nscd daemon])],
	      [use_nscd=$enableval],
	      [use_nscd=yes])

AC_ARG_ENABLE([pt_chown],
	      [AS_HELP_STRING([--enable-pt_chown],
	       [Enable building and installing pt_chown])],
	      [build_pt_chown=$enableval],
	      [build_pt_chown=no])
AC_SUBST(build_pt_chown)
if test "$build_pt_chown" = yes; then
  AC_DEFINE(HAVE_PT_CHOWN)
fi

# The abi-tags file uses a fairly simplistic model for name recognition that
# can't distinguish i486-pc-linux-gnu fully from i486-pc-gnu.  So we mutate a
# $host_os of `gnu*' here to be `gnu-gnu*' just so that it can tell.
# This doesn't get used much beyond that, so it's fairly safe.
case "$host_os" in
linux*)
  ;;
gnu*)
  host_os=`echo $host_os | sed -e 's/gnu/gnu-gnu/'`
  ;;
esac

AC_ARG_ENABLE([mathvec],
	      [AS_HELP_STRING([--enable-mathvec],
	      [Enable building and installing mathvec @<:@default depends on architecture@:>@])],
	      [build_mathvec=$enableval],
	      [build_mathvec=notset])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[]], [[
#ifndef __CET__
# error no CET compiler support
#endif]])],
	       [libc_cv_compiler_default_cet=yes],
	       [libc_cv_compiler_default_cet=no])

AC_ARG_ENABLE([cet],
	      AS_HELP_STRING([--enable-cet],
			     [enable Intel Control-flow Enforcement Technology (CET), x86 only]),
	      [enable_cet=$enableval],
	      [enable_cet=$libc_cv_compiler_default_cet])

AC_ARG_ENABLE([scv],
	      AS_HELP_STRING([--disable-scv],
			     [syscalls will not use scv instruction, even if the kernel supports it, powerpc only]),
	      [use_scv=$enableval],
	      [use_scv=yes])

AS_IF([[test "$use_scv" != "no"]],[AC_DEFINE(USE_PPC_SCV)])

dnl Build glibc with _FORTIFY_SOURCE
AC_ARG_ENABLE(fortify-source,
              AS_HELP_STRING([--enable-fortify-source@<:@=1|2|3@:>@],
                             [Use -D_FORTIFY_SOURCE=[1|2|3] to control code hardening, defaults to highest possible value supported by the build compiler.]),
              [enable_fortify_source=$enableval],
              [enable_fortify_source=no])
case "$enable_fortify_source" in
1|2|3|no|yes) ;;
*) AC_MSG_ERROR([Not a valid argument for --enable-fortify-source: "$enable_fortify_source"]);;
esac

AC_ARG_ENABLE([sframe],
	      [AS_HELP_STRING([--enable-sframe],
			      [Enable building with SFrame support @<:@default=no@:>@])],
	      [use_sframe=$enableval],
	      [use_sframe=no])

# We keep the original values in `$config_*' and never modify them, so we
# can write them unchanged into config.make.  Everything else uses
# $machine, $vendor, and $os, and changes them whenever convenient.
config_machine=$host_cpu config_vendor=$host_vendor config_os=$host_os

# Don't allow vendor == "unknown"
test "$config_vendor" = unknown && config_vendor=
config_os="`echo $config_os | sed 's/^unknown-//'`"

# Some configurations imply other options.
elf=yes

# The configure fragment of a port can modify these to supplement
# or override the table in the case statement below.  No fragment should
# ever change the config_* variables, however.
machine=$config_machine
vendor=$config_vendor
os=$config_os
base_os=''

submachine=
AC_ARG_WITH([cpu],
	    AS_HELP_STRING([--with-cpu=CPU], [select code for CPU variant]),
	    [dnl
  case "$withval" in
  yes|'') AC_MSG_ERROR([--with-cpu requires an argument]) ;;
  no) ;;
  *) submachine="$withval" ;;
  esac
])

# An preconfigure script can set this when it wants to disable the sanity
# check below.
libc_config_ok=no

# A preconfigure script for a system that may or may not use fpu
# sysdeps directories sets this to a preprocessor conditional for
# whether to use such directories.
with_fp_cond=1

# A preconfigure script may define another name to TLS descriptor variant
mtls_descriptor=gnu2

# A preconfigure script may define another name to traditional TLS variant
mtls_traditional=gnu

dnl Let sysdeps/*/preconfigure act here.
LIBC_PRECONFIGURE([$srcdir], [for sysdeps])


###
### By using the undocumented --enable-hacker-mode option for configure
### one can skip this test to make the configuration not fail for unsupported
### platforms.
###
if test -z "$enable_hacker_mode" && test x"$libc_config_ok" != xyes; then
  case "$machine-$host_os" in
  *-linux* | *-gnu*)
    ;;
  *)
    AC_MSG_ERROR([
*** The GNU C library is currently unavailable for this platform.
*** If you are interested in seeing glibc on this platform visit
*** the "How to submit a new port" in the wiki:
***   https://sourceware.org/glibc/wiki/#Development
*** and join the community!])
    ;;
  esac
fi

# Set base_machine if not set by a preconfigure fragment.
test -n "$base_machine" || base_machine=$machine
AC_SUBST(base_machine)

### Locate tools.

AC_PROG_INSTALL
if test "$INSTALL" = "${srcdir}/scripts/install-sh -c"; then
  # The makefiles need to use a different form to find it in $srcdir.
  INSTALL='\$(..)./scripts/install-sh -c'
fi
AC_PROG_LN_S

LIBC_PROG_BINUTILS

# Accept binutils 2.39 or newer.
libc_cv_with_lld=no
case $($LD --version) in
  "GNU gold"*)
  # Accept gold 1.14 or higher
    AC_CHECK_PROG_VER(LD, $LD, --version,
		    [GNU gold.* \([0-9][0-9]*\.[0-9.]*\)],
		    [1.1[4-9]*|1.[2-9][0-9]*|1.1[0-9][0-9]*|[2-9].*|[1-9][0-9]*],
		    LD=: critic_missing="$critic_missing GNU gold")
    ;;
  "LLD"*)
  # Accept LLD 13.0.0 or higher
    AC_CHECK_PROG_VER(LD, $LD, --version,
		    [LLD.* \([0-9][0-9]*\.[0-9.]*\)],
		    [1[3-9].*|[2-9][0-9].*],
		    LD=: critic_missing="$critic_missing LLD")
    libc_cv_with_lld=yes
    ;;
  *)
    AC_CHECK_PROG_VER(LD, $LD, --version,
		    [GNU ld.* \([0-9][0-9]*\.[0-9.]*\)],
		    [2.[1-9][0-9][0-9]*|2.39*|2.[4-9][0-9]*|[3-9].*|[1-9][0-9]*],
		    LD=: critic_missing="$critic_missing GNU ld")
    ;;
esac
LIBC_CONFIG_VAR([with-lld], [$libc_cv_with_lld])

# These programs are version sensitive.
AC_CHECK_PROG_VER(MAKE, gnumake gmake make, --version,
  [GNU Make[^0-9]*\([0-9][0-9.]*\)],
  [[4-9].* | [1-9][0-9]*], critic_missing="$critic_missing make")

AC_CHECK_PROG_VER(MSGFMT, gnumsgfmt gmsgfmt msgfmt, --version,
  [GNU gettext.* \([0-9]*\.[0-9.]*\)],
  [0.10.3[6-9]* | 0.10.[4-9][0-9]* | 0.1[1-9]* | 0.[2-9][0-9]* | [1-9].*],
  MSGFMT=: aux_missing="$aux_missing msgfmt")
AC_CHECK_PROG_VER(MAKEINFO, makeinfo, --version,
  [GNU texinfo.* \([0-9][0-9.]*\)],
  [4.[7-9]*|4.[1-9][0-9]*|[5-9].*],
  MAKEINFO=: aux_missing="$aux_missing makeinfo")
AC_CHECK_PROG_VER(SED, sed, --version,
  [GNU sed[^0-9]* \([0-9]*\.[0-9.]*\)],
  [3.0[2-9]*|3.[1-9]*|[4-9]*],
  SED=: aux_missing="$aux_missing sed")
AC_CHECK_PROG_VER(AWK, gawk, --version,
  [GNU Awk[^0-9]*\([0-9][0-9.]*\)],
  [3.1.[2-9]*|3.[2-9]*|[4-9]*], critic_missing="$critic_missing gawk")
AC_CHECK_PROG_VER(BISON, bison, --version,
  [bison (GNU Bison) \([0-9]*\.[0-9.]*\)],
  [2.7*|[3-9].*|[1-9][0-9]*], critic_missing="$critic_missing bison")

# Require GCC 12.1 to build.
AC_CACHE_CHECK([if $CC is sufficient to build libc], libc_cv_compiler_ok, [
AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[]], [[
#if (!defined __GNUC__ || __GNUC__ < 12 || (__GNUC__ == 12 && __GNUC_MINOR__ < 1)) && (!defined __clang__ || __clang_major__ < 18)
#error insufficient compiler
#endif]])],
	       [libc_cv_compiler_ok=yes],
	       [libc_cv_compiler_ok=no])])
AS_IF([test $libc_cv_compiler_ok != yes],
      [critic_missing="$critic_missing compiler"])

if test "x$maintainer" = "xyes"; then
  AC_CHECK_PROGS(AUTOCONF, autoconf, no)
  case "x$AUTOCONF" in
  xno|x|x:) AUTOCONF=no ;;
  *)
    AC_CACHE_CHECK(dnl
  whether $AUTOCONF${ACFLAGS:+ }$ACFLAGS works, libc_cv_autoconf_works, [dnl
    if (cd $srcdir; $AUTOCONF $ACFLAGS configure.ac > /dev/null 2>&1); then
      libc_cv_autoconf_works=yes
    else
      libc_cv_autoconf_works=no
    fi])
    test $libc_cv_autoconf_works = yes || AUTOCONF=no
    ;;
  esac
  if test "x$AUTOCONF" = xno; then
    aux_missing="$aux_missing autoconf"
  fi
else
  AUTOCONF=no
fi

# Check for python3 if available, or else python.
AC_CHECK_PROG_VER(PYTHON_PROG, python3 python, --version,
  [Python \([0-9][0-9.]*\)],
  [3.[4-9]*|3.[1-9][0-9]*|[4-9].*|[1-9][0-9]*],
  critic_missing="$critic_missing python")
PYTHON="$PYTHON_PROG -B"
AC_SUBST(PYTHON)

test -n "$critic_missing" && AC_MSG_ERROR([
*** These critical programs are missing or too old:$critic_missing
*** Check the INSTALL file for required versions.])

test -n "$aux_missing" && AC_MSG_WARN([
*** These auxiliary programs are missing or incompatible versions:$aux_missing
*** some features or tests will be disabled.
*** Check the INSTALL file for required versions.])


# Determine whether to use fpu or nofpu sysdeps directories.
AC_CACHE_CHECK([for use of fpu sysdeps directories],
	       libc_cv_with_fp, [dnl
cat > conftest.c <<EOF
#if $with_fp_cond
int dummy;
#else
# error "no hardware floating point"
#endif
EOF
libc_cv_with_fp=no
if ${CC-cc} $CFLAGS $CPPFLAGS -S conftest.c -o conftest.s \
   1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD ; then
  libc_cv_with_fp=yes
fi
rm -f conftest*])
AC_SUBST(libc_cv_with_fp)

conftest_code="
#ifndef __clang__
#error Not Clang!
#endif
"

dnl Check if clang is used to test glibc.
LIBC_TRY_TEST_CC_COMMAND([for clang],
  [$conftest_code],
  [-c],
  libc_cv_test_clang,
  [libc_cv_test_clang=yes], [libc_cv_test_clang=no],
)
LIBC_CONFIG_VAR([have-test-clang], [$libc_cv_test_clang])

dnl Check if clang++ is used to test glibc.
LIBC_TRY_TEST_CXX_COMMAND([for clang++],
  [$conftest_code],
  [-c],
  libc_cv_test_clangxx,
  [libc_cv_test_clangxx=yes], [libc_cv_test_clangxx=no],
)
LIBC_CONFIG_VAR([have-test-clangxx], [$libc_cv_test_clangxx])

if test "$libc_cv_test_clang" = "yes"; then
  conftest_code="
  #if __clang_major__ > 19
  #error clang version > 19
  #endif
  "
  LIBC_TRY_TEST_CC_COMMAND([for clang],
    [$conftest_code],
    [-c],
    libc_cv_test_clang_19_or_less,
    [libc_cv_test_clang_19_or_less=yes], [libc_cv_test_clang_19_or_less=no],
  )
fi
LIBC_CONFIG_VAR([have-test-clang-19-or-less], [$libc_cv_test_clang_19_or_less])

LIBC_TRY_CC_AND_TEST_CC_OPTION([for -fstack-protector],
  [-Werror -fstack-protector],
  libc_cv_ssp,
  [libc_cv_ssp=yes],
  [libc_cv_ssp=no],
  libc_cv_test_ssp,
  [libc_cv_test_ssp=yes],
  [libc_cv_test_ssp=no])

AC_CACHE_CHECK(for -fstack-protector-strong, libc_cv_ssp_strong, [dnl
LIBC_TRY_CC_OPTION([$CFLAGS $CPPFLAGS -Werror -fstack-protector-strong],
		   [libc_cv_ssp_strong=yes],
		   [libc_cv_ssp_strong=no])
])

AC_CACHE_CHECK(for -fstack-protector-all, libc_cv_ssp_all, [dnl
LIBC_TRY_CC_OPTION([$CFLAGS $CPPFLAGS -Werror -fstack-protector-all],
		   [libc_cv_ssp_all=yes],
		   [libc_cv_ssp_all=no])
])

stack_protector=
no_stack_protector=
if test "$libc_cv_ssp" = yes; then
  no_stack_protector="-fno-stack-protector -DSTACK_PROTECTOR_LEVEL=0"
  AC_DEFINE(HAVE_CC_NO_STACK_PROTECTOR)
fi
if test "$libc_cv_test_ssp" = yes; then
  AC_DEFINE(HAVE_TEST_CC_NO_STACK_PROTECTOR)
fi


if test "$enable_stack_protector" = yes && test "$libc_cv_ssp" = yes; then
  stack_protector="-fstack-protector"
  AC_DEFINE(STACK_PROTECTOR_LEVEL, 1)
elif test "$enable_stack_protector" = all && test "$libc_cv_ssp_all" = yes; then
  stack_protector="-fstack-protector-all"
  AC_DEFINE(STACK_PROTECTOR_LEVEL, 2)
elif test "$enable_stack_protector" = strong && test "$libc_cv_ssp_strong" = yes; then
  stack_protector="-fstack-protector-strong"
  AC_DEFINE(STACK_PROTECTOR_LEVEL, 3)
else
  stack_protector="-fno-stack-protector"
  AC_DEFINE(STACK_PROTECTOR_LEVEL, 0)
fi
AC_SUBST(libc_cv_ssp)
AC_SUBST(stack_protector)
AC_SUBST(no_stack_protector)

if test -n "$stack_protector"; then
  dnl Don't run configure tests with stack-protection on, to avoid problems with
  dnl bootstrapping.
  no_ssp=-fno-stack-protector
else
  no_ssp=

  if test "$enable_stack_protector" != no; then
    AC_MSG_ERROR([--enable-stack-protector=$enable_stack_protector specified, but specified level of stack protection is not supported by the compiler.])
  fi
fi

# For the multi-arch option we need support in the assembler & linker.
AC_CACHE_CHECK([for assembler and linker STT_GNU_IFUNC support],
	       libc_cv_ld_gnu_indirect_function, [dnl
cat > conftest.S <<EOF
.type foo,%gnu_indirect_function
foo:
.globl _start
_start:
.globl __start
__start:
.data
#ifdef _LP64
.quad foo
#else
.long foo
#endif
EOF
libc_cv_ld_gnu_indirect_function=no
if ${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS \
	    -nostartfiles -nostdlib $no_ssp \
	    -o conftest conftest.S 1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD; then
  # Do a link to see if the backend supports IFUNC relocs.
  $READELF -r conftest 1>&AS_MESSAGE_LOG_FD
  LC_ALL=C $READELF -Wr conftest | grep -q 'IRELATIVE\|R_SPARC_JMP_IREL' && {
    libc_cv_ld_gnu_indirect_function=yes
  }
fi
rm -f conftest*])

# Check if gcc supports attribute ifunc as it is used in libc_ifunc macro.
AC_CACHE_CHECK([for gcc attribute ifunc support],
	       libc_cv_gcc_indirect_function, [dnl
cat > conftest.c <<EOF
extern int func (int);
int used_func (int a)
{
  return a;
}
static void *resolver ()
{
  return &used_func;
}
extern __typeof (func) func __attribute__ ((ifunc ("resolver")));
EOF
libc_cv_gcc_indirect_function=no
if ${CC-cc} -c conftest.c -o conftest.o 1>&AS_MESSAGE_LOG_FD \
   2>&AS_MESSAGE_LOG_FD ; then
  if $READELF -s conftest.o | grep IFUNC >/dev/null 2>&AS_MESSAGE_LOG_FD; then
    libc_cv_gcc_indirect_function=yes
  fi
fi
rm -f conftest*])

# Check if linker supports textrel relocation with ifunc (used on elf/tests).
# Note that it relies on libc_cv_ld_gnu_indirect_function test above.
AC_CACHE_CHECK([whether the linker supports textrels along with ifunc],
               libc_cv_textrel_ifunc, [dnl
cat > conftest.S <<EOF
.type foo,%gnu_indirect_function
foo:
.globl _start
_start:
.globl __start
__start:
.data
#ifdef _LP64
.quad foo
#else
.long foo
#endif
.text
.globl address
address:
#ifdef _LP64
.quad address
#else
.long address
#endif
EOF
libc_cv_textrel_ifunc=no
if test $libc_cv_ld_gnu_indirect_function = yes; then
   if AC_TRY_COMMAND(${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -nostartfiles -nostdlib $no_ssp -pie -o conftest conftest.S); then
     libc_cv_textrel_ifunc=yes
   fi
fi
rm -f conftest*])
AC_SUBST(libc_cv_textrel_ifunc)

# Check if CC supports attribute retain as it is used in attribute_used_retain macro.
AC_CACHE_CHECK([for GNU attribute retain support],
	       libc_cv_gnu_retain, [dnl
cat > conftest.c <<EOF
static int var  __attribute__ ((used, retain, section ("__libc_atexit")));
EOF
libc_cv_gnu_retain=no
if ${CC-cc} -Werror -c conftest.c -o /dev/null 1>&AS_MESSAGE_LOG_FD \
   2>&AS_MESSAGE_LOG_FD ; then
  libc_cv_gnu_retain=yes
fi
rm -f conftest*])
if test $libc_cv_gnu_retain = yes; then
  AC_DEFINE(HAVE_GNU_RETAIN)
fi
LIBC_CONFIG_VAR([have-gnu-retain], [$libc_cv_gnu_retain])

# Check if gcc warns about alias for function with incompatible types.
AC_CACHE_CHECK([if compiler warns about alias for function with incompatible types],
	       libc_cv_gcc_incompatible_alias, [dnl
cat > conftest.c <<EOF
int __redirect_foo (const void *s, int c);

__typeof (__redirect_foo) *foo_impl (void) __asm__ ("foo");
__typeof (__redirect_foo) *foo_impl (void)
{
  return 0;
}

extern __typeof (__redirect_foo) foo_alias __attribute__ ((alias ("foo")));
EOF
libc_cv_gcc_incompatible_alias=yes
if ${CC-cc} -Werror -c conftest.c -o conftest.o 1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD ; then
  libc_cv_gcc_incompatible_alias=no
fi
rm -f conftest*])

if test x"$libc_cv_ld_gnu_indirect_function" != xyes; then
  if test x"$multi_arch" = xyes; then
    AC_MSG_ERROR([--enable-multi-arch support requires assembler and linker support])
  else
    multi_arch=no
  fi
fi
if test x"$libc_cv_gcc_indirect_function" != xyes; then
  # GCC 8+ emits a warning for alias with incompatible types and it might
  # fail to build ifunc resolvers aliases to either weak or internal
  # symbols.  Disables multiarch build in this case.
  if test x"$libc_cv_gcc_incompatible_alias" = xyes; then
    AC_MSG_WARN([gcc emits a warning for alias between functions of incompatible types])
    if test x"$multi_arch" = xyes; then
      AC_MSG_ERROR([--enable-multi-arch support requires a gcc with gnu-indirect-function support])
    fi
    AC_MSG_WARN([Multi-arch is disabled.])
    multi_arch=no
  elif test x"$multi_arch" = xyes; then
    AC_MSG_WARN([--enable-multi-arch support recommends a gcc with gnu-indirect-function support.
Please use a gcc which supports it by default or configure gcc with --enable-gnu-indirect-function])
  fi
fi
multi_arch_d=
if test x"$multi_arch" != xno; then
  multi_arch_d=/multiarch
fi

# Compute the list of sysdep directories for this configuration.
# This can take a while to compute.
sysdep_dir=$srcdir/sysdeps
AC_MSG_CHECKING(sysdep dirs)
dnl We need to use [ and ] for other purposes for a while now.
changequote(,)dnl
# Make sco3.2v4 become sco3.2.4 and sunos4.1.1_U1 become sunos4.1.1.U1.
os="`echo $os | sed 's/\([0-9A-Z]\)[v_]\([0-9A-Z]\)/\1.\2/g'`"

test "x$base_os" != x || case "$os" in
gnu*)
  base_os=mach/hurd ;;
linux*)
  base_os=unix/sysv ;;
esac

# For sunos4.1.1, try sunos4.1.1, then sunos4.1, then sunos4, then sunos.
tail=$os
ostry=$os
while o=`echo $tail | sed 's/\.[^.]*$//'`; test $o != $tail; do
  ostry="$ostry /$o"
  tail=$o
done
o=`echo $tail | sed 's/[0-9]*$//'`
if test $o != $tail; then
  ostry="$ostry /$o"
fi
# For linux-gnu, try linux-gnu, then linux.
o=`echo $tail | sed 's/-.*$//'`
if test $o != $tail; then
  ostry="$ostry /$o"
fi

# For unix/sysv/sysv4, try unix/sysv/sysv4, then unix/sysv, then unix.
base=
tail=$base_os
while b=`echo $tail | sed 's@^\(.*\)/\([^/]*\)$@& \1@'`; test -n "$b"; do
  set $b
  base="$base /$1"
  tail="$2"
done

# For sparc/sparc32, try sparc/sparc32 and then sparc.
mach=
tail=$machine${submachine:+/$submachine}
while m=`echo $tail | sed 's@^\(.*\)/\([^/]*\)$@& \1@'`; test -n "$m"; do
  set $m
  # Prepend the machine's FPU directory unless the architecture specific
  # preconfigure disables it.
  if test "$libc_cv_with_fp" = yes; then
    maybe_fpu=/fpu
  else
    maybe_fpu=/nofpu
  fi
  # For each machine term, try it with and then without /multiarch.
  for try_fpu in $maybe_fpu ''; do
    for try_multi in $multi_arch_d ''; do
      mach="$mach /$1$try_fpu$try_multi"
    done
  done
  tail="$2"
done

dnl We are done with glob and regexp uses of [ and ]; return to autoconf.
changequote([,])dnl

# Find what sysdep directories exist.
sysnames=
for b in $base ''; do
  for m0 in $mach ''; do
    for v in /$vendor ''; do
      test "$v" = / && continue
      for o in /$ostry ''; do
	test "$o" = / && continue
	for m in $mach ''; do
	  try_suffix="$m0$b$v$o$m"
	  if test -n "$try_suffix"; then
	    try_srcdir="${srcdir}/"
	    try="sysdeps$try_suffix"
	    test -n "$enable_debug_configure" &&
	    echo "$0 [DEBUG]: try $try" >&2
	    if test -d "$try_srcdir$try"; then
	      sysnames="$sysnames $try"
	      { test -n "$o" || test -n "$b"; } && os_used=t
	      { test -n "$m" || test -n "$m0"; } && machine_used=t
	      case x${m0:-$m} in
	      x*/$submachine) submachine_used=t ;;
	      esac
	    fi
	  fi
	done
      done
    done
  done
done

# If the assembler supports gnu_indirect_function symbol type and the
# architecture supports multi-arch, we enable multi-arch by default.
case $sysnames in
*"$multi_arch_d"*)
  ;;
*)
  test x"$multi_arch" = xdefault && multi_arch=no
  ;;
esac
if test x"$multi_arch" != xno; then
  AC_DEFINE(USE_MULTIARCH)
fi
AC_SUBST(multi_arch)

if test -z "$os_used" && test "$os" != none; then
  AC_MSG_ERROR(Operating system $os is not supported.)
fi
if test -z "$machine_used" && test "$machine" != none; then
  AC_MSG_ERROR(The $machine is not supported.)
fi
if test -z "$submachine_used" && test -n "$submachine"; then
  AC_MSG_ERROR(The $submachine subspecies of $host_cpu is not supported.)
fi
AC_SUBST(submachine)

# We have now validated the configuration.

# Expand the list of system names into a full list of directories
# from each element's parent name and Implies file (if present).
set $sysnames
names=
while test $# -gt 0; do
  name=$1
  shift

  case " $names " in *" $name "*)
    # Already in the list.
    continue
  esac

  # Report each name as we discover it, so there is no long pause in output.
  echo $ECHO_N "$name $ECHO_C" >&AS_MESSAGE_FD

  name_base=`echo $name | sed -e 's@\(.*sysdeps\)/.*@\1@'`

  case $name in
    /*) xsrcdir= ;;
    *)  xsrcdir=$srcdir/ ;;
  esac
  test -n "$enable_debug_configure" &&
  echo "[DEBUG]: name/Implies $xsrcdir$name/Implies" >&2

  for implies_file in Implies Implies-before Implies-after; do
    implies_type=`echo $implies_file | sed s/-/_/`
    eval ${implies_type}=
    if test -f $xsrcdir$name/$implies_file; then
      # Collect more names from the `Implies' file (removing comments).
      implied_candidate="`sed 's/#.*$//' < $xsrcdir$name/$implies_file`"
      for x in $implied_candidate; do
	found=no
	if test -d $xsrcdir$name_base/$x; then
	  eval "${implies_type}=\"\$${implies_type} \$name_base/\$x\""
	  found=yes
	fi
	try="sysdeps/$x"
	try_srcdir=$srcdir/
	test -n "$enable_debug_configure" &&
	 echo "[DEBUG]: $name $implies_file $x try() {$try_srcdir}$try" >&2
	if test $try != $xsrcdir$name_base/$x && test -d $try_srcdir$try;
	then
	  eval "${implies_type}=\"\$${implies_type} \$try\""
	  found=yes
	fi
	if test $found = no; then
	  AC_MSG_WARN($name/$implies_file specifies nonexistent $x)
	fi
      done
    fi
  done

  # Add NAME to the list of names.
  names="$names $name"

  # Find the parent of NAME, using the empty string if it has none.
changequote(,)dnl
  parent="`echo $name | sed -n -e 's=/[^/]*$==' -e '/sysdeps$/q' -e p`"
changequote([,])dnl

  test -n "$enable_debug_configure" &&
    echo "[DEBUG]: $name Implies='$Implies' rest='$*' parent='$parent' \
Implies_before='$Implies_before' Implies_after='$Implies_after'" >&2

  # Add the names implied by NAME, and NAME's parent (if it has one), to
  # the list of names to be processed (the argument list).  We prepend the
  # implied names to the list and append the parent.  We want implied
  # directories to come before further directories inferred from the
  # configuration components; this ensures that for sysv4, unix/common
  # (implied by unix/sysv/sysv4) comes before unix/sysv (in ostry (here $*)
  # after sysv4).
  sysnames="`echo $Implies $* $Implies_before $parent $Implies_after`"
  test -n "$sysnames" && set $sysnames
done

# Add the default directories.
default_sysnames="sysdeps/generic"
sysnames="$names $default_sysnames"
AC_SUBST(sysnames)
# The other names were emitted during the scan.
AC_MSG_RESULT($default_sysnames)

# if using special system headers, find out the compiler's sekrit
# header directory and add that to the list.  NOTE: Only does the right
# thing on a system that doesn't need fixincludes.  (Not presently a problem.)
if test -n "$sysheaders"; then
  SYSINCLUDES=-nostdinc
  for d in include include-fixed; do
    i=`$CC -print-file-name="$d"` && test "x$i" != x && test "x$i" != "x$d" &&
    SYSINCLUDES="$SYSINCLUDES -isystem $i"
  done
  SYSINCLUDES="$SYSINCLUDES \
-isystem `echo $sysheaders | sed 's/:/ -isystem /g'`"
  if test -n "$CXX"; then
    CXX_SYSINCLUDES=
    for cxxheaders in `$CXX -v -S -x c++ /dev/null -o /dev/null 2>&1 \
    | sed -n -e '1,/#include/d' -e 's/^ \(\/.*\/[cg]++\)/\1/p'`; do
      test "x$cxxheaders" != x &&
      CXX_SYSINCLUDES="$CXX_SYSINCLUDES -isystem $cxxheaders"
    done
  fi
fi
AC_SUBST(SYSINCLUDES)
AC_SUBST(CXX_SYSINCLUDES)

# Obtain some C++ header file paths.  This is used to make a local
# copy of those headers in Makerules.
if test -n "$TEST_CXX"; then
  saved_CXX="$CXX"
  CXX="$TEST_CXX"
  find_cxx_header () {
    echo "#include <$1>" | $CXX -H -fsyntax-only -x c++ - 2>&1 \
	 | $AWK '$1 == "."{print $2}'
  }
  CXX_CSTDLIB_HEADER="$(find_cxx_header cstdlib)"
  CXX_CMATH_HEADER="$(find_cxx_header cmath)"
  CXX_BITS_STD_ABS_H="$(find_cxx_header bits/std_abs.h)"
  CXX="$saved_CXX"
fi
AC_SUBST(CXX_CSTDLIB_HEADER)
AC_SUBST(CXX_CMATH_HEADER)
AC_SUBST(CXX_BITS_STD_ABS_H)

# Test if LD_LIBRARY_PATH contains the notation for the current directory
# since this would lead to problems installing/building glibc.
# LD_LIBRARY_PATH contains the current directory if one of the following
# is true:
# - one of the terminals (":" and ";") is the first or last sign
# - two terminals occur directly after each other
# - the path contains an element with a dot in it
AC_MSG_CHECKING(LD_LIBRARY_PATH variable)
changequote(,)dnl
case ${LD_LIBRARY_PATH} in
  [:\;]* | *[:\;] | *[:\;][:\;]* |  *[:\;]. | .[:\;]*| . | *[:\;].[:\;]* )
    ld_library_path_setting="contains current directory"
    ;;
  *)
    ld_library_path_setting="ok"
    ;;
esac
changequote([,])dnl
AC_MSG_RESULT($ld_library_path_setting)
if test "$ld_library_path_setting" != "ok"; then
AC_MSG_ERROR([
*** LD_LIBRARY_PATH shouldn't contain the current directory when
*** building glibc. Please change the environment variable
*** and run configure again.])
fi

AC_PATH_PROG(BASH_SHELL, bash, no)

AC_PATH_PROG(PERL, perl, no)
if test "$PERL" != no &&
   (eval `$PERL -V:apiversion`; test `expr "$apiversion" \< 5` -ne 0); then
  PERL=no
fi
AC_PATH_PROG(INSTALL_INFO, install-info, no,
	     $PATH:/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin)

AC_CACHE_CHECK(for .set assembler directive, libc_cv_asm_set_directive, [dnl
cat > conftest.s <<EOF
.text
foo:
.set glibc_conftest_frobozz,foo
.globl glibc_conftest_frobozz
EOF
# The alpha-dec-osf1 assembler gives only a warning for `.set'
# (but it doesn't work), so we must do a linking check to be sure.
cat > conftest1.c <<\EOF
extern int glibc_conftest_frobozz;
void _start() { glibc_conftest_frobozz = 1; }
EOF
if ${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS \
	    -nostartfiles -nostdlib $no_ssp \
	    -o conftest conftest.s conftest1.c 1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD; then
  libc_cv_asm_set_directive=yes
else
  libc_cv_asm_set_directive=no
fi
rm -f conftest*])
if test $libc_cv_asm_set_directive = yes; then
  AC_DEFINE(HAVE_ASM_SET_DIRECTIVE)
fi

AC_CACHE_CHECK(linker support for protected data symbol,
	       libc_cv_protected_data,
	       [cat > conftest.c <<EOF
		int bar __attribute__ ((visibility ("protected"))) = 1;
EOF
		libc_cv_protected_data=no
		if AC_TRY_COMMAND(${CC-cc} -nostdlib -nostartfiles $no_ssp -fPIC -shared conftest.c -o conftest.so); then
		  cat > conftest.c <<EOF
		  extern int bar;
		  int main (void) { return bar; }
EOF
		  if AC_TRY_COMMAND(${CC-cc} -nostdlib -nostartfiles $no_ssp conftest.c -o conftest conftest.so); then
		    libc_cv_protected_data=yes
		  fi
		fi
		rm -f conftest.*
	       ])
AC_SUBST(libc_cv_protected_data)

AC_CACHE_CHECK(linker support for INSERT in linker script,
	       libc_cv_insert,
	       [cat > conftest.c <<EOF
		int __attribute__ ((section(".bar"))) bar = 0x12345678;
		int test (void) { return bar; }
EOF
		cat > conftest.t <<EOF
		SECTIONS
		{
		  .bar : { *(.bar) }
		}
		INSERT AFTER .rela.dyn;
EOF
		libc_cv_insert=no
		if AC_TRY_COMMAND([${CC-cc} -nostdlib -nostartfiles $no_ssp -fPIC -shared conftest.c -Wl,-T,conftest.t -o conftest.so]); then
		  libc_cv_insert=yes
		fi
		rm -f conftest.*
	       ])
AC_SUBST(libc_cv_insert)

AC_CACHE_CHECK(for broken __attribute__((alias())),
	       libc_cv_broken_alias_attribute,
	       [cat > conftest.c <<EOF
	       extern int foo (int x) __asm ("xyzzy");
	       int bar (int x) { return x; }
	       extern __typeof (bar) foo __attribute ((weak, alias ("bar")));
	       extern int dfoo;
	       extern __typeof (dfoo) dfoo __asm ("abccb");
	       int dfoo = 1;
EOF
	       libc_cv_broken_alias_attribute=yes
	       if AC_TRY_COMMAND(${CC-cc} -Werror -S conftest.c -o conftest.s 1>&AS_MESSAGE_LOG_FD); then
		 if grep 'xyzzy' conftest.s >/dev/null &&
		    grep 'abccb' conftest.s >/dev/null; then
		   libc_cv_broken_alias_attribute=no
		 fi
	       fi
	       rm -f conftest.c conftest.s
	       ])
if test $libc_cv_broken_alias_attribute = yes; then
  AC_MSG_ERROR(working alias attribute support required)
fi

AC_CACHE_CHECK(whether to put _rtld_local into .sdata section,
	       libc_cv_have_sdata_section,
	       [echo "int i;" > conftest.c
		libc_cv_have_sdata_section=no
		if ${CC-cc} $LDFLAGS -fPIC -shared -Wl,--verbose conftest.c -o conftest.so 2>&1 \
		   | grep '\.sdata' >/dev/null; then
		  libc_cv_have_sdata_section=yes
		fi
		rm -f conftest.c conftest.so
	       ])
if test $libc_cv_have_sdata_section = yes; then
  AC_DEFINE(HAVE_SDATA_SECTION)
fi

AC_CACHE_CHECK(whether the compiler links with libunwind,
	       libc_cv_cc_with_libunwind, [dnl
cat > conftest.c <<EOF
int main (void) { return 0; }
EOF
  if ${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS -static -o conftest \
     conftest.c -v 2>&1 >/dev/null | grep 'libunwind.a' >/dev/null; then
    libc_cv_cc_with_libunwind=yes
  else
    libc_cv_cc_with_libunwind=no
  fi
  rm -f conftest*])
AC_SUBST(libc_cv_cc_with_libunwind)
if test $libc_cv_cc_with_libunwind = yes; then
  AC_DEFINE(HAVE_CC_WITH_LIBUNWIND)
fi
LIBC_CONFIG_VAR([have-cc-with-libunwind], [$libc_cv_cc_with_libunwind])

ASFLAGS_config=
AC_CACHE_CHECK(whether --noexecstack is desirable for .S files,
	       libc_cv_as_noexecstack, [dnl
cat > conftest.c <<EOF
void foo (void) { }
EOF
if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS
		   -S -o conftest.s conftest.c 1>&AS_MESSAGE_LOG_FD]) \
   && grep .note.GNU-stack conftest.s >/dev/null \
   && AC_TRY_COMMAND([${CC-cc} $ASFLAGS -Wa,--noexecstack
		      -c -o conftest.o conftest.s 1>&AS_MESSAGE_LOG_FD])
then
  libc_cv_as_noexecstack=yes
else
  libc_cv_as_noexecstack=no
fi
rm -f conftest*])
if test $libc_cv_as_noexecstack = yes; then
  ASFLAGS_config="$ASFLAGS_config -Wa,--noexecstack"
fi
AC_SUBST(ASFLAGS_config)

LIBC_TEST_LINKER_FEATURE([--no-error-execstack], [-Wl,--no-error-execstack],
		    [libc_cv_test_no_error_execstack=yes], [libc_cv_test_no_error_execstack=no])
AC_SUBST(libc_cv_test_no_error_execstack)

LIBC_LINKER_FEATURE([-z execstack], [-Wl,-z,execstack],
		    [libc_cv_z_execstack=yes], [libc_cv_z_execstack=no])
AC_SUBST(libc_cv_z_execstack)

LIBC_LINKER_FEATURE([-z start-stop-gc], [-Wl,-z,start-stop-gc],
		    [libc_cv_z_start_stop_gc=yes], [libc_cv_z_start_stop_gc=no])
LIBC_CONFIG_VAR([have-z-start-stop-gc], [$libc_cv_z_start_stop_gc])

LIBC_TEST_LINKER_FEATURE([--depaudit], [-Wl,--depaudit,x],
			 [libc_cv_test_depaudit=yes], [libc_cv_test_depaudit=no])
LIBC_CONFIG_VAR([have-depaudit], [$libc_cv_test_depaudit])

LIBC_LINKER_FEATURE([-z pack-relative-relocs],
		    [-Wl,-z,pack-relative-relocs],
		    [libc_cv_dt_relr=yes], [libc_cv_dt_relr=no])
LIBC_CONFIG_VAR([have-dt-relr], [$libc_cv_dt_relr])

LIBC_LINKER_FEATURE([--no-dynamic-linker],
		    [-Wl,--no-dynamic-linker],
		    [libc_cv_no_dynamic_linker=yes],
		    [libc_cv_no_dynamic_linker=no])
LIBC_CONFIG_VAR([have-no-dynamic-linker], [$libc_cv_no_dynamic_linker])

LIBC_TRY_CC_AND_TEST_CC_OPTION(for -static-pie, [-static-pie],
  libc_cv_static_pie,
  [libc_cv_static_pie=yes], [libc_cv_static_pie=no],
  libc_cv_test_static_pie,
  [libc_cv_test_static_pie=yes], [libc_cv_test_static_pie=no]
)
LIBC_CONFIG_VAR([have-static-pie], [$libc_cv_static_pie])
AC_SUBST(libc_cv_test_static_pie)

AC_CACHE_CHECK(for -fpie, libc_cv_fpie, [dnl
LIBC_TRY_CC_OPTION([-fpie], [libc_cv_fpie=yes], [libc_cv_fpie=no])
])

AC_SUBST(libc_cv_fpie)

AC_CACHE_CHECK(for GLOB_DAT reloc,
	       libc_cv_has_glob_dat, [dnl
cat > conftest.c <<EOF
extern int mumble;
int foo (void) { return mumble; }
EOF
if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS $LDFLAGS
			-fPIC -shared -o conftest.so conftest.c
			-nostdlib -nostartfiles $no_ssp
			1>&AS_MESSAGE_LOG_FD])
then
dnl look for GLOB_DAT relocation.
  if $READELF -rW conftest.so | grep '_GLOB_DAT' > /dev/null; then
    libc_cv_has_glob_dat=yes
  else
    libc_cv_has_glob_dat=no
  fi
else
  libc_cv_has_glob_dat=no
fi
rm -f conftest*])
AC_SUBST(libc_cv_has_glob_dat)

conftest_code="
__thread int i;
void foo (void)
{
  i = 10;
}
"
dnl Check if TEST_CC support tls descriptor.
LIBC_TRY_TEST_CC_COMMAND([for tls descriptor support],
  [$conftest_code],
  [-fPIC -mtls-dialect=$mtls_descriptor -nostdlib -nostartfiles -shared],
  libc_cv_test_mtls_descriptor,
  [libc_cv_test_mtls_descriptor=$mtls_descriptor],
  [libc_cv_test_mtls_descriptor=no])
LIBC_CONFIG_VAR([have-test-mtls-descriptor],
		[$libc_cv_test_mtls_descriptor])

dnl Check if TEST_CC support traditional tls.
LIBC_TRY_TEST_CC_COMMAND([for traditional tls support],
  [$conftest_code],
  [-fPIC -mtls-dialect=$mtls_traditional -nostdlib -nostartfiles -shared],
  libc_cv_test_mtls_traditional,
  [libc_cv_test_mtls_traditional=$mtls_traditional],
  [libc_cv_test_mtls_traditional=no])
LIBC_CONFIG_VAR([have-test-mtls-traditional],
		[$libc_cv_test_mtls_traditional])

AC_CACHE_CHECK([for -Wno-maybe-uninitialized],
                libc_cv_wno_maybe_uninitialized, [dnl
		LIBC_TRY_CC_OPTION([-Werror -Wno-maybe-uninitialized],
				   [libc_cv_wno_maybe_uninitialized=-Wno-maybe-uninitialized],
				   [libc_cv_wno_maybe_uninitialized=])
])
LIBC_CONFIG_VAR([config-cflags-wno-maybe-uninitialized],
		[$libc_cv_wno_maybe_uninitialized])

AC_CACHE_CHECK(whether cc puts quotes around section names,
	       libc_cv_have_section_quotes,
	       [cat > conftest.c <<EOF
		static const int foo
		__attribute__ ((section ("bar"))) = 1;
EOF
		if ${CC-cc} -S conftest.c -o conftest.s; then
		  if grep '\.section.*"bar"' conftest.s >/dev/null; then
		    libc_cv_have_section_quotes=yes
		  else
		    libc_cv_have_section_quotes=no
		  fi
		else
		  libc_cv_have_section_quotes=unknown
		fi
		rm -f conftest.{c,s}
		])
if test $libc_cv_have_section_quotes = yes; then
  AC_DEFINE(HAVE_SECTION_QUOTES)
fi

AC_CACHE_CHECK(for __builtin_memset, libc_cv_gcc_builtin_memset, [dnl
cat > conftest.c <<\EOF
void zero (void *x)
{
  __builtin_memset (x, 0, 1000);
}
EOF
dnl
if AC_TRY_COMMAND([${CC-cc} -O3 -S conftest.c -o - | grep -F "memset" > /dev/null]);
then
  libc_cv_gcc_builtin_memset=no
else
  libc_cv_gcc_builtin_memset=yes
fi
rm -f conftest* ])
if test "$libc_cv_gcc_builtin_memset" = yes ; then
  AC_DEFINE(HAVE_BUILTIN_MEMSET)
fi

AC_CACHE_CHECK(for 64-bit atomic support, libc_cv_gcc_has_64b_atomics, [dnl
cat > conftest.c <<\EOF
typedef struct { long long t; } X;
extern void has_64b_atomics(void);
void f(void)
{
  X x;
  /* Use address of structure with 64-bit type.  This avoids incorrect
     implementations which return true even if long long is not 64-bit aligned.
     This works on GCC and LLVM - other cases have bugs and they disagree.  */
  _Static_assert (__atomic_always_lock_free (sizeof (x), &x), "no_64b_atomics");
}
EOF
dnl
if AC_TRY_COMMAND([${CC-cc} -O2 -S conftest.c]);
then
  libc_cv_gcc_has_64b_atomics=yes
else
  libc_cv_gcc_has_64b_atomics=no
fi
rm -f conftest* ])
if test "$libc_cv_gcc_has_64b_atomics" = yes; then
  AC_DEFINE(HAVE_64B_ATOMICS, 1)
else
  AC_DEFINE(HAVE_64B_ATOMICS, 0)
fi

dnl Determine how to disable generation of FMA instructions.
AC_CACHE_CHECK([for compiler option to disable generation of FMA instructions],
	       libc_cv_cc_nofma, [dnl
libc_cv_cc_nofma=
for opt in -ffp-contract=off -mno-fused-madd; do
  LIBC_TRY_CC_OPTION([$opt], [libc_cv_cc_nofma=$opt; break])
done])
AC_SUBST(libc_cv_cc_nofma)

if test -n "$submachine"; then
  AC_CACHE_CHECK([for compiler option for CPU variant],
		 libc_cv_cc_submachine, [dnl
  libc_cv_cc_submachine=no
  for opt in "-march=$submachine" "-mcpu=$submachine"; do
    LIBC_TRY_CC_OPTION([$opt], [
      libc_cv_cc_submachine="$opt"
      break], [])
  done])
  if test "x$libc_cv_cc_submachine" = xno; then
    AC_MSG_ERROR([${CC-cc} does not support $submachine])
  fi
fi
AC_SUBST(libc_cv_cc_submachine)

dnl Determine if compiler supports -fsignaling-nans
LIBC_TRY_CC_AND_TEST_CC_OPTION([for compiler option that -fsignaling-nans],
  [-Werror -fsignaling-nans],
  libc_cv_cc_signaling_nans,
  [libc_cv_cc_signaling_nans=-fsignaling-nans],
  [libc_cv_cc_signaling_nans=],
  libc_cv_test_cc_signaling_nans,
  [libc_cv_test_cc_signaling_nans=-fsignaling-nans],
  [libc_cv_test_cc_signaling_nans=])
LIBC_CONFIG_VAR([config-cflags-signaling-nans],
		[$libc_cv_cc_signaling_nans])
AC_SUBST(libc_cv_test_cc_signaling_nans)

dnl Check if TEST_CC supports -fsemantic-interposition.
LIBC_TRY_TEST_CC_OPTION([-fsemantic-interposition],
  [-Werror -fsemantic-interposition],
  libc_cv_test_cc_cflags_fsemantic_interposition,
  [libc_cv_test_cc_cflags_fsemantic_interposition=yes],
  [libc_cv_test_cc_cflags_fsemantic_interposition=no]
)
LIBC_CONFIG_VAR(have-test-cc-cflags-fsemantic-interposition,
		$libc_cv_test_cc_cflags_fsemantic_interposition)

dnl Determine if TEST_CC supports -ffloat-store.
LIBC_TRY_TEST_CC_OPTION([for -ffloat-store],
  [-Werror -ffloat-store],
  libc_cv_test_cc_float_store,
  [libc_cv_test_cc_float_store="-ffloat-store"],
  [libc_cv_test_cc_float_store=])
LIBC_CONFIG_VAR([test-config-cflags-float-store],
		[$libc_cv_test_cc_float_store])

conftest_code="
void
__attribute__ ((__optimize__ (\"-fno-tree-loop-distribute-patterns\")))
foo (void) {}
"
LIBC_TRY_CC_AND_TEST_CC_COMMAND([if __attribute__ ((__optimize__("-fno-tree-loop-distribute-patterns"))) works],
  [$conftest_code],
  [-c -Werror],
  libc_cv_cc_loop_to_function,
  [libc_cv_cc_loop_to_function=yes],
  [libc_cv_cc_loop_to_function=no],
  libc_cv_test_cc_loop_to_function,
  [libc_cv_test_cc_loop_to_function=yes],
  [libc_cv_test_cc_loop_to_function=no])
if test $libc_cv_cc_loop_to_function = yes; then
  AC_DEFINE(HAVE_CC_INHIBIT_LOOP_TO_LIBCALL)
fi
if test $libc_cv_test_cc_loop_to_function = yes; then
  AC_DEFINE(HAVE_TEST_CC_INHIBIT_LOOP_TO_LIBCALL)
fi
AC_SUBST(libc_cv_cc_loop_to_function)
LIBC_CONFIG_VAR(config-cflags-cc-loop-to-function,$libc_cv_cc_loop_to_function)

LIBC_TRY_CC_AND_TEST_CC_OPTION([for -Wimplicit-fallthrough],
  [-Werror -Wimplicit-fallthrough],
  libc_cv_cc_wimplicit_fallthrough,
  [libc_cv_cc_wimplicit_fallthrough=-Wimplicit-fallthrough],
  [libc_cv_cc_wimplicit_fallthrough=],
  libc_cv_test_cc_wimplicit_fallthrough,
  [libc_cv_test_cc_wimplicit_fallthrough=-Wimplicit-fallthrough],
  [libc_cv_test_cc_wimplicit_fallthrough=])
dnl Clang's -Wimplicit-fallthrough warning, which flags unannotated
dnl fall-through in switch statements, does not recognize specific
dnl comments like /* FALLTHROUGH */ for suppressing the warning, unlike
dnl GCC.  Since fall through comments are used extensively in glibc,
dnl disable -Wimplicit-fallthrough when clang is in use.
if test $libc_cv_test_clang = yes; then
  libc_cv_test_cc_wimplicit_fallthrough=
fi
LIBC_CONFIG_VAR([cc-option-wimplicit-fallthrough],
		[$libc_cv_cc_wimplicit_fallthrough])
AC_SUBST(libc_cv_test_cc_wimplicit_fallthrough)

LIBC_TRY_CC_AND_TEST_CC_OPTION([for -Wfree-labels],
  [-Werror -Wfree-labels],
  libc_cv_cc_wfree_labels,
  [libc_cv_cc_wfree_labels=-Wfree-labels],
  [libc_cv_cc_wfree_labels=],
  libc_cv_test_cc_wfree_labels,
  [libc_cv_test_cc_wfree_labels=-Wfree-labels],
  [libc_cv_test_cc_wfree_labels=])
LIBC_CONFIG_VAR([cc-option-wfree-labels], [$libc_cv_cc_wfree_labels])
AC_SUBST(libc_cv_test_cc_wfree_labels)

LIBC_TRY_CC_AND_TEST_CC_OPTION([for -Wmissing-parameter-name],
  [-Werror -Wmissing-parameter-name],
  libc_cv_cc_wmissing_parameter_name,
  [libc_cv_cc_wmissing_parameter_name=-Wmissing-parameter-name],
  [libc_cv_cc_wmissing_parameter_name=],
  libc_cv_test_cc_wmissing_parameter_name,
  [libc_cv_test_cc_wmissing_parameter_name=-Wmissing-parameter-name],
  [libc_cv_test_cc_wmissing_parameter_name=])
LIBC_CONFIG_VAR([cc-option-wmissing-parameter-name],
		[$libc_cv_cc_wmissing_parameter_name])
AC_SUBST(libc_cv_test_cc_wmissing_parameter_name)

conftest_code="
void bar (void (*callback) (void));
int foo (void)
{
  int var = 0;
  void callback (void) { var = 1; }
  bar (callback);
  return var;
}
"

dnl Check if TEST_CC support trampolines.
LIBC_TRY_TEST_CC_COMMAND([support for trampolines],
  [$conftest_code],
  [-c -Werror],
  libc_cv_test_cc_trampolines,
  [libc_cv_test_cc_trampolines=yes],
  [libc_cv_test_cc_trampolines=no]
)
LIBC_CONFIG_VAR([have-test-cc-trampoline],
		[$libc_cv_test_cc_trampolines])

dnl Check if TEST_CC supports -Wno-restrict.
LIBC_TRY_TEST_CC_OPTION([-Wno-restrict],
  [-Werror -Wno-restrict],
  libc_cv_test_cflags_wno_restrict,
  [libc_cv_test_cflags_wno_restrict=-Wno-restrict],
  [libc_cv_test_cflags_wno_restrict=]
)
LIBC_CONFIG_VAR([test-config-cflags-wno-restrict],
		[$libc_cv_test_cflags_wno_restrict])

dnl Check if TEST_CC supports -Wno-fortify-source.
LIBC_TRY_TEST_CC_OPTION([-Wno-fortify-source],
  [-Werror -Wno-fortify-source],
  libc_cv_test_cflags_wno_fortify_source,
  [libc_cv_test_cflags_wno_fortify_source=-Wno-fortify-source],
  [libc_cv_test_cflags_wno_fortify_source=]
)
LIBC_CONFIG_VAR([test-config-cflags-wno-fortify-source],
		[$libc_cv_test_cflags_wno_fortify_source])

dnl Check if TEST_CC supports -finput-charset=ascii.
LIBC_TRY_TEST_CC_OPTION([-finput-charset=ascii],
  [-Werror -finput-charset=ascii],
  libc_cv_test_cflags_finput_charset_ascii,
  [libc_cv_test_cflags_finput_charset_ascii="-finput-charset=ascii"],
  [libc_cv_test_cflags_finput_charset_ascii=]
)
LIBC_CONFIG_VAR(test-config-cflags-finput-charset-ascii,
		$libc_cv_test_cflags_finput_charset_ascii)

dnl Check if TEST_CXX supports -finput-charset=ascii.
LIBC_TRY_TEST_CXX_OPTION([$CXX -finput-charset=ascii],
  [-Werror -finput-charset=ascii],
  libc_cv_test_cxxflags_finput_charset_ascii,
  [libc_cv_test_cxxflags_finput_charset_ascii="-finput-charset=ascii"],
  [libc_cv_test_cxxflags_finput_charset_ascii=]
)
LIBC_CONFIG_VAR(test-config-cxxflags-finput-charset-ascii,
		$libc_cv_test_cxxflags_finput_charset_ascii)

dnl Check if TEST_CXX supports -std=c++26.
LIBC_TRY_TEST_CXX_OPTION([$CXX -std=c++26],
  [-Werror -std=c++26],
  libc_cv_test_cxxflags_stdcxx26,
  [libc_cv_test_cxxflags_stdcxx26="-std=c++26"],
  [libc_cv_test_cxxflags_stdcxx26=]
)
LIBC_CONFIG_VAR(test-config-cxxflags-stdcxx26,
		$libc_cv_test_cxxflags_stdcxx26)

conftest_code="
extern int not_exist (void);

inline int make_unique (void)
{
  /* Static variables in inline functions and classes
     generate STB_GNU_UNIQUE symbols.  */
  static int unique;
  return ++unique;
}

int foo (void)
{
  return make_unique () + not_exist ();
}
"

dnl Check if TEST_CXX supports STB_GNU_UNIQUE.
LIBC_TRY_TEST_CXX_COMMAND([for STB_GNU_UNIQUE],
  [$conftest_code],
  [-c],
  libc_cv_test_stb_gnu_unique,
  [
if LC_ALL=C $READELF -sW conftest \
   | grep -q "UNIQUE.* _ZZ11make_uniquevE6unique"; then
  libc_cv_test_stb_gnu_unique=yes
else
  libc_cv_test_stb_gnu_unique=no
fi
  ],
  [libc_cv_test_stb_gnu_unique=no],
)
LIBC_CONFIG_VAR([have-test-stb-gnu-unique], [$libc_cv_test_stb_gnu_unique])

dnl Check whether we have the gd library available.
AC_MSG_CHECKING(for libgd)
if test "$with_gd" != "no"; then
  old_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $libgd_include"
  old_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $libgd_ldflags"
  old_LIBS="$LIBS"
  LIBS="$LIBS -lgd -lpng -lz -lm"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <gd.h>]], [[gdImagePng (0, 0)]])],
		 [LIBGD=yes], [LIBGD=no])
  CFLAGS="$old_CFLAGS"
  LDFLAGS="$old_LDFLAGS"
  LIBS="$old_LIBS"
else
  LIBGD=no
fi
AC_MSG_RESULT($LIBGD)
AC_SUBST(LIBGD)

# SELinux detection
if test x$with_selinux = xno ; then
  have_selinux=no;
else
  # See if we have the SELinux library
  AC_CHECK_LIB(selinux, is_selinux_enabled,
	       have_selinux=yes, have_selinux=no)
  if test x$with_selinux = xyes ; then
    if test x$have_selinux = xno ; then
      AC_MSG_ERROR([SELinux explicitly required, but SELinux library not found])
    fi
  fi
fi
# Check if we're building with SELinux support.
if test "x$have_selinux" = xyes; then
  AC_DEFINE(HAVE_SELINUX, 1, [SELinux support])

  # See if we have the libaudit library
  AC_CHECK_LIB(audit, audit_log_user_avc_message,
	       have_libaudit=yes, have_libaudit=no)
  if test "x$have_libaudit" = xyes; then
    AC_DEFINE(HAVE_LIBAUDIT, 1, [SELinux libaudit support])
  fi
  AC_SUBST(have_libaudit)

  # See if we have the libcap library
  AC_CHECK_LIB(cap, cap_init, have_libcap=yes, have_libcap=no)
  if test "x$have_libcap" = xyes; then
    AC_DEFINE(HAVE_LIBCAP, 1, [SELinux libcap support])
  fi
  AC_SUBST(have_libcap)
fi
AC_SUBST(have_selinux)

dnl Check if we support the requested _FORTIFY_SOURCE level
dnl If not, then don't use it.
dnl Note that _FORTIFY_SOURCE may have been set through FLAGS too.
dnl _FORTIFY_SOURCE value will be selectively disabled for function that can't
dnl support it
no_fortify_source="-U_FORTIFY_SOURCE"
fortify_source="${no_fortify_source}"

LIBC_TRY_CC_AND_TEST_LINK([for maximum supported _FORTIFY_SOURCE level],
  [__builtin_dynamic_object_size("", 0)],
  libc_cv_supported_fortify_source,
  [libc_cv_supported_fortify_source=3],
  [libc_cv_supported_fortify_source=2],
  libc_cv_test_supported_fortify_source,
  [libc_cv_test_supported_fortify_source=3],
  [libc_cv_test_supported_fortify_source=2])
AC_SUBST(libc_cv_test_supported_fortify_source)

AS_CASE([$enable_fortify_source],
        [yes], [libc_cv_fortify_source=yes enable_fortify_source=$libc_cv_supported_fortify_source],
        [1|2], [libc_cv_fortify_source=yes],
        [3], [AS_IF([test $libc_cv_supported_fortify_source = 3],
                    [libc_cv_fortify_source=yes],
                    [AC_MSG_ERROR([Compiler doesn't provide necessary support for _FORTIFY_SOURCE=3])])],
        [libc_cv_fortify_source=no])

AS_IF([test "$libc_cv_fortify_source" = yes],
      [fortify_source="${fortify_source} -D_FORTIFY_SOURCE=${enable_fortify_source}"]
      )

AC_SUBST(enable_fortify_source)
AC_SUBST(libc_cv_fortify_source)
AC_SUBST(no_fortify_source)
AC_SUBST(fortify_source)
LIBC_CONFIG_VAR([supported-fortify], [$libc_cv_supported_fortify_source])

dnl Check if compiler define _FILE_OFFSET_BITS by default
AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[]], [[
#ifndef _FILE_OFFSET_BITS
# error _FILE_OFFSET_BITS not defined
#endif]])],
	       [libc_cv_compiler_default_file_offset_bits=yes],
	       [libc_cv_compiler_default_file_offset_bits=no])
AS_IF([test "$libc_cv_compiler_default_file_offset_bits" = yes],
      [no_file_offset_bits_source="-U_FILE_OFFSET_BITS"])
AC_SUBST(no_file_offset_bits_source)
LIBC_CONFIG_VAR([no-file-offset-bits-source], [$no_file_offset_bits_source])

dnl Check if compiler define _TIME_BITS by default
AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[]], [[
#ifndef _TIME_BITS
# error _TIME_BITS not defined
#endif]])],
	       [libc_cv_compiler_default_time_bits=yes],
	       [libc_cv_compiler_default_time_bits=no])
AS_IF([test "$libc_cv_compiler_default_time_bits" = yes],
      [no_time_bits_source="-U_TIME_BITS"])
AC_SUBST(no_time_bits_source)
LIBC_CONFIG_VAR([no-time-bits-source], [$no_time_bits_source])

dnl Starting with binutils 2.35, GAS can attach multiple symbol versions
dnl to one symbol (PR 23840).
AC_CACHE_CHECK(whether the assembler requires one version per symbol,
               libc_cv_symver_needs_alias, [dnl
  cat > conftest.s <<EOF
        .text
testfunc:
        .globl testfunc
        .symver testfunc, testfunc1@VERSION1
        .symver testfunc, testfunc1@VERSION2
EOF
  libc_cv_symver_needs_alias=no
  if ${CC-cc} $ASFLAGS -c conftest.s 2>&AS_MESSAGE_LOG_FD; then
    libc_cv_symver_needs_alias=no
  else
    libc_cv_symver_needs_alias=yes
  fi
  rm conftest.*
])
if test "$libc_cv_symver_needs_alias" = yes; then
  AC_DEFINE(SYMVER_NEEDS_ALIAS)
fi

AC_CACHE_CHECK(for __builtin_trap with no external dependencies,
	       libc_cv_builtin_trap, [dnl
libc_cv_builtin_trap=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[__builtin_trap ()]])],[
libc_undefs=`$NM -u conftest.o |
  LC_ALL=C $AWK '$1 == "U" { print $2 | "sort -u"; next } { exit(1) }' \
    2>&AS_MESSAGE_LOG_FD` || {
  AC_MSG_ERROR([confusing output from $NM -u])
}
echo >&AS_MESSAGE_LOG_FD "libc_undefs='$libc_undefs'"
if test -z "$libc_undefs"; then
  libc_cv_builtin_trap=yes
fi],[])])
if test $libc_cv_builtin_trap = yes; then
  AC_DEFINE([HAVE_BUILTIN_TRAP])
fi

dnl Check if
AC_CACHE_CHECK([whether the compiler supports __attribute__ ((aligned (65536)))],
	       libc_cv_aligned_65536, [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
char bss[0xb5dce8] __attribute__ ((aligned (65536)));
])],
	       [libc_cv_aligned_65536=yes],
	       [libc_cv_aligned_65536=no])
])
LIBC_CONFIG_VAR([aligned-65536], [$libc_cv_aligned_65536])

dnl C++ feature tests.
AC_LANG_PUSH([C++])

AC_CACHE_CHECK([whether the C++ compiler supports thread_local],
	       libc_cv_cxx_thread_local, [
old_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -std=gnu++11"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <thread>

// Compiler support.
struct S
{
  S ();
  ~S ();
};
thread_local S s;
S * get () { return &s; }

// libstdc++ support.
#ifndef _GLIBCXX_HAVE___CXA_THREAD_ATEXIT_IMPL
#error __cxa_thread_atexit_impl not supported
#endif
])],
	       [libc_cv_cxx_thread_local=yes],
	       [libc_cv_cxx_thread_local=no])
CXXFLAGS="$old_CXXFLAGS"
])
AC_SUBST(libc_cv_cxx_thread_local)

AC_LANG_POP([C++])
dnl End of C++ feature tests.

### End of automated tests.
### Now run sysdeps configure fragments.

# They also can set these variables.
use_ldconfig=no
ldd_rewrite_script=no
libc_cv_sysconfdir=$sysconfdir
libc_cv_localstatedir=$localstatedir
libc_cv_gcc_unwind_find_fde=no
libc_cv_idn=no
abort_in_ld=no

# Iterate over all the sysdep directories we will use, running their
# configure fragments.
for dir in $sysnames; do
  case $dir in
    /*) dest=$dir ;;
    *)  dest=$srcdir/$dir ;;
  esac
  if test -r $dest/configure; then
    AC_MSG_RESULT(running configure fragment for $dir)
    . $dest/configure
  fi
done

if test x"$build_mathvec" = xnotset; then
  build_mathvec=no
fi
LIBC_CONFIG_VAR([build-mathvec], [$build_mathvec])

AC_SUBST(libc_extra_cflags)
AC_SUBST(libc_extra_cppflags)

if test x$libc_cv_gcc_unwind_find_fde = xyes; then
  AC_DEFINE(EXPORT_UNWIND_FIND_FDE)
fi
AC_SUBST(libc_cv_gcc_unwind_find_fde)

# A sysdeps configure fragment can reset this if IFUNC is not actually
# usable even though the assembler knows how to generate the symbol type.
if test x"$libc_cv_ld_gnu_indirect_function" = xyes; then
  AC_DEFINE(HAVE_IFUNC)
fi
LIBC_CONFIG_VAR([have-ifunc], [$libc_cv_ld_gnu_indirect_function])

if test x"$libc_cv_gcc_indirect_function" = xyes; then
  AC_DEFINE(HAVE_GCC_IFUNC)
fi
LIBC_CONFIG_VAR([have-gcc-ifunc], [$libc_cv_gcc_indirect_function])

# This is far from the AC_ARG_ENABLE that sets it so that a sysdeps
# configure fragment can override the value to prevent this AC_DEFINE.
AC_SUBST(use_nscd)
if test "x$use_nscd" != xno; then
  AC_DEFINE([USE_NSCD])
fi
if test "x$build_nscd" = xdefault; then
  build_nscd=$use_nscd
fi

AC_SUBST(libc_cv_slibdir)
AC_SUBST(libc_cv_rtlddir)
AC_SUBST(libc_cv_complocaledir)
AC_SUBST(libc_cv_sysconfdir)
AC_SUBST(libc_cv_localstatedir)
AC_SUBST(libc_cv_rootsbindir)

if test x$use_ldconfig = xyes; then
  AC_DEFINE(USE_LDCONFIG)
fi
AC_SUBST(use_ldconfig)
AC_SUBST(ldd_rewrite_script)

AC_SUBST(static)
AC_SUBST(shared)

AC_CACHE_CHECK([whether -fPIC is default], libc_cv_pic_default,
[libc_cv_pic_default=yes
cat > conftest.c <<EOF
#if defined __PIC__ || defined __pic__ || defined PIC || defined pic
# error PIC is default.
#endif
EOF
if eval "${CC-cc} -S conftest.c 2>&AS_MESSAGE_LOG_FD 1>&AS_MESSAGE_LOG_FD"; then
  libc_cv_pic_default=no
fi
rm -f conftest.*])
LIBC_CONFIG_VAR([build-pic-default], [$libc_cv_pic_default])

AC_CACHE_CHECK([whether -fPIE is default], libc_cv_cc_pie_default,
[libc_cv_cc_pie_default=yes
cat > conftest.c <<EOF
#if defined __PIE__ || defined __pie__ || defined PIE || defined pie
# error PIE is default.
#endif
EOF
if eval "${CC-cc} -S conftest.c 2>&AS_MESSAGE_LOG_FD 1>&AS_MESSAGE_LOG_FD"; then
  libc_cv_cc_pie_default=no
fi
rm -f conftest.*])
LIBC_CONFIG_VAR([cc-pie-default], [$libc_cv_cc_pie_default])

# Get Position Dependent Executable (PDE) load address to be used to
# load static Position Independent Executable (PIE) at a known working
# non-zero load address.  This is only used by glibc tests to verify
# that PIE and static PIE with non-zero load address work correctly.
AC_CACHE_CHECK([PDE load address],
	       libc_cv_pde_load_address, [dnl
cat > conftest.S <<EOF
.globl _start
_start:
.globl __start
__start:
	.byte 0
EOF
if test $libc_cv_cc_pie_default = yes; then
  pde_ld_flags="-no-pie"
fi
if ${CC-cc} $pde_ld_flags $CFLAGS $CPPFLAGS $LDFLAGS \
	    -nostartfiles -nostdlib $no_ssp \
	    -o conftest conftest.S 1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD; then
  # Get the load address of the first PT_LOAD segment.
  libc_cv_pde_load_address=$(LC_ALL=C $READELF -Wl conftest \
			     | $AWK '/LOAD/ { print $3; exit 0; }')
else
  AC_MSG_ERROR([${CC-cc} can not create PDE])
fi
rm -f conftest*])
LIBC_CONFIG_VAR([pde-load-address], [$libc_cv_pde_load_address])

# Get the linker command-line option to load executable at a non-zero
# load address.  This is only used by glibc tests to verify that PIE and
# static PIE with non-zero load address work correctly.
LIBC_LINKER_FEATURE([-Ttext-segment=$libc_cv_pde_load_address],
		    [-Wl,-Ttext-segment=$libc_cv_pde_load_address],
		    [libc_cv_load_address_ldflag=-Wl,-Ttext-segment],
		    [libc_cv_load_address_ldflag=])
LIBC_CONFIG_VAR([load-address-ldflag], [$libc_cv_load_address_ldflag])

# Check if compiler supports -fno-fp-int-builtin-inexact
LIBC_TRY_CC_AND_TEST_CC_OPTION([if compiler supports -fno-fp-int-builtin-inexact],
  [-Werror -fno-fp-int-builtin-inexact],
  libc_cv_cc_no_fp_builtin_inexact,
  [libc_cv_cc_no_fp_builtin_inexact=-fno-fp-int-builtin-inexact],
  [libc_cv_cc_no_fp_builtin_inexact=],
  libc_cv_test_cc_no_fp_builtin_inexact,
  [libc_cv_test_cc_no_fp_builtin_inexact=-fno-fp-int-builtin-inexact],
  [libc_cv_test_cc_no_fp_builtin_inexact=])
LIBC_CONFIG_VAR([config-cflags-no-fp-builtin-inexact], [$libc_cv_cc_no_fp_builtin_inexact])


LIBC_LINKER_FEATURE([--undefined-version], [-Wl,--undefined-version],
		    [libc_cv_ld_undefined_version=-Wl,--undefined-version],
		    [libc_cv_ld_undefined_version=])
LIBC_CONFIG_VAR([have-undefined-version], [$libc_cv_ld_undefined_version])

AC_MSG_CHECKING(if we can build programs as PIE)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#ifdef PIE_UNSUPPORTED
# error PIE is not supported
#endif]])], [libc_cv_pie_supported=yes], [libc_cv_pie_supported=no])
AC_MSG_RESULT($libc_cv_pie_supported)
# Disable build-pie-default if target does not support it or glibc is
# configured with --disable-default-pie.
if test "x$default_pie" = xno; then
  build_pie_default=no
else
  build_pie_default=$libc_cv_pie_supported
fi
LIBC_CONFIG_VAR([build-pie-default], [$build_pie_default])

AC_MSG_CHECKING(if we can build static PIE programs)
libc_cv_static_pie_supported=$libc_cv_pie_supported
if test "x$libc_cv_pie_supported" != xno \
   -a "$libc_cv_no_dynamic_linker" = yes; then
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([[#ifndef SUPPORT_STATIC_PIE
# error static PIE is not supported
#endif]])], [libc_cv_static_pie_supported=yes],
	    [libc_cv_static_pie_supported=no])
fi
AC_MSG_RESULT($libc_cv_static_pie_supported)

# Enable static-pie only if it is available and glibc isn't configured
# with --disable-default-pie.
if test "x$default_pie" = xno; then
  libc_cv_static_pie=no
else
  libc_cv_static_pie=$libc_cv_static_pie_supported
fi
if test "$libc_cv_static_pie" = "yes"; then
  AC_DEFINE(ENABLE_STATIC_PIE)
fi
LIBC_CONFIG_VAR([enable-static-pie], [$libc_cv_static_pie])

# Check if we can link support functionality against libgcc_s.
# Must not be used for linking installed binaries, to produce the
# same binaries for bootstrapped and bootstrapping builds (the latter
# with a GCC that does not build libgcc_s).
AC_CACHE_CHECK([whether $CC can link against -lgcc_s], libc_cv_have_libgcc_s, [dnl
  old_LIBS="$LIBS"
  LIBS="$LIBS -lgcc_s"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		 [libc_cv_have_libgcc_s=yes], [libc_cv_have_libgcc_s=no])
  LIBS="$old_LIBS"])
LIBC_CONFIG_VAR([have-libgcc_s], [$libc_cv_have_libgcc_s])

# Check which runtime library compiler uses
AC_CACHE_CHECK([for the usable compiler runtime library], [libc_cv_runtime_library], [dnl
  libc_cv_libgcc_usable=no
  libgcc_name=`${CC-cc} $CFLAGS $CPPFLAGS -print-libgcc-file-name | sed 's:.*/::'`
  case "$libgcc_name" in
    libgcc*)
      libc_cv_libgcc=-lgcc
      libc_cv_libgcc_eh=-lgcc_eh
      libc_cv_runtime_library=libgcc
      ;;
    libclang_rt*)
      libc_cv_libgcc=-lclang_rt.builtins
      libc_cv_libgcc_eh=
      libc_cv_runtime_library=compiler-rt
      ;;
    *) AC_MSG_FAILURE([non supported compiler runtime library]) ;;
  esac])
LIBC_CONFIG_VAR([libgcc-name], [$libc_cv_libgcc])
LIBC_CONFIG_VAR([libgcc_eh-name], [$libc_cv_libgcc_eh])

# Check if compiler runtime library provides __sfp_handle_exceptions
LIBC_TRY_CC_AND_TEST_LINK([for __sfp_handle_exceptions support],
  [void __sfp_handle_exceptions (int); __sfp_handle_exceptions (0)],
  libc_cv_sfp_handle_exceptions,
  [libc_cv_sfp_handle_exceptions=yes],
  [libc_cv_sfp_handle_exceptions=no],
  libc_cv_test_sfp_handle_exceptions,
  [libc_cv_test_sfp_handle_exceptions=yes],
  [libc_cv_test_sfp_handle_exceptions=no])
if test $libc_cv_sfp_handle_exceptions = yes; then
  AC_DEFINE([HAVE_SFP_HANDLE_EXCEPTIONS])
fi

# Support configure.ac under sysdeps.
AC_SUBST(libc_cv_test_cc_mprefer_vector_width)
AC_SUBST(test_enable_cet)

enable_gsframe=no
if test $use_sframe = yes; then
  # SFrame requires to be explicit enabled by the architecture
  if test -z $libc_cv_support_sframe; then
    AC_MSG_FAILURE([the architecture doesn't support SFrame])
  fi

  # SFrame requires binutils 2.45 or higher.
  libc_cv_sframe_readelf_version=yes
  AC_CHECK_PROG_VER(READELF, $READELF, --version,
    [GNU readelf.* \([0-9][0-9]*\.[0-9.]*\)],
    [2.4[5-9]*|2.[5-9][0-9]*|2.[1-9][0-9][0-9]*|[3-9]*|[1-9][0-9]*],
    libc_cv_sframe_readelf_version=no)
  if test $libc_cv_sframe_readelf_version = no; then
    AC_MSG_FAILURE([binutils too old to enable SFrame])
  fi

  # Check if the current toolchain supports SFrame
  AC_CACHE_CHECK([for SFrame support], libc_cv_default_sframe,
  [dnl
cat > conftest.c <<EOF
int test_function(void)
{
  return 42;
}
EOF
    libc_cv_default_sframe=no
    if ${CC} -c conftest.c -o conftest.o -Wa,--gsframe >/dev/null 2>&1 && \
       # Check if .sframe section is present and if version > 1
       $READELF --sframe conftest.o | grep "SFRAME_VER" | grep -qv "VERSION_1"; then
      libc_cv_default_sframe=yes
    fi
    rm -f conftest.c conftest.o
  ])

  if test $libc_cv_default_sframe = no; then
    AC_MSG_FAILURE([toolchain doesn't support SFrame v2 or higher])
  fi

  enable_gsframe=yes
  AC_DEFINE(ENABLE_SFRAME)
fi
LIBC_CONFIG_VAR([enable-gsframe], [$enable_gsframe])

# Set the `multidir' variable by grabbing the variable from the compiler.
# We do it once and save the result in a generated makefile.
libc_cv_multidir=`${CC-cc} $CFLAGS $CPPFLAGS -print-multi-directory`
AC_SUBST(libc_cv_multidir)

AC_SUBST(profile)

AC_SUBST(DEFINES)

dnl See sysdeps/mach/configure.ac for this variable.
AC_SUBST(mach_interface_list)

VERSION=`sed -n -e 's/^#define VERSION "\([^"]*\)"/\1/p' < $srcdir/version.h`
RELEASE=`sed -n -e 's/^#define RELEASE "\([^"]*\)"/\1/p' < $srcdir/version.h`
AC_SUBST(VERSION)
AC_SUBST(RELEASE)

# Some symbols that might be auto-generated by the compiler are redefined
# to internal alias (for instance mempcpy to __mempcpy).  However, if fortify
# is enabled, the fortify wrapper is define before the alias re-defined and
# clang warns attribute declaration must precede definition.
AC_CACHE_CHECK([attribute can declared after function declaration],
	       libc_cv_attr_after_func_decl, [dnl
cat > conftest.c <<EOF
extern __inline int foo (void) { return 0; };
extern __typeof (foo) foo __asm__ ("__foo");
EOF
libc_cv_attr_after_func_decl=no
if ${CC-cc} $CFLAGS $CPPFLAGS -fgnu89-inline -Werror conftest.c -c \
   1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD ; then
   libc_cv_attr_after_func_decl=yes
fi
rm -rf conftest*])
if test "$libc_cv_attr_after_func_decl" = yes; then
   AC_DEFINE(ATTR_AFTER_FUNC_DECL)
fi

LIBC_CONFIG_VAR([abort-in-ld],[$abort_in_ld]);

AC_CONFIG_FILES([config.make Makefile])
AC_CONFIG_COMMANDS([default],[[
case $CONFIG_FILES in *config.make*)
echo "$config_vars" >> config.make;;
esac
test -d bits || mkdir bits]],[[config_vars='$config_vars']])
AC_OUTPUT
