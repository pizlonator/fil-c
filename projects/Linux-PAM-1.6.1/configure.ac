dnl Process this file with autoconf to produce a configure script.
AC_INIT([Linux-PAM],[1.6.1],[],[Linux-PAM])
AC_CONFIG_SRCDIR([conf/pam_conv1/pam_conv_y.y])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Wno-portability])
AC_PREREQ([2.61])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST

AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

dnl
dnl By default, everything under PAM is installed below /usr.
dnl
AC_PREFIX_DEFAULT(/usr)

dnl and some hacks to use /etc and /lib
test "${prefix}" = "NONE" && prefix="/usr"
if test ${prefix} = '/usr'
then
dnl If we use /usr as prefix, use /etc for config files
        if test ${sysconfdir} = '${prefix}/etc'
        then
                sysconfdir="/etc"
        fi
	if test ${libdir} = '${exec_prefix}/lib'
	then
		case "$host_cpu" in
		    x86_64|ppc64|s390x|sparc64)
			libdir="/lib64" ;;
		    *)
			libdir="/lib" ;;
		esac
	fi
	if test ${sbindir} = '${exec_prefix}/sbin'
        then
                sbindir="/sbin"
        fi
dnl If we use /usr as prefix, use /usr/share/man for manual pages
        if test ${mandir} = '${prefix}/man'
        then
                mandir='${prefix}/share/man'
        fi
dnl Add security to include directory
	if test ${includedir} = '${prefix}/include'
	then
		includedir="${prefix}/include/security"
	fi

dnl Add /var directory
        if test ${localstatedir} = '${prefix}/var'
        then
                localstatedir="/var"
        fi

fi

dnl This should be called before any macros that run the C compiler.
AC_USE_SYSTEM_EXTENSIONS

LT_INIT([disable-static])
AC_ENABLE_STATIC([no])
AC_ENABLE_SHARED([yes])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_YACC
AM_PROG_LEX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
PAM_LD_AS_NEEDED
PAM_LD_NO_UNDEFINED
PAM_LD_O1
PAM_LD_Z_NOW

dnl Largefile support
AC_SYS_LARGEFILE

pam_WARN_LANG_FLAGS

if test "x${CC_FOR_BUILD+set}" != "xset" ; then
  if test "x$cross_compiling" = "xyes" ; then
    AC_CHECK_PROGS(CC_FOR_BUILD, gcc cc)
  else
    CC_FOR_BUILD=${CC}
  fi
fi
AC_MSG_CHECKING([for CC_FOR_BUILD])
AC_MSG_RESULT([$CC_FOR_BUILD])
AC_SUBST(CC_FOR_BUILD)

if test "x${BUILD_CPPFLAGS+set}" != "xset" ; then
  if test "x$cross_compiling" = "xyes" ; then
    BUILD_CPPFLAGS=
  else
    BUILD_CPPFLAGS=${CPPFLAGS}
  fi
fi
AC_SUBST(BUILD_CPPFLAGS)

if test "x${BUILD_CFLAGS+set}" != "xset" ; then
  if test "x$cross_compiling" = "xyes" ; then
    BUILD_CFLAGS=
  else
    BUILD_CFLAGS=${CFLAGS}
  fi
fi
AC_SUBST(BUILD_CFLAGS)

if test "x${BUILD_LDFLAGS+set}" != "xset" ; then
  if test "x$cross_compiling" = "xyes" ; then
    BUILD_LDFLAGS=
  else
    BUILD_LDFLAGS=${LDFLAGS}
  fi
fi
AC_SUBST(BUILD_LDFLAGS)

PAM_ATTRIBUTE_UNUSED

dnl
dnl Check if --version-script is supported by ld
dnl
AC_CACHE_CHECK(for .symver assembler directive, libc_cv_asm_symver_directive,
[cat > conftest.s <<EOF
${libc_cv_dot_text}
_sym:
.symver _sym,sym@VERS
EOF
if ${CC-cc} -c $ASFLAGS conftest.s 1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD; then
  libc_cv_asm_symver_directive=yes
else
  libc_cv_asm_symver_directive=no
fi
rm -f conftest*])
AC_CACHE_CHECK(for ld --version-script, libc_cv_ld_version_script_option, [dnl
if test $libc_cv_asm_symver_directive = yes; then
  cat > conftest.s <<EOF
${libc_cv_dot_text}
_sym:
.symver _sym,sym@VERS
EOF
  cat > conftest.map <<EOF
VERS_1 {
        global: sym;
};

VERS_2 {
        global: sym;
} VERS_1;
EOF
  if ${CC-cc} -c $ASFLAGS conftest.s 1>&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD;
then
    if AC_TRY_COMMAND([${CC-cc} $CFLAGS $LDFLAGS -shared
                                -o conftest.so conftest.o
                                -nostartfiles -nostdlib
                                -Wl,--version-script,conftest.map
                       1>&AS_MESSAGE_LOG_FD]);
    then
      libc_cv_ld_version_script_option=yes
    else
      libc_cv_ld_version_script_option=no
    fi
  else
    libc_cv_ld_version_script_option=no
  fi
else
  libc_cv_ld_version_script_option=no
fi
rm -f conftest*])
AM_CONDITIONAL([HAVE_VERSIONING],
	[test "$libc_cv_ld_version_script_option" = "yes"])

