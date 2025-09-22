#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIR([m4])
AC_INIT([procps-ng],
	m4_esyscmd([local/git-version-gen .tarball-version]),
	[procps@freelists.org],,[https://gitlab.com/procps-ng/procps])
AM_INIT_AUTOMAKE([foreign 1.11 subdir-objects -Wall -Wno-portability tar-pax no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/free.c])
AC_CONFIG_HEADERS([config.h])
AC_LANG([C])

# Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CC_STDC
if test "$cross_compiling" = no; then
  if test "x$ac_cv_prog_cc_c99" = "xno" || test "x$ac_cv_prog_cc_c99" = "x"; then
  # We might be on RHEL5 with a git checkout and so broken
  # autoconf. Check if CC is gcc and if it bails when given -std=gnu99.
  # If not, use that.  Yuck.
    if test "x$ac_cv_c_compiler_gnu" = "xyes"; then
       CC="$CC -std=gnu99"
       AC_RUN_IFELSE(
          [AC_LANG_PROGRAM([],[[
  return 0;
      ]])],
      [],
      [AC_MSG_ERROR([Could not find a C99 compatible compiler])])
    else
      AC_MSG_ERROR([Could not find a C99 compatible compiler])
    fi
  fi
fi
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Check pkg-config is there for autoconf and at ./configure
m4_ifndef([PKG_PREREQ],
  [m4_fatal([must install pkg-config 0.29 or later before running autoconf/autogen])])
PKG_PROG_PKG_CONFIG
AS_IF([test -z "$PKG_CONFIG"],[
  AC_MSG_ERROR([Could not find pkg-config])])

AC_PROG_MAKE_SET

AC_DEFUN([PROCPS_PROG_PO4A], [
  AC_REQUIRE([AM_NLS])
  AC_CHECK_PROGS([PO4A], [po4a])
  AS_IF([test "$USE_NLS" = "yes" && test -n "$PO4A"], [
    USE_PO4A=yes
  ], [
    USE_PO4A=no
  ])
  AC_SUBST([USE_PO4A])
])
PROCPS_PROG_PO4A

# Checks for header files.
AC_HEADER_MAJOR
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h langinfo.h libintl.h limits.h locale.h stdint.h stdio_ext.h stdlib.h string.h sys/file.h sys/ioctl.h sys/param.h sys/time.h termios.h unistd.h utmp.h utmpx.h values.h wchar.h wctype.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_PID_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_CHECK_MEMBERS([siginfo_t.si_int], [], [], [[#include <signal.h>]])

dnl Needed for musl
if test "x$ac_cv_header_utmpx_h" = xyes
then :
AC_CHECK_DECLS([__UT_HOSTSIZE],
               [AC_DEFINE([HAVE_UT_HOSTSIZE_IN_UTMPX], [1],
                          [Define if __UT_HOSTSIZE in utmpx.h])],
               [],
               [[#include <utmpx.h>]])
fi

dnl libtool
LT_INIT

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MBRTOWC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRCOLL
AC_FUNC_STRTOD

usrbin_execdir='${exec_prefix}/bin'
AC_SUBST([usrbin_execdir])

if test "x$host_os" = xcygwin
then
  prefix=/usr
  sysconfdir='${prefix}/etc'
  libexecdir='${prefix}/lib'
  localstatedir='${prefix}/var'
  datadir='${prefix}/share'
  mandir='${prefix}/share/man'
  infodir='${prefix}/share/info'
  AC_SUBST([prefix])
  AC_SUBST([sysconfdir])
  AC_SUBST([libexecdir])
  AC_SUBST([localstatedir])
  AC_SUBST([datadir])
  AC_SUBST([mandir])
  AC_SUBST([infodir])
 AC_DEFINE([USE_PROC_CTTY], [1], [Use /proc/$PID/ctty to determine controlling TTY])
else
 AC_DEFINE([USE_PROC_CTTY], [0], [Use /proc/$PID/ctty to determine controlling TTY])
fi

AM_GNU_GETTEXT_VERSION([0.14.1])
AM_GNU_GETTEXT([external])
if test -d "$srcdir/po"
then
	ALL_LINGUAS=`cd $srcdir/po > /dev/null && echo *.po | sed 's/\.po//g'`
else
	ALL_LINGUAS="af am ar as be bg bn_IN bn ca cs cy da de el en_GB es et eu_ES fa fi fr gl gu he hi hr hu hy id is it ja ka kn ko ku lo lt lv mk ml mr ms my nb nl nn no nso or pa pl pt_BR pt ro ru si sk sl sq sr@Latn sr sv ta te th tr uk ur vi zh_CN zh_TW zu"
fi

AC_CHECK_HEADERS(error.h, [], [], AC_INCLUDES_DEFAULT)

AC_CHECK_HEADERS(stdio_ext.h, [], [], AC_INCLUDES_DEFAULT)

AC_MSG_CHECKING(for __progname)
AC_LINK_IFELSE([AC_LANG_SOURCE(
	[
		#include <stdio.h>
		#include <stdlib.h>
		extern char *__progname;
		int main(void) {
			const char *cp = __progname;
			printf("%s\n", cp);
			exit(0);
		}
	])],
	AC_DEFINE(HAVE___PROGNAME, 1, [Define if __progname exists])
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no))

AC_MSG_CHECKING(whether program_invocation_name is defined)
AC_TRY_COMPILE([#include <errno.h>],
		[program_invocation_name = "test";],
		AC_DEFINE(HAVE_PROGRAM_INVOCATION_NAME, 1,
			[Define if program_invocation_name is defined])
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no))

AC_MSG_CHECKING(whether program_invocation_short_name is defined)
AC_TRY_COMPILE([#include <errno.h>],
		[program_invocation_short_name = "test";],
		AC_DEFINE(HAVE_PROGRAM_INVOCATION_SHORT_NAME, 1,
			[Define if program_invocation_short_name is defined])
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no))

AC_CHECK_FUNC([pidfd_open], [enable_pidwait=yes], [
  AC_MSG_CHECKING([for __NR_pidfd_open])
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <sys/syscall.h>
#ifndef __NR_pidfd_open
#error __NR_pidfd_open not defined
#endif
    ])], [enable_pidwait=yes], [enable_pidwait=no])
  AC_MSG_RESULT([$enable_pidwait])
])
if test "$enable_pidwait" = yes; then
  AC_DEFINE([ENABLE_PIDWAIT], [1], [Enable pidwait])
fi
AM_CONDITIONAL([BUILD_PIDWAIT], [test x$enable_pidwait = xyes])

dnl watch8bit must be before the AC_ARG_WITH set as it sets up ncurses
AC_SUBST([WITH_WATCH8BIT])
AC_ARG_ENABLE([watch8bit],
  AS_HELP_STRING([--enable-watch8bit], [enable watch to be 8bit clean (requires ncursesw)]),
  [], [enable_watch8bit=no]
)
if test "$enable_watch8bit" = "yes"; then
  AC_DEFINE([WITH_WATCH8BIT], [1], [Enable 8 bit clean watch])
fi

AC_SUBST([WITH_COLORWATCH])
AC_ARG_ENABLE([colorwatch],
  AS_HELP_STRING([--enable-colorwatch], [enable watch to use color by default]),
  [], [enable_colorwatch=no]
)
if test "$enable_colorwatch" = "yes"; then
  AC_DEFINE([WITH_COLORWATCH], [1], [Enable color watch by default])
fi



AC_ARG_ENABLE([libselinux],
  AS_HELP_STRING([--enable-libselinux], [enable libselinux]),
  [], [enable_libselinux=no]
)
if test "$enable_libselinux" = "yes"; then
  AC_DEFINE([ENABLE_LIBSELINUX], [1], [Enable libselinux])
fi

# Enable hardened compile and link flags
AC_ARG_ENABLE([harden_flags],
  [AS_HELP_STRING([--enable-harden-flags], [enable hardened compilier and linker flags])],
  [enable_harden_flags=$enableval],
  [enable_harden_flags="no"])

# Check that harden CFLAGS and LDFLAGS will compile
AS_IF([test "$enable_harden_flags" = "yes"],
  HARDEN_CFLAGS="-g -O2 -fPIE -fstack-protector-strong -Wformat -Werror=format-security"
  [HARDEN_LDFLAGS="-fPIE -pie -Wl,-z,relro -Wl,-z,now"]
  [ AC_MSG_CHECKING([compiler supports harden flags])
  save_harden_cflags="$CFLAGS"
  CFLAGS="$CFLAGS $HARDEN_CFLAGS"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,,)],
      [AC_MSG_RESULT([yes])],
     [AC_MSG_RESULT([no]); HARDEN_CFLAGS='']
    )
  CFLAGS="$save_harden_cflags"],
  [HARDEN_CFLAGS=""
   HARDEN_LDFLAGS=""])
