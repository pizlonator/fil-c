# Written by Joachim Wiberg <troglobit@gmail.com> and put in the public domain.
AC_INIT([Mg], [3.7], [https://github.com/troglobit/mg/issues], [mg])
AC_CONFIG_AUX_DIR(build-aux)
AM_INIT_AUTOMAKE([1.11 foreign])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([Makefile doc/Makefile src/Makefile])

AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_HEADERS([pty.h utmp.h])
AC_CANONICAL_HOST

# Check build host, DragonFly BSD uses priv
case $host_os in
    *dragonfly*)
        LIBS="$LIBS -L/usr/lib/priv"
        CPPFLAGS="$CPPFLAGS -I/usr/include/priv/ncurses"
        ;;
    *netbsd*)
        CPPFLAGS="$CPPFLAGS -D_OPENBSD_SOURCE"
        ;;
    *solaris*)
	LIBS="-lsocket -lnsl"
	CPPFLAGS="-DBSD_COMP -D__EXTENSIONS__"
	;;
    *)
        ;;
esac

# Check for library functions
AC_CHECK_LIB([util], [fparseln])
AC_CHECK_LIB([util], [login_tty])
AC_CHECK_LIB([util], [openpty])

# Check for usually missing API's, which we can replace
AC_REPLACE_FUNCS([fparseln futimens login_tty openpty reallocarray strlcpy strlcat strtonum])
AC_CONFIG_LIBOBJ_DIR([lib])

# Check for configured features
AC_ARG_ENABLE(size-optimizations,
	AS_HELP_STRING([--enable-size-optimizations], [Optimize for size, try real hard]))

AC_ARG_ENABLE(autoexec, AS_HELP_STRING([--disable-autoexec], [Disable auto-execute support]))
AC_ARG_ENABLE(cmode,    AS_HELP_STRING([--disable-cmode],    [Disable C-mode support]))
AC_ARG_ENABLE(compile,  AS_HELP_STRING([--disable-compile],  [Disable C compile & grep mode, used by C-mode]))
AC_ARG_ENABLE(cscope,   AS_HELP_STRING([--disable-cscope],   [Disable Cscope support]))
AC_ARG_ENABLE(ctags,    AS_HELP_STRING([--disable-ctags],    [Disable ctags(1) support, required by Cscope]))
AC_ARG_ENABLE(dired,    AS_HELP_STRING([--disable-dired],    [Disable directory editor]))
AC_ARG_ENABLE(regexp,   AS_HELP_STRING([--disable-regexp],   [Disable full regexp search]))
AC_ARG_ENABLE(togglenl, AS_HELP_STRING([--disable-togglenl], [Disable toggle-newline-prompt extension]))
AC_ARG_ENABLE(all,      AS_HELP_STRING([--disable-all],      [Disable all optional features]))

AC_ARG_WITH(startup,
        AS_HELP_STRING([--with-startup=FILE], [If ~/.mg is missing, default: $sysconfdir/mg]),
	[startup=$withval], [startup="$sysconfdir/mg"])

