#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Prelude.
AC_PREREQ([2.59])

AC_INIT([Check], [0.15.2], [check-devel at lists dot sourceforge dot net])
CHECK_MAJOR_VERSION=0
CHECK_MINOR_VERSION=15
CHECK_MICRO_VERSION=2
CHECK_VERSION=$CHECK_MAJOR_VERSION.$CHECK_MINOR_VERSION.$CHECK_MICRO_VERSION

# unique source file --- primitive safety check 
AC_CONFIG_SRCDIR([src/check.c])

# place where extra autoconf macros are kept
AC_CONFIG_MACRO_DIR([m4])

# place where portability library functions are kept
AC_CONFIG_LIBOBJ_DIR([lib])

# really severe build strictness
AM_INIT_AUTOMAKE([-Wall gnits 1.11.2])
# Change to using into-in-builddir in the future:
#AM_INIT_AUTOMAKE([info-in-builddir -Wall -Werror gnits 1.14])

# define things like _GNU_SOURCE appropriately
# From patch 2803433, request system extensions to generate 64-bit safe code
AC_USE_SYSTEM_EXTENSIONS

AC_SUBST(CHECK_MAJOR_VERSION)
AC_SUBST(CHECK_MINOR_VERSION)
AC_SUBST(CHECK_MICRO_VERSION)
AC_SUBST(CHECK_VERSION)

# Configure options.
# allow `./configure --enable-silent-rules' and `make V=0'
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([no])])

AC_ARG_ENABLE(gcov,
AC_HELP_STRING([--enable-gcov],
	       [turn on test coverage @<:@default=no@:>@]),
[case "${enableval}" in
  yes) enable_gcov=true ;;
  no)  enable_gcov=false ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-gcov) ;;
esac], [enable_gcov=false ])

if test x$enable_gcov = xtrue ; then
  if test x"$GCC" != xyes; then
    AC_MSG_ERROR([gcov only works if gcc is used])
  fi

  GCOV_CFLAGS="-fprofile-arcs -ftest-coverage"
  AC_SUBST(GCOV_CFLAGS)

  dnl libtool 1.5.22 and lower strip -fprofile-arcs from the flags
  dnl passed to the linker, which is a bug; -fprofile-arcs implicitly
  dnl links in -lgcov, so we do it explicitly here for the same effect
  GCOV_LIBS=-lgcov
  AC_SUBST(GCOV_LIBS)
fi

AM_CONDITIONAL(ENABLE_GCOV, test x"$enable_gcov" = "xtrue")

AC_ARG_ENABLE(timeout-tests,
AC_HELP_STRING([--enable-timeout-tests],
	       [turn on timeout tests @<:@default=yes@:>@]),
[case "${enableval}" in
  yes) enable_timeout_tests=true ;;
  no)  enable_timeout_tests=false ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-timeout-tests) ;;
esac], [enable_timeout_tests=true ])

AC_ARG_ENABLE(build-docs,
AC_HELP_STRING([--enable-build-docs],
           [turn on building documentation @<:@default=yes@:>@]),
[case "${enableval}" in
  yes) enable_build_docs=true ;;
  no)  enable_build_docs=false ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-build-docs) ;;
esac], [enable_build_docs=true ])

AM_CONDITIONAL(NO_TIMEOUT_TESTS, test x"$enable_timeout_tests" = "xfalse")

AC_ARG_ENABLE(subunit,
AC_HELP_STRING([--enable-subunit],
	       [enable support for the subunit test protocol @<:@default=autodetect@:>@]),
[case "${enableval}" in
  yes)
    enable_subunit=true
    echo "Enabled subunit support"
    ;;
  no)
    enable_subunit=false
    echo "Disabled subunit support"
    ;;
  autodetect)
    echo "Subunit support will enable automatically."
    ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-subunit) ;;
esac], 
[echo "Subunit support will enable automatically."
 enable_subunit=autodetect])

AC_ARG_ENABLE(fork,
AC_HELP_STRING([--enable-fork],
	       [enable support for fork @<:@default=autodetect@:>@]),
[case "${enableval}" in
  yes) enable_fork=true ;;
  no)  enable_fork=false ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-fork) ;;
esac], [enable_fork=true ])

AC_ARG_ENABLE(snprintf-replacement,
AC_HELP_STRING([--enable-snprintf-replacement],
	       [enable check snprintf replacement, (even if the system provides a C99 compliant version) @<:@default=autodetect@:>@]),
[case "${enableval}" in
  yes) enable_snprintf_replacement=true ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-snprintf-replacement) ;;
esac], [enable_snprintf_replacement=autodetect ])