AC_SUBST([HARDEN_CFLAGS])
AC_SUBST([HARDEN_LDFLAGS])

# Optional packages - AC_ARG_WITH
AC_ARG_WITH([ncurses],
  AS_HELP_STRING([--without-ncurses], [build only applications not needing ncurses]),
  [],
  [with_ncurses=yes]
)
if test "x$with_ncurses" = xno; then
  AM_CONDITIONAL(WITH_NCURSES, false)
  AS_IF([test "x$enable_watch8bit" = "xyes"],
    [AC_MSG_ERROR([Cannot have both --enable-watch8bit and --without-ncurses])])
else
  AM_CONDITIONAL(WITH_NCURSES, true)
  PKG_CHECK_MODULES([NCURSES], [ncursesw],
    [AC_DEFINE([HAVE_NCURSES], [1], [Use NCURSESW])],
    [
      AS_IF([test "x$enable_watch8bit" = "xyes"],
        [AC_MSG_ERROR([Cannot find ncurses wide library ncursesw with --enable-watch8bit])])
      PKG_CHECK_MODULES([NCURSES], [ncurses],
        [AC_DEFINE([HAVE_NCURSES], [1], [Use NCURSES])],
        [AC_MSG_ERROR([ncurses support missing/incomplete (for partial build use --without-ncurses)])]
      )
    ]
  )
