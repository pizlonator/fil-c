dnl If needed, define the m4_ifblank and m4_ifnblank macros from autoconf 2.64
dnl This allows us to run with earlier Autoconfs as well.
dnl
dnl m4_ifblank(COND, [IF-BLANK], [IF-TEXT])
dnl m4_ifnblank(COND, [IF-TEXT], [IF-BLANK])
dnl ----------------------------------------
dnl If COND is empty, or consists only of blanks (space, tab, newline),
dnl then expand IF-BLANK, otherwise expand IF-TEXT.  This differs from
dnl m4_ifval only if COND has just whitespace, but it helps optimize in
dnl spite of users who mistakenly leave trailing space after what they
dnl thought was an empty argument:
dnl   macro(
dnl         []
dnl        )
dnl
dnl Writing one macro in terms of the other causes extra overhead, so
dnl we inline both definitions.
ifdef([m4_ifblank],[],[
m4_define([m4_ifblank],
[m4_if(m4_translit([[$1]],  [ ][	][
]), [], [$2], [$3])])])

ifdef([m4_ifnblank],[],[
m4_define([m4_ifnblank],
[m4_if(m4_translit([[$1]],  [ ][	][
]), [], [$3], [$2])])])

dnl UL_PKG_STATIC(VARIABLE, MODULES)
dnl
dnl Calls pkg-config --static
dnl
AC_DEFUN([UL_PKG_STATIC], [
  if test "$enable_static" != xno; then
    if AC_RUN_LOG([$PKG_CONFIG --exists --print-errors "$2"]); then
      $1=`$PKG_CONFIG --libs --static "$2"`
    else
      AC_MSG_ERROR([pkg-config description of $2, needed for static build, is not available])
    fi
  fi
])

dnl UL_CHECK_LIB(LIBRARY, FUNCTION, [VARSUFFIX = $1]))
dnl
dnl The VARSUFFIX is optional and overrides the default behavior. For example:
dnl     UL_CHECK_LIB(yyy, func, xxx) generates have_xxx and HAVE_LIBXXX
dnl     UL_CHECK_LIB(yyy, func)      generates have_yyy and HAVE_LIBYYY
dnl
AC_DEFUN([UL_CHECK_LIB], [
  m4_define([suffix], m4_default([$3],$1))
  [have_]suffix=yes
  m4_ifdef([$3],
    [AC_CHECK_LIB([$1], [$2], [AC_DEFINE(AS_TR_CPP([HAVE_LIB]suffix), 1)], [[have_]suffix=no])],
    [AC_CHECK_LIB([$1], [$2], [], [[have_]suffix=no])])
  AM_CONDITIONAL(AS_TR_CPP([HAVE_]suffix), [test [$have_]suffix = yes])
])


dnl UL_SET_ARCH(ARCHNAME, PATTERN)
dnl
dnl Define ARCH_<archname> condition if the pattern match with the current
dnl architecture
dnl
AC_DEFUN([UL_SET_ARCH], [
  cpu_$1=false
  case "$host" in
   $2) cpu_$1=true ;;
  esac
  AM_CONDITIONAL(AS_TR_CPP(ARCH_$1), [test "x$cpu_$1" = xtrue])
])


dnl UL_SET_LIBS(LIBS)
dnl
dnl Sets new global LIBS, the original setting could be restored by UL_RESTORE_LIBS()
dnl
AC_DEFUN([UL_SET_LIBS], [
  old_LIBS="$LIBS"
  LIBS="$LIBS $1"
])

dnl UL_RESTORE_LIBS()
dnl
dnl Restores LIBS previously saved by UL_SET_LIBS()
dnl
AC_DEFUN([UL_RESTORE_LIBS], [
  LIBS="$old_LIBS"
])


dnl UL_CHECK_SYSCALL(SYSCALL, FALLBACK, ...)
dnl
dnl Only specify FALLBACK if the SYSCALL you're checking for is a "newish" one
dnl
AC_DEFUN([UL_CHECK_SYSCALL], [
  dnl This macro uses host_cpu.
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_CACHE_CHECK([for syscall $1],
    [ul_cv_syscall_$1],
    [_UL_SYSCALL_CHECK_DECL([SYS_$1],
      [syscall=SYS_$1],
      [dnl Our libc failed use, so see if we can get the kernel
      dnl headers to play ball ...
      _UL_SYSCALL_CHECK_DECL([__NR_$1],
	[syscall=__NR_$1],
	[
	  syscall=no
	  if test "x$linux_os" = xyes; then
	    case $host_cpu in
	      _UL_CHECK_SYSCALL_FALLBACK(m4_shift($@))
	    esac
	  fi
        ])
      ])
    ul_cv_syscall_$1=$syscall
    ])
  case $ul_cv_syscall_$1 in #(
  no) AC_MSG_WARN([Unable to detect syscall $1.]) ;;
  SYS_*) ;;
  *) AC_DEFINE_UNQUOTED([SYS_$1], [$ul_cv_syscall_$1],
	[Fallback syscall number for $1]) ;;
  esac
])