AC_ARG_ENABLE(timer-replacement,
AC_HELP_STRING([--enable-timer-replacement],
	       [enable check timer replacement, (even if the system provides timer_create, timer_settime, and timer_delete) @<:@default=autodetect@:>@]),
[case "${enableval}" in
  yes) enable_timer_replacement=true ;;
  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-timer-replacement) ;;
esac], [enable_timer_replacement=autodetect ])


# Checks for programs.
AC_PROG_SED
AC_PROG_AWK
AC_PROG_CC
# Automake wants this for per-target CFLAGS
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
# for non-POSIX archivers like the one on OS X
# use m4_ifdef to work on older automake (1.11)
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL

# initialize libtool to build .la files
LT_INIT

# add these options to CFLAGS if the compiler supports them
AC_DEFUN([AX_CFLAGS_ADD],[AX_C_CHECK_FLAG($1, , , CFLAGS="$CFLAGS $1")])
# Do not use the -ansi flag, currently there is a bug in MinGW/MinGW-w64
# which prevents Check from compiling. Add after this is resolved:
#    sourceforge.net/p/mingw/bugs/2024
#AX_CFLAGS_WARN_ALL_ANSI
# Do not use the -pedantic flag, as on solaris it has a different
# meaning than on gcc. Using the flag causes the build to fail.
AX_CFLAGS_ADD([-Wextra])
AX_CFLAGS_ADD([-Wstrict-prototypes])
AX_CFLAGS_ADD([-Wmissing-prototypes])
AX_CFLAGS_ADD([-Wwrite-strings])
AX_CFLAGS_ADD([-Wno-variadic-macros])
AX_CFLAGS_ADD([-Wimport])
AX_CFLAGS_ADD([-Wfatal-errors])
AX_CFLAGS_ADD([-Wformat=2])
AX_CFLAGS_ADD([-Winit-self])
AX_CFLAGS_ADD([-Wmissing-include-dirs])
AX_CFLAGS_ADD([-Wswitch-default])
AX_CFLAGS_ADD([-Wunknown-pragmas])
# The following flag is to enable C99 support on AIX, which is
# necessary for variable macros in check.h
case "${host_os}" in
    *aix*)
    if ! test "$GCC" = "yes"; then
        AX_CFLAGS_ADD([-qlanglvl=stdc99])
    fi
    ;;
    *)
    ;;
esac

AC_CHECK_PROGS(GCOV, gcov, false)
AC_CHECK_PROGS(LCOV, lcov, false)
AC_CHECK_PROGS(GENHTML, genhtml, false)

if test "xtrue" = x"$enable_build_docs"; then
    AC_CHECK_PROGS(MAKEINFO, makeinfo, false)
    if test "$MAKEINFO" = "false"; then
        # Make it [somewhat] clear to maintainers that makeinfo is missing. Not an error
        # though because 'make install' (which users need) does not build the docs
        # anyway.
        AC_MSG_WARN(makeinfo not installed: cannot rebuild HTML documentation.)
    fi

    AM_CONDITIONAL(MAKE_DOCS, [test x"$MAKEINFO" != "xfalse"])
else
   AM_CONDITIONAL(MAKE_DOCS, [false])
fi
AC_CHECK_PROGS(FILTERDIFF, filterdiff, false)
if test "$FILTERDIFF" = "false"; then
    # Make it [somewhat] clear to maintainers that filterdiff is missing.
    # This is not an error, but will prevent builds from being
    # reproducible.
    AC_MSG_WARN(filterdiff not installed; build will not be reproducible.)
fi

AM_CONDITIONAL(USE_FILTERDIFF, [test x"$FILTERDIFF" = x"filterdiff"])

AC_CHECK_PROGS(GRAPHVIZ, dot, false)
# If graphviz doesn't exist 'make doc/doxygen-devel' will skip rendering graphs
# and inform the developer about it. This target is optional and it aims
# developers of libcheck, not users.
AM_CONDITIONAL(USE_GRAPHVIZ, [test x"$GRAPHVIZ" = x"dot"])

# Checks for pthread implementation.
ACX_PTHREAD
CC="$PTHREAD_CC"

# Check if floor is in the math library, and if so add -lm to LIBS
AC_CHECK_LIB([m], [floor])

# Check if clock_gettime, timer_create, timer_settime, and timer_delete are available in lib rt, and if so,
# add -lrt to LIBS
AC_CHECK_LIB([rt], [clock_gettime, timer_create, timer_settime, timer_delete])