fi
#  PKG_CHECK_MODULES([NCURSES], [ncurses], [], [
#    AC_CHECK_LIB(ncurses, initscr, [have_ncurses=yes], [have_ncurses=no])
#    AC_CHECK_HEADERS(curses.h ncurses.h term.h, [], [have_ncurses=no], AC_INCLUDES_DEFAULT)
#    if test "x$have_ncurses" = xno; then
#      AC_MSG_ERROR([ncurses support missing/incomplete (for partial build use --without-ncurses)])
#    fi
#    NCURSES_LIBS="-lncurses"
#  ])
#  AM_CONDITIONAL(WITH_NCURSES, true)
#  if test "$enable_watch8bit" = yes; then
#    PKG_CHECK_MODULES([NCURSESW], [ncursesw], [WATCH_NCURSES_LIBS="$NCURSESW_LIBS"], [
#      AC_CHECK_LIB([ncursesw], [addwstr], [WATCH_NCURSES_LIBS=-lncursesw], [
#        AC_CHECK_LIB([ncurses], [addwstr], [WATCH_NCURSES_LIBS=-lncurses], [
#          AC_MSG_ERROR([Cannot find ncurses wide library ncursesw with --enable-watch8bit])
#	])
#      ])
#    ])
#  else
#    WATCH_NCURSES_LIBS="$NCURSES_LIBS"
#  fi

AC_ARG_WITH([systemd],
  [AS_HELP_STRING([--with-systemd], [enable systemd support])],
  [], [with_systemd=no]
)
AS_IF([test "x$with_systemd" != "xno"], [
  PKG_CHECK_MODULES([SYSTEMD], [libsystemd],,
    [PKG_CHECK_MODULES([SYSTEMD], [libsystemd-login])]
  )
  AC_DEFINE(WITH_SYSTEMD, 1, [enable systemd support])

  # The functions needed to replace utmp with logind are only available
  # with systemd v254 or later.
  old_LIBS="$LIBS"
  LIBS="$LIBS $SYSTEMD_LIBS"
  AC_CHECK_FUNCS([sd_session_get_leader])
  LIBS="$old_LIBS"
])
AM_CONDITIONAL([WITH_SYSTEMD], [test x$with_systemd != xno])

AC_ARG_WITH([elogind],
  [AS_HELP_STRING([--with-elogind], [enable elogind support])],
  [], [with_elogind=no]
)
# Do not allow elogind if systemd is wanted and found
AS_IF([test "x$with_systemd" != "xno"], [with_elogind=no])

AS_IF([test "x$with_elogind" != "xno"], [
  PKG_CHECK_MODULES([ELOGIND], [libelogind])
  AC_DEFINE(WITH_ELOGIND, 1, [enable elogind support])
])
AM_CONDITIONAL([WITH_ELOGIND], [test x$with_elogind != xno])

# AC_ARG_ENABLEs
AC_ARG_ENABLE([pidof],
  AS_HELP_STRING([--disable-pidof], [do not build pidof]),
  [], [enable_pidof=yes]
)
AM_CONDITIONAL(BUILD_PIDOF, test "x$enable_pidof" = xyes)