AC_ARG_ENABLE([pie],
              [AS_HELP_STRING([--disable-pie],
                              [disable position-independent executables (PIE)])],
              [], [enable_pie=check])
case "$enable_pie" in
  no) ;;
  yes|check)
    dnl Check for -fpie/-pie support
    AC_CACHE_CHECK([for -fpie/-pie support],
                   [pam_cv_pie],
                   [saved_CFLAGS="$CFLAGS"
                    saved_LDFLAGS="$LDFLAGS"
                    CFLAGS="$CFLAGS -fpie"
                    LDFLAGS="$LDFLAGS -pie"
                    AC_LINK_IFELSE([AC_LANG_PROGRAM(,)],
                                   [pam_cv_pie=yes],
                                   [pam_cv_pie=no])
                    LDFLAGS="$saved_LDFLAGS"
                    CFLAGS="$saved_CFLAGS"])
    AS_IF([test "$pam_cv_pie" = yes],
          [enable_pie=yes],
          [AS_IF([test "$enable_pie" = yes],
                 [AC_MSG_FAILURE([requested -fpie/-pie support not found])],
                 [enable_pie=no])])
    ;;
  *) AC_MSG_ERROR([bad value $enable_pie for --enable-pie option]) ;;
esac

AS_IF([test "$enable_pie" = yes],
      [PIE_CFLAGS="-fpie"
       PIE_LDFLAGS="-pie"],
      [PIE_CFLAGS=
       PIE_LDFLAGS=])

EXE_CFLAGS="$PIE_CFLAGS"
EXE_LDFLAGS="$PIE_LDFLAGS $ZNOW_LDFLAGS"
AC_SUBST(EXE_CFLAGS)
AC_SUBST(EXE_LDFLAGS)


dnl
dnl options and defaults
dnl

AC_ARG_ENABLE([doc],
        AS_HELP_STRING([--disable-doc],[Do not generate or install documentation]),
        WITH_DOC=$enableval, WITH_DOC=yes)
AM_CONDITIONAL([HAVE_DOC], [test "x$WITH_DOC" = "xyes"])

AC_ARG_ENABLE([examples],
        AS_HELP_STRING([--disable-examples],[Do not build the examples]),
        WITH_EXAMPLES=$enableval, WITH_EXAMPLES=yes)
AM_CONDITIONAL([HAVE_EXAMPLES], [test "x$WITH_EXAMPLES" = "xyes"])

AC_ARG_ENABLE([prelude],
	AS_HELP_STRING([--disable-prelude],[do not use prelude]),
	WITH_PRELUDE=$enableval, WITH_PRELUDE=check)
if test "$WITH_PRELUDE" != "no" ; then
  AM_PATH_LIBPRELUDE([0.9.0])
  if test "$LIBPRELUDE_CONFIG" != "no" ; then
    LIBPRELUDE_CFLAGS="$LIBPRELUDE_CFLAGS -DPRELUDE=1"
  elif test "x$WITH_PRELUDE" = "xyes" ; then
    AC_MSG_ERROR([libprelude not found])
  fi
fi

dnl lots of debugging information goes to /var/run/pam-debug.log
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug],[specify you are building with debugging on]))

if test x"$enable_debug" = x"yes" ; then
   AC_DEFINE([PAM_DEBUG],,
		[lots of stuff gets written to /var/run/pam-debug.log])
fi
AC_DEFINE([PAM_NO_HEADER_FUNCTIONS],1,[disable function bodies in headers])