dnl _UL_SYSCALL_CHECK_DECL(SYMBOL, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND)
dnl
dnl Check if SYMBOL is declared, using the headers needed for syscall checks.
dnl
m4_define([_UL_SYSCALL_CHECK_DECL],
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/syscall.h>
#include <unistd.h>
]], [[int test = $1;]])],
[$2], [$3])
])

dnl _UL_CHECK_SYSCALL_FALLBACK(PATTERN, VALUE, ...)
dnl
dnl Helper macro to create the body for the above `case'.
dnl
m4_define([_UL_CHECK_SYSCALL_FALLBACK],
[m4_ifval([$1],
  [#(
  $1) syscall="$2" ;;dnl
  _UL_CHECK_SYSCALL_FALLBACK(m4_shiftn(2, $@))])dnl
])


dnl UL_REQUIRES_LINUX(NAME, [VARSUFFIX = $1])
dnl
dnl Modifies $build_<name>  variable according to $enable_<name> and OS type. The
dnl $enable_<name> could be "yes", "no" and "check". If build_<name> is "no" then
dnl all checks are skipped.
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $2.
dnl
AC_DEFUN([UL_REQUIRES_LINUX], [
  m4_define([suffix], m4_default([$2],$1))
  if test "x$[build_]suffix" != xno; then
    AC_REQUIRE([AC_CANONICAL_HOST])
    case $[enable_]suffix:$linux_os in #(
    no:*)
      [build_]suffix=no ;;
    yes:yes)
      [build_]suffix=yes ;;
    yes:*)
      AC_MSG_ERROR([$1 selected for non-linux system]);;
    check:yes)
      [build_]suffix=yes ;;
    check:*)
      AC_MSG_WARN([non-linux system; not building $1])
      [build_]suffix=no ;;
    esac
  fi
])


dnl UL_EXCLUDE_ARCH(NAME, ARCH, [VARSUFFIX = $1])
dnl
dnl Modifies $build_<name>  variable according to $enable_<name> and $host. The
dnl $enable_<name> could be "yes", "no" and "check". If build_<name> is "no" then
dnl all checks are skipped.
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $3.
dnl
AC_DEFUN([UL_EXCLUDE_ARCH], [
  m4_define([suffix], m4_default([$3],$1))
  if test "x$[build_]suffix" != xno; then
    AC_REQUIRE([AC_CANONICAL_HOST])
    case $[enable_]suffix:"$host" in #(
    no:*)
      [build_]suffix=no ;;
    yes:$2)
      AC_MSG_ERROR([$1 selected for unsupported architecture]);;
    yes:*)
      [build_]suffix=yes ;;
    check:$2)
      AC_MSG_WARN([excluded for $host architecture; not building $1])
      [build_]suffix=no ;;
    check:*)
      [build_]suffix=yes ;;
    esac
  fi
])



dnl UL_REQUIRES_ARCH(NAME, ARCH, [VARSUFFIX = $1])
dnl
dnl Modifies $build_<name>  variable according to $enable_<name> and $host. The
dnl $enable_<name> could be "yes", "no" and "check". If build_<name> is "no" then
dnl all checks are skipped.
dnl
dnl The <arch> maybe a list, then at least one of the patterns in the list
dnl have to match.
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $3.
dnl
AC_DEFUN([UL_REQUIRES_ARCH], [
  m4_define([suffix], m4_default([$3],$1))
  if test "x$[build_]suffix" != xno; then
    AC_REQUIRE([AC_CANONICAL_HOST])
    [ul_archone_]suffix=no
    m4_foreach([onearch], [$2],  [
      case "$host" in #(
      onearch)
        [ul_archone_]suffix=yes ;;
      esac
    ])dnl
    case $[enable_]suffix:$[ul_archone_]suffix in #(
    no:*)
      [build_]suffix=no ;;
    yes:no)
      AC_MSG_ERROR([$1 selected for unsupported architecture]);;
    yes:*)
      [build_]suffix=yes ;;
    check:no)
      AC_MSG_WARN([excluded for $host architecture; not building $1])
      [build_]suffix=no ;;
    check:*)
      [build_]suffix=yes ;;
    esac
  fi
])

dnl UL_REQUIRES_HAVE(NAME, HAVENAME, HAVEDESC, [VARSUFFIX=$1])
dnl
dnl Modifies $build_<name> variable according to $enable_<name> and
dnl $have_<havename>.  The <havedesc> is description used for warning/error
dnl message (e.g. "function").
dnl
dnl The <havename> maybe a list, then at least one of the items in the list
dnl have to exist, for example: [ncurses, tinfo] means that have_ncurser=yes
dnl *or* have_tinfo=yes must be defined.
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $4.
dnl
AC_DEFUN([UL_REQUIRES_HAVE], [
  m4_define([suffix], m4_default([$4],$1))

  if test "x$[build_]suffix" != xno; then

    [ul_haveone_]suffix=no
    m4_foreach([onehave], [$2],  [
      if test "x$[have_]onehave" = xyes; then
        [ul_haveone_]suffix=yes
      fi
    ])dnl

    case $[enable_]suffix:$[ul_haveone_]suffix in #(
    no:*)
      [build_]suffix=no ;;
    yes:yes)
      [build_]suffix=yes ;;
    yes:*)
      AC_MSG_ERROR([$1 selected, but required $3 not available]);;
    check:yes)
      [build_]suffix=yes ;;
    check:*)
      AC_MSG_WARN([$3 not found; not building $1])
      [build_]suffix=no ;;
    esac
  fi
])

dnl UL_REQUIRES_COMPILE(NAME, PROGRAM_PROLOGUE, PROGRAM_BODY, DESC, [VARSUFFIX=$1])
dnl
dnl Modifies $build_<name> variable according to $enable_<name> and
dnl ability compile AC_LANG_PROGRAM(<program_prologue>, <program_body>).
dnl
dnl The <desc> is description used for warning/error dnl message (e.g. "foo support").
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $5.

AC_DEFUN([UL_REQUIRES_COMPILE], [
  m4_define([suffix], m4_default([$5],$1))

  if test "x$[build_]suffix" != xno; then

    AC_MSG_CHECKING([$4])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([$2], [$3])],
	[AC_MSG_RESULT([yes])
	 [ul_haveprogram_]suffix=yes],
	[AC_MSG_RESULT([no])
	 [ul_haveprogram_]suffix=no])

    case $[enable_]suffix:$[ul_haveprogram_]suffix in #(
    no:*)
      [build_]suffix=no ;;
    yes:yes)
      [build_]suffix=yes ;;
    yes:*)
      AC_MSG_ERROR([$1 selected, but required $4 not available]);;
    check:yes)
      [build_]suffix=yes ;;
    check:*)
      AC_MSG_WARN([$4 not found; not building $1])
      [build_]suffix=no ;;
    esac
  fi
])


dnl UL_REQUIRES_PROGRAM(NAME, PROGVAR, PROGRAM, DESC, [VARSUFFIX=$1])
dnl
dnl Modifies $build_<name> variable according to $enable_<name> and
dnl ability compile AC_PATH_PROG().
dnl
dnl The <desc> is description used for warning/error dnl message (e.g. "foo support").
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $5.
AC_DEFUN([UL_REQUIRES_PROGRAM], [
  m4_define([suffix], m4_default([$5],$1))

  if test "x$[build_]suffix" != xno; then

    AC_PATH_PROG([$2], [$3])

    case $[enable_]suffix:x$$2 in #(
    no:*)
      [build_]suffix=no ;;
    yes:x)
      AC_MSG_ERROR([$1 selected, but required $3 not available]);;
    yes:x*)
      [build_]suffix=yes ;;
    check:x)
      AC_MSG_WARN([$3 not found; not building $4])
      [build_]suffix=no ;;
    check:x*)
      [build_]suffix=yes ;;
    esac
  fi
])

dnl
dnl UL_CONFLICTS_BUILD(NAME, ANOTHER, ANOTHERDESC, [VARSUFFIX=$1])
dnl
dnl - ends with error if $enable_<name> and $build_<another>
dnl   are both set to 'yes'
dnl - sets $build_<name> to 'no' if $build_<another> is 'yes' and
dnl   $enable_<name> is 'check' or 'no'
dnl
dnl The <havedesc> is description used for warning/error
dnl message (e.g. "function").
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $4.
dnl
AC_DEFUN([UL_CONFLICTS_BUILD], [
  m4_define([suffix], m4_default([$4],$1))

  if test "x$[build_]suffix" != xno; then
    case $[enable_]suffix:$[build_]$2 in #(
    no:*)
      [build_]suffix=no ;;
    check:yes)
      [build_]suffix=no ;;
    check:no)
      [build_]suffix=yes ;;
    yes:yes)
      AC_MSG_ERROR([$1 selected, but it conflicts with $3]);;
    esac
  fi
])


dnl UL_REQUIRES_BUILD(NAME, BUILDNAME, [VARSUFFIX=$1])
dnl
dnl Modifies $build_<name> variable according to $enable_<name> and $have_funcname.
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $3.
dnl
AC_DEFUN([UL_REQUIRES_BUILD], [
  m4_define([suffix], m4_default([$3],$1))

  if test "x$[build_]suffix" != xno; then
    case $[enable_]suffix:$[build_]$2 in #(
    no:*)
      [build_]suffix=no ;;
    yes:yes)
      [build_]suffix=yes ;;
    yes:*)
      AC_MSG_ERROR([$2 is needed to build $1]);;
    check:yes)
      [build_]suffix=yes ;;
    check:*)
      AC_MSG_WARN([$2 disabled; not building $1])
      [build_]suffix=no ;;
    esac
  fi
])

dnl UL_REQUIRES_SYSCALL_CHECK(NAME, SYSCALL-TEST, [SYSCALLNAME=$1], [VARSUFFIX=$1])
dnl
dnl Modifies $build_<name> variable according to $enable_<name> and SYSCALL-TEST
dnl result. The $enable_<name> variable could be "yes", "no" and "check". If build_<name>
dnl is "no" then all checks are skipped.
dnl
dnl Note that SYSCALL-TEST has to define $ul_cv_syscall_<name> variable, see
dnl also UL_CHECK_SYSCALL().
dnl
dnl The default <name> for $build_ and $enable_ count be overwrited by option $4 and
dnl $ul_cv_syscall_ could be overwrited by $3.
dnl
AC_DEFUN([UL_REQUIRES_SYSCALL_CHECK], [
  m4_define([suffix], m4_default([$4],$1))
  m4_define([callname], m4_default([$3],$1))

  if test "x$[build_]suffix" != xno; then
    if test "x$[enable_]suffix" = xno; then
      [build_]suffix=no
    else
      $2
      case $[enable_]suffix:$[ul_cv_syscall_]callname in #(
      no:*)
        [build_]suffix=no ;;
      yes:no)
        AC_MSG_ERROR([$1 selected but callname syscall not found]) ;;
      check:no)
        AC_MSG_WARN([callname syscall not found; not building $1])
        [build_]suffix=no ;;
      *)
        dnl default $ul_cv_syscall_ is SYS_ value
        [build_]suffix=yes ;;
      esac
    fi
  fi
])

dnl UL_BUILD_INIT(NAME, [ENABLE_STATE], [VARSUFFIX = $1])
dnl
dnl Initializes $build_<name>  variable according to $enable_<name>. If
dnl $enable_<name> is undefined then ENABLE_STATE is used and $enable_<name> is
dnl set to ENABLE_STATE.
dnl
dnl The default <name> for $build_ and $enable_ could be overwrited by option $3.
dnl
AC_DEFUN([UL_BUILD_INIT], [
  m4_define([suffix], m4_default([$3],$1))
  m4_ifblank([$2],
[if test "x$enable_[]suffix" = xno; then
   build_[]suffix=no
else
   build_[]suffix=yes
fi],
[if test "x$ul_default_estate" != x; then
  enable_[]suffix=$ul_default_estate
  build_[]suffix=yes
  if test "x$ul_default_estate" = xno; then
    build_[]suffix=no
  fi
else[]
  ifelse(
      [$2], [check],[
  build_[]suffix=yes
  enable_[]suffix=check],
      [$2], [yes],[
  build_[]suffix=yes
  enable_[]suffix=yes],
      [$2], [no], [
  build_[]suffix=no
  enable_[]suffix=no])
fi])
])

dnl UL_DEFAULT_ENABLE(NAME, ENABLE_STATE)
dnl
dnl Initializes $enable_<name>  variable according to ENABLE_STATE. The default
dnl setting is possible to override by global $ul_default_estate.
dnl
AC_DEFUN([UL_DEFAULT_ENABLE], [
  m4_define([suffix], $1)
  if test "x$ul_default_estate" != x; then
    enable_[]suffix=$ul_default_estate
  else
    enable_[]suffix=$2
  fi
])


dnl UL_ENABLE_ALIAS(NAME, MASTERNAME)
dnl
dnl Initializes $enable_<name> variable according to $enable_<mastername>. This
dnl is useful for example if you want to use one --enable-mastername option
dnl for group of programs.
dnl
AC_DEFUN([UL_ENABLE_ALIAS], [
  m4_define([suffix], $1)
  m4_define([mastersuffix], $2)

  enable_[]suffix=$enable_[]mastersuffix
])


dnl UL_NCURSES_CHECK(NAME)
dnl
dnl Initializes $have_<name>, NCURSES_LIBS and NCURSES_CFLAGS variables according to
dnl <name>{6,5}_config output.
dnl
dnl The expected <name> is ncurses or ncursesw.
dnl
AC_DEFUN([UL_NCURSES_CHECK], [
  m4_define([suffix], $1)
  m4_define([SUFFIX], m4_toupper($1))

  # ncurses6-config
  #
  AS_IF([test "x$have_[]suffix" = xno], [
    AC_CHECK_TOOL(SUFFIX[]6_CONFIG, suffix[]6-config)
    if AC_RUN_LOG([$SUFFIX[]6_CONFIG --version >/dev/null]); then
      have_[]suffix=yes
      NCURSES_LIBS=`$SUFFIX[]6_CONFIG --libs`
      NCURSES_CFLAGS=`$SUFFIX[]6_CONFIG --cflags`
    else
      have_[]suffix=no
    fi
  ])

  # ncurses5-config
  #
  AS_IF([test "x$have_[]suffix" = xno], [
    AC_CHECK_TOOL(SUFFIX[]5_CONFIG, suffix[]5-config)
    if AC_RUN_LOG([$SUFFIX[]5_CONFIG --version >/dev/null]); then
      have_[]suffix=yes
      NCURSES_LIBS=`$SUFFIX[]5_CONFIG --libs`
      NCURSES_CFLAGS=`$SUFFIX[]5_CONFIG --cflags`
    else
      have_[]suffix=no
    fi
  ])

  # pkg-config (not supported by ncurses upstream by default)
  #
  AS_IF([test "x$have_[]suffix" = xno], [
    PKG_CHECK_MODULES(SUFFIX, [$1], [
      have_[]suffix=yes
      NCURSES_LIBS=${SUFFIX[]_LIBS}
      NCURSES_CFLAGS=${SUFFIX[]_CFLAGS}
    ],[have_[]suffix=no])
  ])

  # classic autoconf way
  #
  AS_IF([test "x$have_[]suffix" = xno], [
    AC_CHECK_LIB([$1], [initscr], [have_[]suffix=yes], [have_[]suffix=no])
    AS_IF([test "x$have_[]suffix" = xyes], [NCURSES_LIBS="-l[]suffix"])
  ])
])

dnl
dnl UL_TINFO_CHECK(NAME)
dnl
dnl Initializes $have_<name>, TINFO_LIBS and TINFO_CFLAGS variables.
dnl
dnl The expected <name> is tinfow or tinfo.
dnl
AC_DEFUN([UL_TINFO_CHECK], [
  m4_define([suffix], $1)
  m4_define([SUFFIX], m4_toupper($1))

  PKG_CHECK_MODULES(SUFFIX, [$1], [
    dnl pkg-config success
    have_[]suffix=yes
    TINFO_LIBS=${SUFFIX[]_LIBS}
    TINFO_CFLAGS=${SUFFIX[]_CFLAGS}
    UL_PKG_STATIC([TINFO_LIBS_STATIC], [$1])
  ],[
    dnl If pkg-config failed, fall back to classic searching.
    AC_CHECK_LIB([$1], [tgetent], [
       have_[]suffix=yes
       TINFO_LIBS="-l[]suffix"
       TINFO_LIBS_STATIC="-l[]suffix"
       TINFO_CFLAGS=""])
  ])
])