AC_ARG_ENABLE([kill],
  AS_HELP_STRING([--disable-kill], [do not build kill]),
  [], [enable_kill=yes]
)
AM_CONDITIONAL(BUILD_KILL, test "x$enable_kill" = xyes)
AC_ARG_ENABLE([w],
  AS_HELP_STRING([--disable-w], [do not build w]),
  [], [enable_w=yes]
)
AM_CONDITIONAL(BUILD_W, test "x$enable_w" = xyes)

AM_CONDITIONAL(LINUX, test "x$host_os" = xlinux-gnu)
AM_CONDITIONAL(CYGWIN, test "x$host_os" = xcygwin)

AC_ARG_ENABLE([skill],
  AS_HELP_STRING([--enable-skill], [build skill and snice]),
  [], [enable_skill=no]
)
AM_CONDITIONAL(BUILD_SKILL, test "x$enable_skill" = xyes)

AC_ARG_ENABLE([examples],
  AS_HELP_STRING([--enable-examples], [add example files to installation]),
  [], [enable_examples=no]
)
AM_CONDITIONAL(EXAMPLE_FILES, test "x$enable_examples" = xyes)

AC_ARG_ENABLE([sigwinch],
  AS_HELP_STRING([--enable-sigwinch], [reduce impact of x-windows resize operations on top]),
  [], [enable_sigwinch=no]
)
if test "x$enable_sigwinch" = xyes; then
  AC_DEFINE(SIGNALS_LESS, 1, [reduce impact of x-windows resize operations on top])
fi

AC_ARG_ENABLE([wide-percent],
  AS_HELP_STRING([--enable-wide-percent], [provide extra precision under %CPU and %MEM for top]),
  [], [enable_wide_percent=no]
)
if test "x$enable_wide_percent" = xyes; then
  AC_DEFINE(BOOST_PERCNT, 1, [provide extra precision under %CPU and %MEM for top])
fi

AC_ARG_ENABLE([wide-memory],
  AS_HELP_STRING([--enable-wide-memory], [provide extra precision under memory fields for top]),
  [], [enable_wide_memory=no]
)
if test "x$enable_wide_memory" = xyes; then
  AC_DEFINE(BOOST_MEMORY, 1, [provide extra precision under memory fields for top])
fi

AC_ARG_ENABLE([modern-top],
  AS_HELP_STRING([--disable-modern-top], [disable new startup defaults, return to original top]),
  [], [enable_modern_top=yes]
)
if test "x$enable_modern_top" = xno; then
  AC_DEFINE(ORIG_TOPDEFS, 1, [disable new startup defaults, return to original top])
fi

DL_LIB=
AC_ARG_ENABLE([numa],
  AS_HELP_STRING([--disable-numa], [disable NUMA/Node support in top]),
  [], [enable_numa=yes]
)
if test "x$enable_numa" = xno; then
  AC_DEFINE([NUMA_DISABLE], [1], [disable NUMA/Node support in top])
else
  AC_SEARCH_LIBS([dlopen], [dl], [],
    [AC_MSG_ERROR([dynamic linking unavailable, circumvent with --disable-numa])])
  if test "x$ac_cv_search_dlopen" != "xnone required"; then
    DL_LIB="$ac_cv_search_dlopen"
  fi
fi
AC_SUBST([DL_LIB])

AC_ARG_ENABLE([w-from],
  AS_HELP_STRING([--enable-w-from], [enable w from field by default]),
  [], [enable_w_from=no]
)
if test "x$enable_w_from" = xyes; then
  AC_DEFINE(W_SHOWFROM, 1, [enable w from field by default])
fi

AC_ARG_ENABLE([whining],
  AS_HELP_STRING([--disable-whining], [do not print unnecessary warnings (slackware-ism)]),
  [], [enable_whining=yes]
)
if test "x$enable_whining" = xyes; then
  AC_DEFINE(BUILD_WITH_WHINE, 1, [should extra warnings be printed (slackware-ism)])
fi

if test x"$DEJAGNU" = x
then
  DEJAGNU="/dev/null"
fi
AC_SUBST(DEJAGNU)

AC_CHECK_FUNCS([__fpending alarm atexit dup2 gethostname getpagesize gettimeofday iswprint memchr memmove memset mkdir nl_langinfo putenv regcomp rpmatch select setlocale sigabbrev_np strcasecmp strchr strcspn strdup strerror strncasecmp strndup strpbrk strrchr strspn strstr strtol strtoul strverscmp utmpname wcwidth])

AC_CONFIG_FILES([Makefile
                 local/Makefile
                 po-man/Makefile
                 po/Makefile.in
                 library/libproc2.pc
                 testsuite/Makefile])
AC_OUTPUT