AC_ARG_ENABLE(docbook_rng,
	AS_HELP_STRING([--enable-docbook-rng=FILE],[RNG file for checking XML files @<:@default=http://docbook.org/xml/5.0/rng/docbookxi.rng@:>@]),
	DOCBOOK_RNG=$enableval, DOCBOOK_RNG=http://docbook.org/xml/5.0/rng/docbookxi.rng)
AC_SUBST(DOCBOOK_RNG)

AC_ARG_ENABLE(html_stylesheet,
	AS_HELP_STRING([--enable-html-stylesheet=FILE],[html stylesheet path @<:@default=http://docbook.sourceforge.net/release/xsl-ns/current/html/chunk.xsl@:>@]),
	HTML_STYLESHEET=$enableval, HTML_STYLESHEET=http://docbook.sourceforge.net/release/xsl-ns/current/html/chunk.xsl)
AC_SUBST(HTML_STYLESHEET)

AC_ARG_ENABLE(txt_stylesheet,
	AS_HELP_STRING([--enable-txt-stylesheet=FILE],[text stylesheet path @<:@default=http://docbook.sourceforge.net/release/xsl-ns/current/html/docbook.xsl@:>@]),
	TXT_STYLESHEET=$enableval, TXT_STYLESHEET=http://docbook.sourceforge.net/release/xsl-ns/current/html/docbook.xsl)


${MKDIR_P} doc
AC_SUBST(TXT_STYLESHEET)
# It has to be TXT_STYLESHEET otherwise a html tree will be generated while generating all README files.
sed "s+HTML_STYLESHEET+$TXT_STYLESHEET+g" <$srcdir/doc/custom-html.xsl.in >doc/custom-html.xsl

AC_ARG_ENABLE(pdf_stylesheet,
	AS_HELP_STRING([--enable-pdf-stylesheet=FILE],[pdf stylesheet path @<:@default=http://docbook.sourceforge.net/release/xsl-ns/current/fo/docbook.xsl@:>@]),
	PDF_STYLESHEET=$enableval, PDF_STYLESHEET=http://docbook.sourceforge.net/release/xsl-ns/current/fo/docbook.xsl)
AC_SUBST(PDF_STYLESHEET)

AC_ARG_ENABLE(man_stylesheet,
	AS_HELP_STRING([--enable-man-stylesheet=FILE],[man stylesheet path @<:@default=http://docbook.sourceforge.net/release/xsl-ns/current/manpages/profile-docbook.xsl@:>@]),
	MAN_STYLESHEET=$enableval, MAN_STYLESHEET=http://docbook.sourceforge.net/release/xsl-ns/current/manpages/profile-docbook.xsl)


AC_SUBST(MAN_STYLESHEET)
sed "s+MAN_STYLESHEET+$MAN_STYLESHEET+g" <$srcdir/doc/custom-man.xsl.in >doc/custom-man.xsl

AC_ARG_ENABLE(securedir,
	AS_HELP_STRING([--enable-securedir=DIR],[path to location of PAMs @<:@default=$libdir/security@:>@]),
	SECUREDIR=$enableval, SECUREDIR=$libdir/security)
AC_SUBST(SECUREDIR)

AC_ARG_ENABLE([isadir],
	AS_HELP_STRING([--enable-isadir=DIR],[path to arch-specific module files @<:@default=../../(basename of $libdir)/security@:>@]),
ISA=$enableval,
ISA=../../`basename $libdir`/security)
unset mylibdirbase
AC_DEFINE_UNQUOTED(_PAM_ISA,"$ISA",[Define to the path, relative to SECUREDIR, where PAMs specific to this architecture can be found.])
AC_MSG_RESULT([Defining \$ISA to "$ISA"])

AC_ARG_ENABLE(sconfigdir,
	AS_HELP_STRING([--enable-sconfigdir=DIR],[path to module conf files @<:@default=$sysconfdir/security@:>@]),
	SCONFIGDIR=$enableval, SCONFIGDIR=$sysconfdir/security)
AC_DEFINE_UNQUOTED([SCONFIGDIR], ["$SCONFIGDIR"],
		   [Directory for PAM modules system configuration files])
AC_SUBST(SCONFIGDIR)

AC_ARG_ENABLE(pamlocking,
	AS_HELP_STRING([--enable-pamlocking],[configure libpam to observe a global authentication lock]))

if test x"$enable_pamlocking" = "xyes"; then
   AC_DEFINE([PAM_LOCKING],,
	[libpam should observe a global authentication lock])
fi

AC_ARG_ENABLE(read-both-confs,
	AS_HELP_STRING([--enable-read-both-confs],[read both /etc/pam.d and /etc/pam.conf files]))

if test x"$enable_read_both_confs" = "xyes"; then
   AC_DEFINE([PAM_READ_BOTH_CONFS],,
		[read both /etc/pam.d and /etc/pam.conf files])
fi

AC_ARG_ENABLE([lckpwdf],
	AS_HELP_STRING([--disable-lckpwdf],[do not use the lckpwdf function]),
	WITH_LCKPWDF=$enableval, WITH_LCKPWDF=yes)
if test "$WITH_LCKPWDF" = "yes" ; then
    AC_DEFINE([USE_LCKPWDF], 1,
		[Define to 1 if the lckpwdf function should be used])
fi

AC_CHECK_HEADERS(paths.h)
AC_ARG_WITH(mailspool,
[  --with-mailspool        path to mail spool directory
                          [default _PATH_MAILDIR if defined in paths.h, otherwise /var/spool/mail]],
with_mailspool=${withval})
if test x$with_mailspool != x ; then
	pam_mail_spool="\"$with_mailspool\""
else
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <paths.h>
#include <stdlib.h>
int main() {
#ifdef _PATH_MAILDIR
exit(0);
#else
exit(1);
#endif
}]])],[pam_mail_spool="_PATH_MAILDIR"],[pam_mail_spool="\"/var/spool/mail\""],[pam_mail_spool="\"/var/spool/mail\""])
fi
AC_DEFINE_UNQUOTED(PAM_PATH_MAILDIR, $pam_mail_spool,
	[Path where mails are stored])

AC_ARG_WITH(xauth,
[  --with-xauth            additional path to check for xauth when it is called from pam_xauth
                          [added to the default of /usr/X11R6/bin/xauth, /usr/bin/xauth, /usr/bin/X11/xauth]],
pam_xauth_path=${withval})
if test x$with_xauth = x ; then
	AC_PATH_PROG(pam_xauth_path, xauth)
dnl There is no sense in adding the first default path
	if test x$pam_xauth_path = x/usr/X11R6/bin/xauth ; then
		unset pam_xauth_path
	fi
fi

if test x$pam_xauth_path != x ; then
	AC_DEFINE_UNQUOTED(PAM_PATH_XAUTH, "$pam_xauth_path",
	[Additional path of xauth executable])
fi

dnl Checks for the existence of libdl - in musl its a part of libc
saved_LIBS="$LIBS"
AC_SEARCH_LIBS([dlopen], [dl])
LIBS="$saved_LIBS"
case "$ac_cv_search_dlopen" in
	no) AC_MSG_FAILURE([failed to find dlopen]) ;;
	-l*) LIBDL="$ac_cv_search_dlopen" ;;
	*) LIBDL= ;;
esac
AC_SUBST(LIBDL)

dnl Look for Linux Auditing library - see documentation
AC_ARG_ENABLE([audit],
        AS_HELP_STRING([--disable-audit],[do not enable audit support]),
        WITH_LIBAUDIT=$enableval, WITH_LIBAUDIT=check)
if test x"$WITH_LIBAUDIT" != xno ; then
        AC_CHECK_HEADER([libaudit.h],
              [AC_CHECK_LIB(audit, audit_log_acct_message, LIBAUDIT=-laudit, LIBAUDIT="")
	       AC_CHECK_TYPE([struct audit_tty_status],
		             [HAVE_AUDIT_TTY_STATUS=yes],
			     [HAVE_AUDIT_TTY_STATUS=""],
			     [#include <libaudit.h>])]
        )
        if test -n "$LIBAUDIT" && test "$ac_cv_header_libaudit_h" != "no" ; then
            AC_DEFINE([HAVE_LIBAUDIT], 1, [Define to 1 if audit support should be compiled in.])
        elif test x"$WITH_LIBAUDIT" = xyes ; then
            AC_MSG_ERROR([libaudit not found])
        fi
        if test -n "$HAVE_AUDIT_TTY_STATUS" ; then
            AC_CHECK_MEMBERS([struct audit_tty_status.log_passwd], [],
                            AC_MSG_WARN([audit_tty_status.log_passwd is not available.  The log_passwd option is disabled.]),
                            [[#include <libaudit.h>]])
        fi
else
	LIBAUDIT=""
fi
AC_SUBST(LIBAUDIT)

AC_CHECK_HEADERS(crypt.h)

BACKUP_LIBS=$LIBS
LIBCRYPT=""
PKG_CHECK_MODULES([CRYPT], [libcrypt], [
  CFLAGS="$CFLAGS $CRYPT_CFLAGS"
  CPPFLAGS="$CPPFLAGS $CRYPT_CFLAGS"
  LIBS="$LIBS $CRYPT_LIBS"
  LIBCRYPT="$CRYPT_LIBS"
], [
  AC_SEARCH_LIBS([crypt_gensalt_rn],[crypt])
  case "$ac_cv_search_crypt_gensalt_rn" in
	-l*) LIBCRYPT="$ac_cv_search_crypt_gensalt_rn" ;;
	no) AC_SEARCH_LIBS([crypt_r],[crypt])
		case "$ac_cv_search_crypt_r" in
		-l*) LIBCRYPT="$ac_cv_search_crypt_r" ;;
		no ) AC_SEARCH_LIBS([crypt],[crypt])
		case "$ac_cv_search_crypt" in
			-l*) LIBCRYPT="$ac_cv_search_crypt" ;;
		esac ;;
	esac ;;
  esac
])
AC_CHECK_FUNCS([crypt_r])
LIBS=$BACKUP_LIBS
AC_SUBST(LIBCRYPT)

AC_ARG_WITH([randomdev], AS_HELP_STRING([--with-randomdev=(<path>|yes|no)],[use specified random device instead of /dev/urandom or 'no' to disable]), opt_randomdev=$withval)
if test "$opt_randomdev" = yes || test -z "$opt_randomdev"; then
       opt_randomdev="/dev/urandom"
elif test "$opt_randomdev" = no; then
       opt_randomdev=
fi
if test -n "$opt_randomdev"; then
       AC_DEFINE_UNQUOTED(PAM_PATH_RANDOMDEV, "$opt_randomdev", [Random device path.])
fi

dnl check for libdb or gdbm or libndbm as fallback. Some libndbm compat
dnl libraries are unusable, so try libdb first.
AC_ARG_ENABLE([db],
        AS_HELP_STRING([--enable-db=(db|gdbm|ndbm|yes|no)],[Default behavior 'yes', which is to check for libdb first, followed by gdbm and ndbm. Use 'no' to disable db support.]),
        WITH_DB=$enableval, WITH_DB=yes)
AC_ARG_WITH([db-uniquename],
	AS_HELP_STRING([--with-db-uniquename=extension],[Unique name for db libraries and functions.]))
if test x"$WITH_DB" != xno ; then
        if test x"$WITH_DB" = xyes || test x"$WITH_DB" = xdb ; then
              old_libs=$LIBS
              LIBS="$LIBS -ldb$with_db_uniquename"
              AC_CHECK_FUNCS([db_create$with_db_uniquename db_create dbm_store$with_db_uniquename dbm_store],
                [LIBDB="-ldb$with_db_uniquename"; break])
              LIBS=$old_libs
        fi
        if test x"$WITH_DB" = xgdbm ; then
            AC_CHECK_LIB([gdbm],[gdbm_store], LIBDB="-lgdbm", LIBDB="")
            if test -n "$LIBDB" ; then
                AC_CHECK_HEADERS(gdbm.h)
            fi
        fi
        if test -z "$LIBDB" ; then
            AC_CHECK_LIB([ndbm],[dbm_store], LIBDB="-lndbm", LIBDB="")
            if test -n "$LIBDB" ; then
                AC_CHECK_HEADERS(ndbm.h)
            fi
        else
            AC_CHECK_HEADERS(db.h)
        fi
fi
AC_SUBST(LIBDB)

AC_ARG_ENABLE([nis],
        AS_HELP_STRING([--disable-nis], [Disable building NIS/YP support in pam_unix]),
        WITH_NIS=$enableval, WITH_NIS=check)

if test "$WITH_NIS" != "no" ; then
  old_CFLAGS=$CFLAGS
  old_CPPFLAGS=$CPPFLAGS
  old_LIBS=$LIBS

  dnl if there's libtirpc available, prefer that over the system
  dnl implementation.
  PKG_CHECK_MODULES([TIRPC], [libtirpc], [
    CFLAGS="$CFLAGS $TIRPC_CFLAGS"
    CPPFLAGS="$CPPFLAGS $TIRPC_CFLAGS"
    LIBS="$LIBS $TIRPC_LIBS"
  ], [:;])

  PKG_CHECK_MODULES([NSL], [libnsl], [],
    [AC_CHECK_LIB([nsl],[yp_match],[NSL_LIBS="-lnsl"],[NSL_LIBS=""])])
  CFLAGS="$CFLAGS $NSL_CFLAGS"
  CPPFLAGS="$CPPFLAGS $NSL_CFLAGS"
  LIBS="$LIBS $NSL_LIBS"

  AC_CHECK_FUNCS([yp_get_default_domain yp_bind yp_match yp_unbind])
  AC_CHECK_FUNCS([rpcb_getaddr])
  AC_CHECK_HEADER([rpc/rpc.h], , [enable_nis=no])
  AC_CHECK_HEADER([rpcsvc/ypclnt.h], , [enable_nis=no])
  AC_CHECK_HEADER([rpcsvc/yp_prot.h], , [enable_nis=no])
  AC_CHECK_DECLS([getrpcport], , , [
    #if HAVE_RPC_RPC_H
    # include <rpc/rpc.h>
    #endif
  ])

  CFLAGS="$old_CFLAGS"
  CPPFLAGS="$old_CPPFLAGS"
  LIBS="$old_LIBS"
fi

AC_SUBST([NIS_CFLAGS])
AC_SUBST([NIS_LIBS])
AM_CONDITIONAL([HAVE_NIS], [test "x$enable_nis" != "xno"])
if test "x$enable_nis" != "xno" ; then
   AC_DEFINE([HAVE_NIS], 1,
	     [Defines that NIS should be used])
elif test "$WITH_NIS" = "yes" ; then
    AC_MSG_ERROR([NIS/YP support not available])
fi

AC_ARG_ENABLE([usergroups],
  AS_HELP_STRING([--enable-usergroups], [sets the usergroups option default to enabled]),
  [WITH_USERGROUPS=$enableval], WITH_USERGROUPS=no)
if test "$WITH_USERGROUPS" = "yes" ; then
   AC_DEFINE([DEFAULT_USERGROUPS_SETTING], 1,
	     [Defines the value usergroups option should have by default])
else
   AC_DEFINE([DEFAULT_USERGROUPS_SETTING], 0,
	     [Defines the value usergroups option should have by default])
fi

AC_ARG_ENABLE([selinux],
        AS_HELP_STRING([--disable-selinux],[do not use SELinux]),
        WITH_SELINUX=$enableval, WITH_SELINUX=check)
if test "$WITH_SELINUX" != "no" ; then
  AC_CHECK_LIB([selinux],[getfilecon], LIBSELINUX="-lselinux", LIBSELINUX="")
else
  LIBSELINUX=""
fi
AM_CONDITIONAL([WITH_SELINUX], [test -n "$LIBSELINUX"])
AC_SUBST(LIBSELINUX)
if test -n "$LIBSELINUX" ; then
    AC_DEFINE([WITH_SELINUX], 1, [Defined if SE Linux support is compiled in])
    BACKUP_LIBS=$LIBS
    LIBS="$LIBS $LIBSELINUX"
    AC_CHECK_FUNCS(setkeycreatecon)
    AC_CHECK_FUNCS(getseuser)
    LIBS=$BACKUP_LIBS
elif test "$WITH_SELINUX" = "yes" ; then
    AC_MSG_ERROR([libselinux not found])
fi

LOGIND_CFLAGS=
SYSTEMD_LIBS=
AC_ARG_ENABLE([logind],
  AS_HELP_STRING([--disable-logind], [Disable logind support]),
  [WITH_LOGIND=$enableval], [WITH_LOGIND=check])
if test "$WITH_LOGIND" = "check"; then
  PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 254], [LOGIND_CFLAGS="-DUSE_LOGIND=1 $SYSTEMD_CFLAGS"], [:])
elif test "$WITH_LOGIND" = "yes"; then
  PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 254], [LOGIND_CFLAGS="-DUSE_LOGIND=1 $SYSTEMD_CFLAGS"], [])
fi
AC_SUBST([LOGIND_CFLAGS])
AC_SUBST([SYSTEMD_LIBS])

ECONF_CFLAGS=
ECONF_LIBS=
AC_ARG_ENABLE([econf],
  AS_HELP_STRING([--disable-econf], [do not use libeconf]),
  [WITH_ECONF=$enableval], [WITH_ECONF=check])
if test "$WITH_ECONF" = "check"; then
  PKG_CHECK_MODULES([ECONF], [libeconf >= 0.5.0], [ECONF_CFLAGS="-DUSE_ECONF=1 $ECONF_CFLAGS"], [:])
elif test "$WITH_ECONF" = "yes"; then
  PKG_CHECK_MODULES([ECONF], [libeconf >= 0.5.0], [ECONF_CFLAGS="-DUSE_ECONF=1 $ECONF_CFLAGS"], [])
fi
AC_SUBST([ECONF_CFLAGS])
AC_SUBST([ECONF_LIBS])

AC_ARG_ENABLE([vendordir],
  AS_HELP_STRING([--enable-vendordir=DIR], [Directory for distribution provided configuration files]),,[])
if test -n "$enable_vendordir"; then
  AC_DEFINE_UNQUOTED([VENDORDIR], ["$enable_vendordir"],
		     [Directory for distribution provided configuration files])
  AC_DEFINE_UNQUOTED([VENDOR_SCONFIGDIR], ["$enable_vendordir/security"],
		     [Directory for PAM modules distribution provided configuration files])
  if test "$WITH_ECONF" = "yes" ; then
    STRINGPARAM_VENDORDIR="--stringparam vendordir '$enable_vendordir'"
    profileconditions="with_vendordir;with_vendordir_and_with_econf"
  else
    STRINGPARAM_VENDORDIR="--stringparam vendordir '$enable_vendordir'"
    profileconditions="with_vendordir;with_vendordir_and_without_econf"
  fi
  VENDOR_SCONFIGDIR="$enable_vendordir/security"
else
  profileconditions="without_vendordir"
fi
AC_SUBST([STRINGPARAM_VENDORDIR])
AC_SUBST(VENDOR_SCONFIGDIR)
AM_CONDITIONAL([HAVE_VENDORDIR], [test -n "$enable_vendordir"])

AC_ARG_ENABLE([openssl],
  AS_HELP_STRING([--enable-openssl],[use OpenSSL crypto libraries]),
  [OPENSSL_ENABLED=$enableval], OPENSSL_ENABLED=no)
if test "$OPENSSL_ENABLED" = "yes" ; then
  AC_CHECK_LIB([crypto], [EVP_MAC_CTX_new],
  [CRYPTO_LIBS="-lcrypto"
   use_openssl=yes
   AC_DEFINE([WITH_OPENSSL], 1, [OpenSSL provides crypto algorithm for hmac])
   profileconditions+=";openssl_hmac"],
  [CRYPTO_LIBS=""
   profileconditions+=";no_openssl_hmac"])
fi
AC_SUBST([CRYPTO_LIBS])
AM_CONDITIONAL([COND_USE_OPENSSL], [test "x$use_openssl" = "xyes"])

STRINGPARAM_PROFILECONDITIONS="--stringparam profile.condition '$profileconditions'"
AC_SUBST([STRINGPARAM_PROFILECONDITIONS])

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT

dnl For module/pam_lastlog
AC_CHECK_HEADERS(lastlog.h utmp.h utmpx.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Checks for library functions.
AC_TYPE_GETGROUPS
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_CHECK_FUNCS(getdomainname lckpwdf)
AC_CHECK_FUNCS(getutent_r getpwnam_r getpwuid_r getgrnam_r getgrgid_r getspnam_r getmntent_r)
AC_CHECK_FUNCS(getgrouplist)
AC_CHECK_FUNCS(quotactl)
AC_CHECK_FUNCS(unshare)
AC_CHECK_FUNCS(innetgr)
AC_CHECK_FUNCS(explicit_bzero memset_explicit)
AC_CHECK_FUNCS([ruserok_af ruserok], [break])
AC_CHECK_FUNCS(close_range)

dnl For libpam/pam_delay and modules/pam_timestamp
AC_CHECK_HEADERS([sys/random.h])
dnl May require libbsd/libSystem on non-Linux platforms
AC_CHECK_FUNCS(getrandom)

AC_ARG_ENABLE([regenerate-docu],
  AS_HELP_STRING([--disable-regenerate-docu],[Don't re-build documentation from XML sources]),
  [enable_docu=$enableval], [enable_docu=yes])
dnl
dnl Check for xsltproc
dnl
AC_PATH_PROG([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
     enable_docu=no
fi
AC_PATH_PROG([XMLLINT], [xmllint],[/bin/true])
dnl check for DocBook RNG and stylesheets in the local catalog.
JH_CHECK_XML_CATALOG([http://docbook.org/xml/5.0/rng/docbookxi.rng],
                [DocBook XML RNG V5.0], [], enable_docu=no)
JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl-ns/current/manpages/docbook.xsl],
                [DocBook XSL Stylesheets], [], enable_docu=no)

AC_PATH_PROG([BROWSER], [w3m])
if test -n "$BROWSER"; then
     BROWSER="$BROWSER -T text/html -dump"
else
     AC_PATH_PROG([BROWSER], [elinks])
     if test -n "$BROWSER"; then
          BROWSER="$BROWSER -no-numbering -no-references -dump"
     else
          enable_docu=no
     fi
fi

AC_PATH_PROG([FO2PDF], [fop])

AM_CONDITIONAL(ENABLE_REGENERATE_MAN, test x$enable_docu != xno && test x$enable_doc != xno)
AM_CONDITIONAL(ENABLE_GENERATE_PDF, test -n "$FO2PDF")


AM_GNU_GETTEXT_VERSION([0.18.3])
AM_GNU_GETTEXT([external])
AC_CHECK_FUNCS(bindtextdomain)
AC_CHECK_FUNCS(dngettext)

AH_BOTTOM([#ifdef ENABLE_NLS
#include <libintl.h>
#define _(msgid) dgettext(PACKAGE, msgid)
#define N_(msgid) msgid
#else
#define _(msgid) (msgid)
#define N_(msgid) msgid
#endif /* ENABLE_NLS */])

dnl
dnl Check for the availability of the kernel key management facility
dnl - The pam_keyinit module only requires the syscalls, not the error codes
dnl
AC_CHECK_DECL(__NR_keyctl, [have_key_syscalls=1],[have_key_syscalls=0],[#include <sys/syscall.h>])

dnl
dnl Get values for default uid ranges in login.defs used in pam_usertype
dnl
AC_ARG_WITH([uidmin], AS_HELP_STRING([--with-uidmin=<number>],[default value for regular user min uid (1000)]), opt_uidmin=$withval)
test -n "$opt_uidmin" ||
          opt_uidmin=1000
AC_DEFINE_UNQUOTED(PAM_USERTYPE_UIDMIN, $opt_uidmin, [Minimum regular user uid.])

AC_ARG_WITH([kernel-overflow-uid], AS_HELP_STRING([--with-kernel-overflow-uid=<number>],[kernel overflow uid, default (uint16_t)-2=65534]), opt_kerneloverflowuid=$withval)
test -n "$opt_kerneloverflowuid" ||
          opt_kerneloverflowuid=65534
AC_DEFINE_UNQUOTED(PAM_USERTYPE_OVERFLOW_UID, $opt_kerneloverflowuid, [Kernel overflow uid.])

AC_ARG_WITH([systemdunitdir],
AS_HELP_STRING([--with-systemdunitdir=DIR], [path to systemd service directory]),
    [],
    [
    PKG_CHECK_EXISTS([systemd],
      [with_systemdunitdir=$($PKG_CONFIG --variable=systemdunitdir systemd)],
      [with_systemdunitdir='${prefix}/lib/systemd/system'])
    ])
AC_SUBST([systemdunitdir], [$with_systemdunitdir])

AC_ARG_ENABLE([unix],
              [AS_HELP_STRING([--disable-unix],
                              [do not build pam_unix module])],
              [], [enable_unix=yes])
case "$enable_unix" in
  yes|no) ;;
  *) AC_MSG_ERROR([bad value $enable_unix for --enable-unix option]) ;;
esac

AC_ARG_ENABLE([lastlog],
              [AS_HELP_STRING([--enable-lastlog],
                              [do build pam_lastlog module])],
              [], [enable_lastlog=no])
case "$enable_lastlog" in
  yes|check)
    BACKUP_LIBS=$LIBS
    LIBS="$LIBS -lutil"
    AC_CHECK_FUNCS([logwtmp])
    LIBS=$BACKUP_LIBS
    ;;
  no) ;;
  *) AC_MSG_ERROR([bad value $enable_lastlog for --enable-lastlog option]) ;;
esac

AC_ARG_WITH([misc-conv-bufsize],
AS_HELP_STRING([--with-misc-conv-bufsize=<number>],
    [Size of input buffer for libpam_misc's misc_conv() conversation function, default=4096]),
    [],
    [with_misc_conv_bufsize=4096])
AC_DEFINE_UNQUOTED(PAM_MISC_CONV_BUFSIZE, $with_misc_conv_bufsize, [libpam_misc misc_conv() buffer size.])

AM_CONDITIONAL([COND_BUILD_PAM_KEYINIT], [test "$have_key_syscalls" = 1])
AM_CONDITIONAL([COND_BUILD_PAM_LASTLOG], [test "$ac_cv_func_logwtmp" = yes])
AM_CONDITIONAL([COND_BUILD_PAM_NAMESPACE], [test "$ac_cv_func_unshare" = yes])
AM_CONDITIONAL([COND_BUILD_PAM_RHOSTS], [test "$ac_cv_func_ruserok_af" = yes || test "$ac_cv_func_ruserok" = yes])
AM_CONDITIONAL([COND_BUILD_PAM_SELINUX], [test -n "$LIBSELINUX"])
AM_CONDITIONAL([COND_BUILD_PAM_SEPERMIT], [test -n "$LIBSELINUX"])
AM_CONDITIONAL([COND_BUILD_PAM_SETQUOTA], [test "$ac_cv_func_quotactl" = yes])
AM_CONDITIONAL([COND_BUILD_PAM_TTY_AUDIT], [test "$HAVE_AUDIT_TTY_STATUS" = yes])
AM_CONDITIONAL([COND_BUILD_PAM_UNIX], [test "$enable_unix" = yes])
AM_CONDITIONAL([COND_BUILD_PAM_USERDB], [test -n "$LIBDB"])

dnl Files to be created from when we run configure
AC_CONFIG_FILES([Makefile libpam/Makefile libpamc/Makefile libpamc/test/Makefile \
	libpam_internal/Makefile libpam_misc/Makefile conf/Makefile \
	conf/pam_conv1/Makefile libpam/pam.pc libpam_misc/pam_misc.pc libpamc/pamc.pc \
	po/Makefile.in \
	Make.xml.rules \
	modules/Makefile \
	modules/pam_access/Makefile \
	modules/pam_canonicalize_user/Makefile \
	modules/pam_debug/Makefile modules/pam_deny/Makefile \
	modules/pam_echo/Makefile modules/pam_env/Makefile \
	modules/pam_faildelay/Makefile modules/pam_faillock/Makefile \
	modules/pam_filter/Makefile modules/pam_filter/upperLOWER/Makefile \
	modules/pam_ftp/Makefile modules/pam_group/Makefile \
	modules/pam_issue/Makefile modules/pam_keyinit/Makefile \
	modules/pam_lastlog/Makefile modules/pam_limits/Makefile \
	modules/pam_listfile/Makefile modules/pam_localuser/Makefile \
	modules/pam_loginuid/Makefile modules/pam_mail/Makefile \
	modules/pam_mkhomedir/Makefile modules/pam_motd/Makefile \
	modules/pam_namespace/Makefile \
	modules/pam_namespace/pam_namespace_helper modules/pam_namespace/pam_namespace.service \
	modules/pam_nologin/Makefile modules/pam_permit/Makefile \
	modules/pam_pwhistory/Makefile modules/pam_rhosts/Makefile \
	modules/pam_rootok/Makefile modules/pam_exec/Makefile \
	modules/pam_securetty/Makefile modules/pam_selinux/Makefile \
	modules/pam_sepermit/Makefile modules/pam_setquota/Makefile \
	modules/pam_shells/Makefile modules/pam_stress/Makefile \
	modules/pam_succeed_if/Makefile modules/pam_time/Makefile \
	modules/pam_timestamp/Makefile modules/pam_tty_audit/Makefile \
	modules/pam_umask/Makefile \
	modules/pam_unix/Makefile modules/pam_userdb/Makefile \
	modules/pam_usertype/Makefile \
	modules/pam_warn/Makefile modules/pam_wheel/Makefile \
	modules/pam_xauth/Makefile doc/Makefile doc/specs/Makefile \
	doc/man/Makefile doc/sag/Makefile doc/adg/Makefile \
	doc/mwg/Makefile examples/Makefile tests/Makefile \
	xtests/Makefile])
AC_OUTPUT