# check that struct timespec is defined in time.h. If not, we need to
# define it in libcompat.h. Note the optional inclusion of pthread.h.
# On MinGW and MinGW-w64, the pthread.h file contains the timespec
# definition.
AC_CHECK_MEMBERS([struct timespec.tv_sec, struct timespec.tv_nsec], [], [AC_DEFINE_UNQUOTED(STRUCT_TIMESPEC_DEFINITION_MISSING, 1, "Need to define the timespec structure")], [
#include <time.h>
#if defined(HAVE_PTHREAD)
#include <pthread.h>
#endif /* HAVE_PTHREAD */
])

# check that struct itimerspec is defined in time.h. If not, we need to
# define it in libcompat.h.  Note the optional inclusion of pthread.h.
# On MinGW and MinGW-w64, the pthread.h file contains the itimerspec
# definition.
AC_CHECK_MEMBERS([struct itimerspec.it_interval, struct itimerspec.it_value], [], [AC_DEFINE_UNQUOTED(STRUCT_ITIMERSPEC_DEFINITION_MISSING, 1, "Need to define the itimerspec structure")], [
#include <time.h>
#if defined(HAVE_PTHREAD)
#include <pthread.h>
#endif /* HAVE_PTHREAD */
])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h stddef.h stdlib.h string.h sys/time.h unistd.h])
AX_CREATE_STDINT_H(check_stdint.h)

AS_IF([test x"$enable_subunit" != "xfalse" && test x"$enable_subunit" != "xtrue"], [
	PKG_CHECK_EXISTS([libsubunit], [:], [enable_subunit=false])
	])
AS_IF([test x"$enable_subunit" != "xfalse"], [
	PKG_CHECK_MODULES([LIBSUBUNIT], [libsubunit])
	])
if test "xfalse" = x"$enable_subunit"; then
ENABLE_SUBUNIT="0"
LIBSUBUNIT_PC=""
else
ENABLE_SUBUNIT="1"
LIBSUBUNIT_PC="libsubunit"
fi
AC_SUBST(ENABLE_SUBUNIT)
AC_SUBST([LIBSUBUNIT_PC])
AC_DEFINE_UNQUOTED(ENABLE_SUBUNIT, $ENABLE_SUBUNIT, [Subunit protocol result output])

AM_CONDITIONAL(SUBUNIT, test x"$enable_subunit" != "xfalse")

# Check for POSIX regular expressions support.
AC_CHECK_HEADERS([regex.h], HAVE_REGEX_H=1, HAVE_REGEX_H=0)

# Check if we have the windows headers for Init Once API
AC_CHECK_HEADERS([windows.h], HAVE_WINDOWS_H=1, HAVE_WINDOWS_H=0)
AC_SUBST(HAVE_WINDOWS_H)

# Check if we have the One-Time Initialization API
AC_CHECK_FUNCS([InitOnceBeginInitialize], HAVE_INIT_ONCE_BEGIN_INITIALIZE=1, HAVE_INIT_ONCE_BEGIN_INITIALIZE=0)
AC_CHECK_FUNCS([InitOnceComplete], HAVE_INIT_ONCE_COMPLETE=1, HAVE_INIT_ONCE_COMPLETE=0)
AC_SUBST(HAVE_INIT_ONCE_BEGIN_INITIALIZE)
AC_SUBST(HAVE_INIT_ONCE_COMPLETE)

if test "x$HAVE_REGEX_H" = "x1"; then
    AC_CHECK_FUNCS([regcomp regexec], HAVE_REGEX=1, HAVE_REGEX=0)
else
    HAVE_REGEX=0
fi
AC_SUBST(HAVE_REGEX)
AC_DEFINE_UNQUOTED(HAVE_REGEX, $HAVE_REGEX, "Regular expressions are supported")

if test "x$HAVE_REGEX" = "x1"; then
    ENABLE_REGEX="1"
else
    ENABLE_REGEX="0"
fi
AC_SUBST(ENABLE_REGEX)
AC_DEFINE_UNQUOTED(ENABLE_REGEX, $ENABLE_REGEX, "Regular expressions are supported and enabled")

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_INTMAX_T
AC_TYPE_UINTMAX_T
AC_TYPE_UINT32_T
AC_HEADER_TIME
AC_STRUCT_TM

AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(long, 4)

# The following two checks will attempt to include pthread.h. The
# reason is MinGW and MinGW-w64 have been known to put the time
# related definitions in the pthread headers. Without include
# pthread.h, these checks may mistakenly fail to find the
# definitions.
AC_CHECK_TYPE(clockid_t, [], [AC_DEFINE([clockid_t], [int], [clockid_t])], [
AC_INCLUDES_DEFAULT
#if defined(HAVE_PTHREAD)
#include <pthread.h>
#endif /* HAVE_PTHREAD */
])
AC_CHECK_TYPE(timer_t, [], [AC_DEFINE([timer_t], [int], [timer_t])], [
AC_INCLUDES_DEFAULT
#if defined(HAVE_PTHREAD)
#include <pthread.h>
#endif /* HAVE_PTHREAD */
])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