AC_ARG_WITH(mglog,
	AS_HELP_STRING([--with-mglog], [Enable debugging to log file, default: ./log/*.log]),
	[mglog=$withval], [mglog='no'])

AC_ARG_WITH(docs,
     AS_HELP_STRING([--without-docs], [Skip installation of documenation & manual]),
     [with_docs=$withval], [with_docs='yes'])

AC_ARG_WITH(curses,
     AS_HELP_STRING([--without-curses], [Build without curses/termcap, default: auto]),
     [with_curses=$withval], [with_curses='yes'])

AC_ARG_WITH(tutorial,
     AS_HELP_STRING([--without-tutorial], [Skip installation of $datadir/mg/tutorial.gz]),
     [with_tutorial=$withval], [with_tutorial='yes'])

# Check for a termcap compatible library
AS_IF([test "x$with_curses" = "xyes" -o "x$with_curses" = "xauto"], [
	AC_CHECK_LIB(ncurses, tgoto, , [
		AC_CHECK_LIB(curses, tgoto, , [
			AC_CHECK_LIB(tinfo, tgoto, , [
				AC_CHECK_LIB(termcap, tgoto,, [have_tcap=no])
			])
		])
	])
])

AS_IF([test "x$with_curses" != "xno"], [
	AS_IF([test "x$have_tcap" = "xno" ], [
		with_curses="no"
		curses_status="(not installed)"
		AC_DEFINE([WITHOUT_CURSES], [1], [Fallback since termcap/curses are not installed])
	], [
		# A termcap lib with tgoto() exists, now check for term.h, not
		# termcap.h, because reasons ... on Solaris we need to check for
		# ncurses/curses.h first, because term.h is not standalone.
	        AC_CHECK_HEADERS([ncurses/curses.h],, [
			AC_CHECK_HEADERS([term.h],, [
				AC_MSG_ERROR([Cannot find term.h, install Ncurses, or build --without-curses])
			])
		])
	])
], [
	AC_DEFINE([WITHOUT_CURSES], [1], [Set when built without curses/ncurses])
])

# If all features are to be disabled, do that here
AS_IF([test "x$enable_all" = "xno"],
	enable_autoexec="no"
	enable_cmode="no"
	enable_compile="no"
	enable_cscope="no"
	enable_ctags="no"
	enable_dired="no"
	enable_regexp="no"
	enable_togglenl="no"
	startup="no")

# Enable features
AS_IF([test "x$enable_size_optimizations" = "xyes"],
	enable_tiny="yes"
	tiny_help="(try upx for reducing size even more)"
	CFLAGS="-g -Os"
	AC_DEFINE(TINY, 1, [Enable extreme size optimizations]),
	enable_tiny="no")

AS_IF([test "x$enable_autoexec" != "xno"], enable_autoexec="yes"
	AC_DEFINE(ENABLE_AUTOEXEC, 1, [Enable auto-exec support]))

AS_IF([test "x$enable_cmode" != "xno"], enable_cmode="yes"
	AC_DEFINE(ENABLE_CMODE, 1, [Enable C programming mode, OpenBSD KNF]))

AS_IF([test "x$enable_compile" != "xno"], enable_compile="yes"
	AC_DEFINE(ENABLE_COMPILE_GREP, 1, [Enable C compile mode, M-x compile]))

AS_IF([test "x$enable_cscope" != "xno"], enable_cscope="yes"
	enable_ctags="yes"
	AC_DEFINE(CSCOPE, 1, [Enable Cscope programming tool]))

AS_IF([test "x$enable_ctags" != "xno"], enable_ctags="yes"
	AC_DEFINE(ENABLE_CTAGS, 1, [Enable ctags(1) support]))

AS_IF([test "x$enable_dired" != "xno"], enable_dired="yes"
	AC_DEFINE(ENABLE_DIRED, 1, [Enable dired, the directory editor]))

AS_IF([test "x$enable_regexp" != "xno"], enable_regexp="yes"
	AC_DEFINE(REGEX, 1, [Enable regexp search support]))

AS_IF([test "x$enable_togglenl" != "xno"], enable_togglenl="yes"
	AC_DEFINE(TOGGLENL, 1, [Enable toggle-newline-prompt support]))

AS_IF([test "x$startup" != "xno"],[
	AS_IF([test "x$startup" = "xyes"],[
		AC_MSG_ERROR([Must supply argument])])]
	AC_DEFINE_UNQUOTED(STARTUPFILE, "$startup", [Init file to run if ~/.mg is missing]))

AS_IF([test "x$mglog" != "xno"],
	AC_DEFINE_UNQUOTED(MGLOG, "$mglog", [Debug logging of mg internals]))

# Control build with automake flags
AM_CONDITIONAL(REGEX,    [test "x$enable_regexp"   = "xyes"])
AM_CONDITIONAL(AUTOEXEC, [test "x$enable_autoexec" = "xyes"])
AM_CONDITIONAL(CMODE,    [test "x$enable_cmode"    = "xyes"])
AM_CONDITIONAL(COMPILE,  [test "x$enable_compile"  = "xyes"])
AM_CONDITIONAL(CSCOPE,   [test "x$enable_cscope"   = "xyes"])
AM_CONDITIONAL(CTAGS,    [test "x$enable_ctags"    = "xyes"])
AM_CONDITIONAL(DIRED,    [test "x$enable_dired"    = "xyes"])
AM_CONDITIONAL(TINY,     [test "x$enable_tiny"     = "xyes"])
AM_CONDITIONAL(MGLOG,    [test "x$mglog"          != "xno"])
AM_CONDITIONAL(DOCS,     [test "x$with_docs"      != "xno"])
AM_CONDITIONAL(NOCURSES, [test "x$with_curses"     = "xno"])
AM_CONDITIONAL(TUTOR,    [test "x$with_tutorial"  != "xno"])

# Generate all files
AC_OUTPUT

# Expand directories for configuration summary, unexpanded defaults:
# sysconfdir  => ${prefix}/etc
# runstatedir => ${localstatedir}/run
AS_IF([test "x$prefix" = "x" ],	[PREFIX="/"], [PREFIX=`eval echo $prefix`])
datadir=$datadir/mg
SYSCONFDIR=`eval echo $sysconfdir`
DATADIR=`eval echo $datadir`
DOCDIR=`eval echo $docdir`
STARTUP=`eval echo $startup`

cat <<EOF

------------------ Summary ------------------
 $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix.........: $PREFIX
  Sysconfdir.....: $SYSCONFDIR
  Datadir........: $DATADIR
  Docdir.........: $DOCDIR
  Fallback ~/.mg.: $STARTUP
  C Compiler.....: $CC $CFLAGS $CPPFLAGS $LDFLAGS $LIBS
  Size optimize..: $enable_tiny $tiny_help
  Termcap/curses.: $with_curses $curses_status

 Optional features:
  MGLOG..........: $with_mglog
  doc/ & man/....: $with_docs
  tutorial.gz....: $with_tutorial
  autoexec.......: $enable_autoexec
  cmode..........: $enable_cmode
  compile mode...: $enable_compile
  cscope.........: $enable_cscope
  ctags..........: $enable_ctags
  dired..........: $enable_dired
  regexp.........: $enable_regexp

------------- Compiler version --------------
$($CC --version || true)
---------------------------------------------

Check the above options and compile with:
 ${MAKE-make}

EOF