# Check if the timer_create(), timer_settime(), and timer_delete()
# functions on the system are available and suitable, or need to be
# replaced with Check's replacement of these functions.
HW_LIBRT_TIMERS

# The following checks will replace missing functions from libcompat
AC_REPLACE_FUNCS([alarm clock_gettime getline gettimeofday localtime_r strdup strsignal])
AC_CHECK_DECLS([alarm, clock_gettime, getline, gettimeofday, localtime_r, strdup, strsignal])

# The following checks are to only detect if the functions exist, but
# not replace them
AC_CHECK_DECLS([setenv])

AC_CHECK_FUNCS([setitimer])

# Checks for functions not available in Windows
if test "xtrue" = x"$enable_fork"; then
	AC_CHECK_FUNCS([fork], HAVE_FORK=1, HAVE_FORK=0)
else
	HAVE_FORK=0
fi
AC_SUBST(HAVE_FORK)
AC_CHECK_FUNCS([sigaction])
AC_CHECK_FUNCS([mkstemp])

# Check if the system's snprintf (and its variations) are C99 compliant.
# If they are not, use the version in libcompat.
HW_FUNC_VSNPRINTF
HW_FUNC_SNPRINTF

# Check for whether we can install checkmk (we have a usable awk)
AC_ARG_VAR([AWK_PATH],[Awk interpreter command])
AC_PATH_PROG(AWK_PATH, $AWK, [*NO AWK*])
AM_CONDITIONAL([INSTALL_CHECKMK], [test "x$AWK_PATH" != 'x*NO AWK*'])

# Certain awk implementations disagree with each other on how to
# substitute doubled backslashes in gsub()
AC_SUBST(AWK_GSUB_DBL_BSLASH, '\\\\')

AS_IF([test "x$AWK_PATH" = 'x*NO AWK*'],
      [AC_MSG_WARN([Couldn't find a usable awk; won't install checkmk.])],

      # Determine correct number of backslashes for gsub's replacement
      # value.
      [AS_IF([echo '\' |
              "$AWK_PATH" '{ gsub("\\\\", "\\\\", $0); print }' |
              grep '^\\$' >/dev/null 2>&1], AWK_GSUB_DBL_BSLASH='\\\\\\\\')
       AC_CONFIG_FILES(checkmk/checkmk)
       AC_CONFIG_COMMANDS([checkmk-x], [chmod +x checkmk/checkmk])])

# Output files
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([check.pc
                 Makefile
                 checkmk/Makefile
		 doc/Makefile
		 lib/Makefile
                 src/check.h
                 src/Makefile
                 tests/Makefile
		 tests/test_vars])

AC_OUTPUT

# Finally, print a summary of the Check's compile options

echo
echo "=========================================="
echo "Summary of Check $CHECK_MAJOR_VERSION.$CHECK_MINOR_VERSION.$CHECK_MICRO_VERSION options:"
echo

if test "x0" = x"$HAVE_FORK"; then
    result="no"
else
    result="yes"
fi
echo "fork mode ............................ $result"

case "$hw_cv_librt_timers_posix" in
    "yes")
        result="no"
        ;;
    "no")
        result="yes"
        ;;
    *)
        # The AC_REPLACE_FUNCS macro was invoked,
        # meaning we are cross compiling.
        if test "xno" = x"$ac_cv_func_timer_create"; then
            result="yes"
        else
            result="no"
        fi
        ;;
esac
echo "high resolution timer replacement .... $result"

if test "xno" = x"$hw_cv_func_snprintf_c99"; then
    result="yes"
else
    result="no"
fi
echo "snprintf replacement ................. $result"

if test "xfalse" = x"$enable_subunit"; then
    result="no"
else
    result="yes"
fi
echo "subunit support....................... $result"

if test "xtrue" = x"$enable_timeout_tests"; then
    result="yes"
else
    result="no"
fi
echo "timeout unit tests ................... $result"

if test "x0" = x"$ENABLE_REGEX"; then
    result="no"
else
    result="yes"
fi
echo "POSIX regular expressions ............ $result"

if test "xtrue" = x"$enable_build_docs"; then
    result="yes"
else
    result="no"
fi
echo "build docs ........................... $result"

echo "=========================================="
