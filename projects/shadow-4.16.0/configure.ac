dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
m4_define([libsubid_abi_major], 5)
m4_define([libsubid_abi_minor], 0)
m4_define([libsubid_abi_micro], 0)
m4_define([libsubid_abi], [libsubid_abi_major.libsubid_abi_minor.libsubid_abi_micro])
AC_INIT([shadow], [4.16.0], [pkg-shadow-devel@lists.alioth.debian.org], [],
	[https://github.com/shadow-maint/shadow])
AM_INIT_AUTOMAKE([1.11 foreign dist-xz subdir-objects tar-pax])
AC_CONFIG_MACRO_DIRS([m4])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config.h])

AC_SUBST([LIBSUBID_ABI_MAJOR], [libsubid_abi_major])
AC_SUBST([LIBSUBID_ABI_MINOR], [libsubid_abi_minor])
AC_SUBST([LIBSUBID_ABI_MICRO], [libsubid_abi_micro])
AC_SUBST([LIBSUBID_ABI], [libsubid_abi])

dnl Some hacks...
test "$prefix" = "NONE" && prefix="/usr"
test "$prefix" = "/usr" && exec_prefix=""

AC_USE_SYSTEM_EXTENSIONS

AC_ENABLE_STATIC
AC_ENABLE_SHARED

AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_YACC
LT_INIT
LT_LIB_DLLOAD

dnl Checks for libraries.

dnl Checks for header files.
AC_CHECK_HEADERS(crypt.h utmp.h \
	termio.h sgtty.h sys/ioctl.h paths.h \
	sys/capability.h sys/random.h \
	gshadow.h lastlog.h rpc/key_prot.h acl/libacl.h \
	attr/libattr.h attr/error_context.h)

dnl shadow now uses the libc's shadow implementation
AC_CHECK_HEADER([shadow.h],,[AC_MSG_ERROR([You need a libc with shadow.h])])

AC_CHECK_FUNCS(arc4random_buf futimes \
	getentropy getrandom getspnam getusershell \
	initgroups lckpwdf lutimes \
	setgroups updwtmpx innetgr \
	getspnam_r \
	rpmatch \
	memset_explicit explicit_bzero stpecpy stpeprintf)
AC_SYS_LARGEFILE

dnl Checks for typedefs, structures, and compiler characteristics.

AC_CHECK_MEMBERS([struct utmpx.ut_name,
                  struct utmpx.ut_host,
                  struct utmpx.ut_syslen,
                  struct utmpx.ut_addr,
                  struct utmpx.ut_addr_v6,
                  struct utmpx.ut_time,
                  struct utmpx.ut_xtime],,,[[#include <utmpx.h>]])

dnl Checks for library functions.
AC_TYPE_GETGROUPS
AC_FUNC_UTIME_NULL
AC_REPLACE_FUNCS(putgrent putpwent putspent)
AC_REPLACE_FUNCS(sgetgrent sgetpwent sgetspent)

AC_CHECK_FUNC(setpgrp)
AC_CHECK_FUNC(secure_getenv, [AC_DEFINE(HAS_SECURE_GETENV,
                                        1,
                                        [Defined to 1 if you have the declaration of 'secure_getenv'])])

if test "$ac_cv_header_shadow_h" = "yes"; then
	AC_CACHE_CHECK(for working shadow group support,
		ac_cv_libc_shadowgrp,
		AC_RUN_IFELSE([AC_LANG_SOURCE([
				#include <shadow.h>
				#ifdef HAVE_GSHADOW_H
				#include <gshadow.h>
				#endif
				int
				main()
				{
					struct sgrp *sg = sgetsgent("test:x::");
					/* NYS libc on Red Hat 3.0.3 has broken shadow group support */
					return !sg || !sg->sg_adm || !sg->sg_mem;
				}]
			)],
			[ac_cv_libc_shadowgrp=yes],
			[ac_cv_libc_shadowgrp=no],
			[ac_cv_libc_shadowgrp=no]
		)
	)

	if test "$ac_cv_libc_shadowgrp" = "yes"; then
		AC_DEFINE(HAVE_SHADOWGRP, 1, [Have working shadow group support in libc])
	fi
fi

AC_CACHE_CHECK([location of shared mail directory], shadow_cv_maildir,
[for shadow_cv_maildir in /var/mail /var/spool/mail /usr/spool/mail /usr/mail none; do
	if test -d $shadow_cv_maildir; then
		break
	fi
done])
if test $shadow_cv_maildir != none; then
	AC_DEFINE_UNQUOTED(MAIL_SPOOL_DIR, "$shadow_cv_maildir",
		[Location of system mail spool directory.])
fi

AC_CACHE_CHECK([location of user mail file], shadow_cv_mailfile,
[for shadow_cv_mailfile in Mailbox mailbox Mail mail .mail none; do
	if test -f $HOME/$shadow_cv_mailfile; then
		break
	fi
done])
if test $shadow_cv_mailfile != none; then
	AC_DEFINE_UNQUOTED(MAIL_SPOOL_FILE, "$shadow_cv_mailfile",
		[Name of user's mail spool file if stored in user's home directory.])
fi

AC_CACHE_CHECK([location of utmp], shadow_cv_utmpdir,
[for shadow_cv_utmpdir in /var/run /var/adm /usr/adm /etc none; do
	if test -f $shadow_cv_utmpdir/utmp; then
		break
	fi
done])
if test "$shadow_cv_utmpdir" = "none"; then
	AC_MSG_WARN(utmp file not found)
fi
AC_DEFINE_UNQUOTED(_UTMP_FILE, "$shadow_cv_utmpdir/utmp",
	[Path for utmp file.])

AC_CACHE_CHECK([location of faillog/lastlog/wtmp], shadow_cv_logdir,
[for shadow_cv_logdir in /var/log /var/adm /usr/adm /etc; do
	if test -d $shadow_cv_logdir; then
		break
	fi
done])
AC_DEFINE_UNQUOTED(_WTMP_FILE, "$shadow_cv_logdir/wtmp",
	[Path for wtmp file.])
AC_DEFINE_UNQUOTED(LASTLOG_FILE, "$shadow_cv_logdir/lastlog",
	[Path for lastlog file.])
AC_DEFINE_UNQUOTED(FAILLOG_FILE, "$shadow_cv_logdir/faillog",
	[Path for faillog file.])

AC_CACHE_CHECK([location of the passwd program], shadow_cv_passwd_dir,
[if test -f /usr/bin/passwd; then
	shadow_cv_passwd_dir=/usr/bin
else
	shadow_cv_passwd_dir=/bin
fi])
AC_DEFINE_UNQUOTED(PASSWD_PROGRAM, "$shadow_cv_passwd_dir/passwd",
	[Path to passwd program.])

AC_ARG_ENABLE(shadowgrp,
	[AS_HELP_STRING([--enable-shadowgrp], [enable shadow group support @<:@default=yes@:>@])],
	[case "${enableval}" in
	 yes) enable_shadowgrp="yes" ;;
	  no) enable_shadowgrp="no" ;;
	   *) AC_MSG_ERROR(bad value ${enableval} for --enable-shadowgrp) ;;
	 esac],
	[enable_shadowgrp="yes"]
)

AC_ARG_ENABLE(man,
	[AS_HELP_STRING([--enable-man],
		[regenerate roff man pages from Docbook @<:@default=no@:>@])],
	[enable_man="${enableval}"],
	[enable_man="no"]
)

AC_ARG_ENABLE(account-tools-setuid,
	[AS_HELP_STRING([--enable-account-tools-setuid],
		[Install the user and group management tools setuid and authenticate the callers. This requires --with-libpam.])],
	[case "${enableval}" in
	 yes) enable_acct_tools_setuid="yes" ;;
	  no) enable_acct_tools_setuid="no" ;;
	   *) AC_MSG_ERROR(bad value ${enableval} for --enable-account-tools-setuid)
	   ;;
	 esac],
	[enable_acct_tools_setuid="no"]
)

AC_ARG_ENABLE(subordinate-ids,
	[AS_HELP_STRING([--enable-subordinate-ids],
		[support subordinate ids @<:@default=yes@:>@])],
	[enable_subids="${enableval}"],
	[enable_subids="maybe"]
)

AC_ARG_ENABLE(lastlog,
	[AS_HELP_STRING([--enable-lastlog],
		[enable lastlog @<:@default=no@:>@])],
	[enable_lastlog="${enableval}"],
	[enable_lastlog="no"]
)

AC_ARG_ENABLE(logind,
	[AS_HELP_STRING([--enable-logind],
		[enable logind @<:@default=yes@:>@])],
	[enable_logind="${enableval}"],
	[enable_logind="yes"]
)

AC_ARG_WITH(audit,
	[AS_HELP_STRING([--with-audit], [use auditing support @<:@default=yes if found@:>@])],
	[with_audit=$withval], [with_audit=maybe])
AC_ARG_WITH(libpam,
	[AS_HELP_STRING([--with-libpam], [use libpam for PAM support @<:@default=yes if found@:>@])],
	[with_libpam=$withval], [with_libpam=maybe])
AC_ARG_WITH(btrfs,
	[AS_HELP_STRING([--with-btrfs], [add BtrFS support @<:@default=yes if found@:>@])],
	[with_btrfs=$withval], [with_btrfs=maybe])
AC_ARG_WITH(selinux,
	[AS_HELP_STRING([--with-selinux], [use SELinux support @<:@default=yes if found@:>@])],
	[with_selinux=$withval], [with_selinux=maybe])
AC_ARG_WITH(acl,
	[AS_HELP_STRING([--with-acl], [use ACL support @<:@default=yes if found@:>@])],
	[with_acl=$withval], [with_acl=maybe])
AC_ARG_WITH(attr,
	[AS_HELP_STRING([--with-attr], [use Extended Attribute support @<:@default=yes if found@:>@])],
	[with_attr=$withval], [with_attr=maybe])
AC_ARG_WITH(skey,
	[AS_HELP_STRING([--with-skey], [use S/Key support @<:@default=no@:>@])],
	[with_skey=$withval], [with_skey=no])
AC_ARG_WITH(tcb,
	[AS_HELP_STRING([--with-tcb], [use tcb support (incomplete) @<:@default=yes if found@:>@])],
	[with_tcb=$withval], [with_tcb=maybe])
AC_ARG_WITH(sha-crypt,
	[AS_HELP_STRING([--with-sha-crypt], [allow the SHA256 and SHA512 password encryption algorithms @<:@default=yes@:>@])],
	[with_sha_crypt=$withval], [with_sha_crypt=yes])
AC_ARG_WITH(bcrypt,
	[AS_HELP_STRING([--with-bcrypt], [allow the bcrypt password encryption algorithm @<:@default=no@:>@])],
	[with_bcrypt=$withval], [with_bcrypt=no])
AC_ARG_WITH(yescrypt,
	[AS_HELP_STRING([--with-yescrypt], [allow the yescrypt password encryption algorithm @<:@default=no@:>@])],
	[with_yescrypt=$withval], [with_yescrypt=no])
AC_ARG_WITH(nscd,
	[AS_HELP_STRING([--with-nscd], [enable support for nscd @<:@default=yes@:>@])],
	[with_nscd=$withval], [with_nscd=yes])
AC_ARG_WITH(sssd,
	[AS_HELP_STRING([--with-sssd], [enable support for flushing sssd caches @<:@default=yes@:>@])],
	[with_sssd=$withval], [with_sssd=yes])
AC_ARG_WITH(group-name-max-length,
	[AS_HELP_STRING([--with-group-name-max-length], [set max group name length @<:@default=32@:>@])],
	[with_group_name_max_length=$withval], [with_group_name_max_length=yes])
AC_ARG_WITH(su,
	[AS_HELP_STRING([--with-su], [build and install su program and man page @<:@default=yes@:>@])],
	[with_su=$withval], [with_su=yes])
AC_ARG_WITH(libbsd,
	[AS_HELP_STRING([--with-libbsd], [use libbsd support @<:@default=yes if found@:>@])],
	[with_libbsd=$withval], [with_libbsd=yes])

if test "$with_group_name_max_length" = "no" ; then
	with_group_name_max_length=0
elif test "$with_group_name_max_length" = "yes" ; then
	with_group_name_max_length=32
fi
AC_DEFINE_UNQUOTED(GROUP_NAME_MAX_LENGTH, $with_group_name_max_length, [max group name length])
AC_SUBST(GROUP_NAME_MAX_LENGTH)
GROUP_NAME_MAX_LENGTH="$with_group_name_max_length"


AM_CONDITIONAL(USE_SHA_CRYPT, test "x$with_sha_crypt" = "xyes")
if test "$with_sha_crypt" = "yes"; then
	AC_DEFINE(USE_SHA_CRYPT, 1, [Define to allow the SHA256 and SHA512 password encryption algorithms])
fi

AM_CONDITIONAL(USE_BCRYPT, test "x$with_bcrypt" = "xyes")
if test "$with_bcrypt" = "yes"; then
	AC_DEFINE(USE_BCRYPT, 1, [Define to allow the bcrypt password encryption algorithm])
fi

AM_CONDITIONAL(USE_YESCRYPT, test "x$with_yescrypt" = "xyes")
if test "$with_yescrypt" = "yes"; then
	AC_DEFINE(USE_YESCRYPT, 1, [Define to allow the yescrypt password encryption algorithm])
fi

if test "$with_nscd" = "yes"; then
	AC_CHECK_FUNC(posix_spawn,
	              [AC_DEFINE(USE_NSCD, 1, [Define to support flushing of nscd caches])],
	              [AC_MSG_ERROR([posix_spawn is needed for nscd support])])
fi

if test "$with_sssd" = "yes"; then
	AC_CHECK_FUNC(posix_spawn,
	              [AC_DEFINE(USE_SSSD, 1, [Define to support flushing of sssd caches])],
	              [AC_MSG_ERROR([posix_spawn is needed for sssd support])])
fi

AS_IF([test "$with_su" != "no"], AC_DEFINE(WITH_SU, 1, [Build with su]))
AM_CONDITIONAL([WITH_SU], [test "x$with_su" != "xno"])

dnl Check for some functions in libc first, only if not found check for
dnl other libraries.  This should prevent linking libnsl if not really
dnl needed (Linux glibc, Irix), but still link it if needed (Solaris).

AC_SEARCH_LIBS(gethostbyname, nsl)

PKG_CHECK_MODULES([CMOCKA], [cmocka], [have_cmocka="yes"],
	[AC_MSG_WARN([libcmocka not found, cmocka tests will not be built])])
AM_CONDITIONAL([HAVE_CMOCKA], [test x$have_cmocka = xyes])

AC_CHECK_LIB([econf],[econf_readDirs],[LIBECONF="-leconf"],[LIBECONF=""])
if test -n "$LIBECONF"; then
        AC_DEFINE_UNQUOTED([VENDORDIR], ["$enable_vendordir"],
					[Directory for distribution provided configuration files])
	ECONF_CPPFLAGS="-DUSE_ECONF=1"
	AC_ARG_ENABLE([vendordir],
		AS_HELP_STRING([--enable-vendordir=DIR], [Directory for distribution provided configuration files]),,[])
fi
AC_SUBST(ECONF_CPPFLAGS)
AC_SUBST(LIBECONF)
AC_SUBST([VENDORDIR], [$enable_vendordir])
if test "x$enable_vendordir" != x; then
	AC_DEFINE(HAVE_VENDORDIR, 1, [Define to support vendor settings.])
fi
AM_CONDITIONAL([HAVE_VENDORDIR], [test "x$enable_vendordir" != x])

if test "$enable_shadowgrp" = "yes"; then
	AC_DEFINE(SHADOWGRP, 1, [Define to support the shadow group file.])
fi
AM_CONDITIONAL(SHADOWGRP, test "x$enable_shadowgrp" = "xyes")

if test "$enable_man" = "yes"; then
	dnl
	dnl Check for xsltproc
	dnl
	AC_PATH_PROG([XSLTPROC], [xsltproc])
	if test -z "$XSLTPROC"; then
		enable_man=no
		AC_MSG_ERROR([xsltproc is missing.])
	fi

	dnl check for DocBook DTD and stylesheets in the local catalog.
	JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.5//EN],
		[DocBook XML DTD V4.5], [], enable_man=no)
	JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
		[DocBook XSL Stylesheets >= 1.70.1], [], enable_man=no)
fi
AM_CONDITIONAL(ENABLE_REGENERATE_MAN, test "x$enable_man" != "xno")

if test "$enable_subids" != "no"; then
	dnl
	dnl FIXME: check if 32 bit UIDs/GIDs are supported by libc
	dnl
	AC_CHECK_SIZEOF([uid_t],, [#include "sys/types.h"])
	AC_CHECK_SIZEOF([gid_t],, [#include "sys/types.h"])

	if test "$ac_cv_sizeof_uid_t" -ge 4 && test "$ac_cv_sizeof_gid_t" -ge 4; then
		AC_DEFINE(ENABLE_SUBIDS, 1, [Define to support the subordinate IDs.])
		enable_subids="yes"
	else
		if test "x$enable_subids" = "xyes"; then
			AC_MSG_ERROR([Cannot enable support the subordinate IDs on systems where gid_t or uid_t has less than 32 bits])
		fi
		enable_subids="no"
	fi
fi
AM_CONDITIONAL(ENABLE_SUBIDS, test "x$enable_subids" != "xno")

if test "$enable_lastlog" = "yes" && test "$ac_cv_header_lastlog_h" = "yes"; then
	AC_CACHE_CHECK(for ll_host in struct lastlog,
		ac_cv_struct_lastlog_ll_host,
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <lastlog.h>],
					[struct lastlog ll; char *cp = ll.ll_host;]
				)],
			[ac_cv_struct_lastlog_ll_host=yes],
			[ac_cv_struct_lastlog_ll_host=no]
		)
	)

	if test "$ac_cv_struct_lastlog_ll_host" = "yes"; then
		AC_DEFINE(HAVE_LL_HOST, 1,
			[Define if struct lastlog has ll_host])
		AC_DEFINE(ENABLE_LASTLOG, 1, [Define to support lastlog.])
		enable_lastlog="yes"
	else
		AC_MSG_ERROR([Cannot enable support for lastlog on systems where the data structures aren't available])
		enable_subids="no"
	fi
fi
AM_CONDITIONAL(ENABLE_LASTLOG, test "x$enable_lastlog" != "xno")

AC_SUBST(LIBSYSTEMD)
if test "$enable_logind" = "yes"; then
	AC_CHECK_LIB(systemd, sd_session_get_remote_host,
		[enable_logind="yes"; [LIBSYSTEMD=-lsystemd];
		AC_DEFINE(ENABLE_LOGIND, 1,
			[Define to manage session support with logind.])],
		[enable_logind="no"])
fi
AM_CONDITIONAL(ENABLE_LOGIND, test "x$enable_logind" != "xno")

AC_SUBST(LIBCRYPT)
AC_CHECK_LIB(crypt, crypt, [LIBCRYPT=-lcrypt],
	[AC_MSG_ERROR([crypt() not found])])

AC_SUBST(LIYESCRYPT)
AC_CHECK_LIB(crypt, crypt, [LIYESCRYPT=-lcrypt],
	[AC_MSG_ERROR([crypt() not found])])

AC_SUBST(LIBBSD)
if test "$with_libbsd" != "no"; then
	AC_SEARCH_LIBS([readpassphrase], [bsd], [], [
		AC_MSG_ERROR([readpassphrase() is missing, either from libc or libbsd])
	])
	AS_IF([test "$ac_cv_search_readpassphrase" = "-lbsd"], [
		PKG_CHECK_MODULES([LIBBSD], [libbsd-overlay])
	])
	dnl Make sure either the libc or libbsd provide the header.
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $LIBBSD_CFLAGS"
	AC_CHECK_HEADERS([readpassphrase.h])
	AS_IF([test "$ac_cv_header_readpassphrase_h" != "yes"], [
		AC_MSG_ERROR([readpassphrase.h is missing])
	])
	CFLAGS="$save_CFLAGS"
	AC_DEFINE(WITH_LIBBSD, 1, [Build shadow with libbsd support])
else
	AC_DEFINE(WITH_LIBBSD, 0, [Build shadow without libbsd support])
fi
AM_CONDITIONAL(WITH_LIBBSD, test x$with_libbsd = xyes)

AC_SUBST(LIBACL)
if test "$with_acl" != "no"; then
	AC_CHECK_HEADERS(acl/libacl.h attr/error_context.h, [acl_header="yes"], [acl_header="no"])
	if test "$acl_header$with_acl" = "noyes" ; then
		AC_MSG_ERROR([acl/libacl.h or attr/error_context.h is missing])
	elif test "$acl_header" = "yes" ; then
		AC_CHECK_LIB(acl, perm_copy_file,
		             [AC_CHECK_LIB(acl, perm_copy_fd,
		                           [acl_lib="yes"],
		                           [acl_lib="no"])],
		             [acl_lib="no"])
		if test "$acl_lib$with_acl" = "noyes" ; then
			AC_MSG_ERROR([libacl not found])
		elif test "$acl_lib" = "no" ; then
			with_acl="no"
		else
			AC_DEFINE(WITH_ACL, 1,
			          [Build shadow with ACL support])
			LIBACL="-lacl"
			with_acl="yes"
		fi
	else
		with_acl="no"
	fi
fi

AC_SUBST(LIBATTR)
if test "$with_attr" != "no"; then
	AC_CHECK_HEADERS(attr/libattr.h attr/error_context.h, [attr_header="yes"], [attr_header="no"])
	if test "$attr_header$with_attr" = "noyes" ; then
		AC_MSG_ERROR([attr/libattr.h or attr/error_context.h is missing])
	elif test "$attr_header" = "yes" ; then
		AC_CHECK_LIB(attr, attr_copy_file,
		             [AC_CHECK_LIB(attr, attr_copy_fd,
		                           [attr_lib="yes"],
		                           [attr_lib="no"])],
		             [attr_lib="no"])
		if test "$attr_lib$with_attr" = "noyes" ; then
			AC_MSG_ERROR([libattr not found])
		elif test "$attr_lib" = "no" ; then
			with_attr="no"
		else
			AC_DEFINE(WITH_ATTR, 1,
			          [Build shadow with Extended Attributes support])
			LIBATTR="-lattr"
			with_attr="yes"
		fi
	else
		with_attr="no"
	fi
fi

AC_SUBST(LIBAUDIT)
if test "$with_audit" != "no"; then
	AC_CHECK_HEADER(libaudit.h, [audit_header="yes"], [audit_header="no"])
	if test "$audit_header$with_audit" = "noyes" ; then
		AC_MSG_ERROR([libaudit.h is missing])
	elif test "$audit_header" = "yes"; then
		AC_CHECK_DECL(AUDIT_ADD_USER,,[audit_header="no"],[#include <libaudit.h>])
		AC_CHECK_DECL(AUDIT_DEL_USER,,[audit_header="no"],[#include <libaudit.h>])
		AC_CHECK_DECL(AUDIT_ADD_GROUP,,[audit_header="no"],[#include <libaudit.h>])
		AC_CHECK_DECL(AUDIT_DEL_GROUP,,[audit_header="no"],[#include <libaudit.h>])
		if test "$audit_header$with_audit" = "noyes" ; then
			AC_MSG_ERROR([AUDIT_ADD_USER AUDIT_DEL_USER AUDIT_ADD_GROUP or AUDIT_DEL_GROUP missing from libaudit.h])
		fi
	fi
	if test "$audit_header" = "yes"; then
		AC_CHECK_LIB(audit, audit_log_acct_message,
		             [audit_lib="yes"], [audit_lib="no"])
		if test "$audit_lib$with_audit" = "noyes" ; then
			AC_MSG_ERROR([libaudit not found])
		elif test "$audit_lib" = "no" ; then
			with_audit="no"
		else
			AC_DEFINE(WITH_AUDIT, 1,
			          [Define if you want to enable Audit messages])
			LIBAUDIT="-laudit"
			with_audit="yes"
		fi
	else
		with_audit="no"
	fi
fi

if test "$with_btrfs" != "no"; then
	AC_CHECK_HEADERS([sys/statfs.h linux/magic.h linux/btrfs_tree.h], \
		[btrfs_headers="yes"], [btrfs_headers="no"])
	if test "$btrfs_headers$with_btrfs" = "noyes" ; then
		AC_MSG_ERROR([One of sys/statfs.h linux/magic.h linux/btrfs_tree.h is missing])
	fi

	if test "$btrfs_headers" = "yes" ; then
		AC_DEFINE(WITH_BTRFS, 1, [Build shadow with BtrFS support])
		with_btrfs="yes"
	fi
fi
AM_CONDITIONAL(WITH_BTRFS, test x$with_btrfs = xyes)

AC_SUBST(LIBSELINUX)
AC_SUBST(LIBSEMANAGE)
if test "$with_selinux" != "no"; then
	AC_CHECK_HEADERS(selinux/selinux.h, [selinux_header="yes"], [selinux_header="no"])
	if test "$selinux_header$with_selinux" = "noyes" ; then
		AC_MSG_ERROR([selinux/selinux.h is missing])
	fi

	AC_CHECK_HEADERS(semanage/semanage.h, [semanage_header="yes"], [semanage_header="no"])
	if test "$semanage_header$with_selinux" = "noyes" ; then
		AC_MSG_ERROR([semanage/semanage.h is missing])
	fi

	if test "$selinux_header$semanage_header" = "yesyes" ; then
		AC_CHECK_LIB(selinux, is_selinux_enabled, [selinux_lib="yes"], [selinux_lib="no"])
		if test "$selinux_lib$with_selinux" = "noyes" ; then
			AC_MSG_ERROR([libselinux not found])
		fi

		AC_CHECK_LIB(semanage, semanage_connect, [semanage_lib="yes"], [semanage_lib="no"])
		if test "$semanage_lib$with_selinux" = "noyes" ; then
			AC_MSG_ERROR([libsemanage not found])
		fi

		if test "$selinux_lib$semanage_lib" = "yesyes" ; then
			AC_DEFINE(WITH_SELINUX, 1,
			          [Build shadow with SELinux support])
			LIBSELINUX="-lselinux"
			LIBSEMANAGE="-lsemanage"
			with_selinux="yes"
		else
			with_selinux="no"
		fi
	else
		with_selinux="no"
	fi
fi

AC_SUBST(LIBTCB)
if test "$with_tcb" != "no"; then
	AC_CHECK_HEADERS(tcb.h, [tcb_header="yes"], [tcb_header="no"])
	if test "$tcb_header$with_tcb" = "noyes" ; then
		AC_MSG_ERROR([tcb.h is missing])
	elif test "$tcb_header" = "yes" ; then
		AC_CHECK_LIB(tcb, tcb_is_suspect, [tcb_lib="yes"], [tcb_lib="no"])
		if test "$tcb_lib$with_tcb" = "noyes" ; then
			AC_MSG_ERROR([libtcb not found])
		elif test "$tcb_lib" = "no" ; then
			with_tcb="no"
		else
			AC_DEFINE(WITH_TCB, 1, [Build shadow with tcb support (incomplete)])
			LIBTCB="-ltcb"
			with_tcb="yes"
		fi
	else
		with_tcb="no"
	fi
fi
AM_CONDITIONAL(WITH_TCB, test x$with_tcb = xyes)

AC_SUBST(LIBPAM)
if test "$with_libpam" != "no"; then
	AC_CHECK_LIB(pam, pam_start,
	             [pam_lib="yes"], [pam_lib="no"])
	if test "$pam_lib$with_libpam" = "noyes" ; then
		AC_MSG_ERROR(libpam not found)
	fi

	LIBPAM="-lpam"
	pam_conv_function="no"

	AC_CHECK_LIB(pam, openpam_ttyconv,
	             [pam_conv_function="openpam_ttyconv"],
	             AC_CHECK_LIB(pam_misc, misc_conv,
	                          [pam_conv_function="misc_conv"; LIBPAM="$LIBPAM -lpam_misc"])
	            )

	if test "$pam_conv_function$with_libpam" = "noyes" ; then
		AC_MSG_ERROR(PAM conversation function not found)
	fi

	pam_headers_found=no
	AC_CHECK_HEADERS( [security/openpam.h security/pam_misc.h],
	                 [ pam_headers_found=yes ; break ], [],
	                 [ #include <security/pam_appl.h> ] )
	if test "$pam_headers_found$with_libpam" = "noyes" ; then
	                AC_MSG_ERROR(PAM headers not found)
	fi


	if test "$pam_lib$pam_headers_found" = "yesyes" -a "$pam_conv_function" != "no" ; then
		with_libpam="yes"
	else
		with_libpam="no"
		unset LIBPAM
	fi
fi
dnl Now with_libpam is either yes or no
if test "$with_libpam" = "yes"; then
	AC_CHECK_DECLS([PAM_ESTABLISH_CRED,
	                PAM_DELETE_CRED,
	                PAM_NEW_AUTHTOK_REQD,
	                PAM_DATA_SILENT],
	               [], [], [#include <security/pam_appl.h>])


	save_libs=$LIBS
	LIBS="$LIBS $LIBPAM"
	# We do not use AC_CHECK_FUNCS to avoid duplicated definition with
	# Linux PAM.
	AC_CHECK_FUNC(pam_fail_delay, [AC_DEFINE(HAS_PAM_FAIL_DELAY, 1, [Define to 1 if you have the declaration of 'pam_fail_delay'])])
	LIBS=$save_libs

	AC_DEFINE(USE_PAM, 1, [Define to support Pluggable Authentication Modules])
	AC_DEFINE_UNQUOTED(SHADOW_PAM_CONVERSATION, [$pam_conv_function],[PAM conversation to use])
	AM_CONDITIONAL(USE_PAM, [true])

	AC_MSG_CHECKING(use login and su access checking if PAM not used)
	AC_MSG_RESULT(no)
else
	AC_DEFINE(SU_ACCESS, 1, [Define to support /etc/suauth su access control.])
	AM_CONDITIONAL(USE_PAM, [false])
	AC_MSG_CHECKING(use login and su access checking if PAM not used)
	AC_MSG_RESULT(yes)
fi

if test "$enable_acct_tools_setuid" != "no"; then
	if test "$with_libpam" != "yes"; then
		if test "$enable_acct_tools_setuid" = "yes"; then
			AC_MSG_ERROR(PAM support is required for --enable-account-tools-setuid)
		else
			enable_acct_tools_setuid="no"
		fi
	else
		enable_acct_tools_setuid="yes"
	fi
	if test "$enable_acct_tools_setuid" = "yes"; then
		AC_DEFINE(ACCT_TOOLS_SETUID,
		          1,
		          [Define if account management tools should be installed setuid and authenticate the callers])
	fi
fi
AM_CONDITIONAL(ACCT_TOOLS_SETUID, test "x$enable_acct_tools_setuid" = "xyes")


AC_ARG_WITH(fcaps,
	[AS_HELP_STRING([--with-fcaps], [use file capabilities instead of suid binaries for newuidmap/newgidmap @<:@default=no@:>@])],
	[with_fcaps=$withval], [with_fcaps=no])
AM_CONDITIONAL(FCAPS, test "x$with_fcaps" = "xyes")

if test "x$with_fcaps" = "xyes"; then
	AC_CHECK_PROGS(capcmd, "setcap")
	if test "x$capcmd" = "x" ; then
		AC_MSG_ERROR([setcap command not available])
	fi
fi

AC_SUBST(LIBSKEY)
AC_SUBST(LIBMD)
if test "$with_skey" = "yes"; then
	AC_CHECK_LIB(md, MD5Init, [LIBMD=-lmd])
	AC_CHECK_LIB(skey, skeychallenge, [LIBSKEY=-lskey],
		[AC_MSG_ERROR([liskey missing. You can download S/Key source code from http://rsync1.it.gentoo.org/gentoo/distfiles/skey-1.1.5.tar.bz2])])
	AC_DEFINE(SKEY, 1, [Define to support S/Key logins.])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
		#include <stdio.h>
		#include <skey.h>
	]], [[
		skeychallenge((void*)0, (void*)0, (void*)0, 0);
	]])],[AC_DEFINE(SKEY_BSD_STYLE, 1, [Define to support newer BSD S/Key API])],[])
fi

AC_CHECK_FUNC(fgetpwent_r, [AC_DEFINE(HAVE_FGETPWENT_R, 1, [Defined to 1 if you have the declaration of 'fgetpwent_r'])])

AC_DEFINE_UNQUOTED(SHELL, ["$SHELL"], [The default shell.])

AM_GNU_GETTEXT_VERSION([0.19])
AM_GNU_GETTEXT([external], [need-ngettext])
AM_CONDITIONAL(USE_NLS, test "x$USE_NLS" = "xyes")

AC_CONFIG_FILES([
	Makefile
	po/Makefile.in
	doc/Makefile
	man/Makefile
	man/config.xml
	man/po/Makefile
	man/cs/Makefile
	man/da/Makefile
	man/de/Makefile
	man/es/Makefile
	man/fi/Makefile
	man/fr/Makefile
	man/hu/Makefile
	man/id/Makefile
	man/it/Makefile
	man/ja/Makefile
	man/ko/Makefile
	man/pl/Makefile
	man/pt_BR/Makefile
	man/ru/Makefile
	man/sv/Makefile
	man/tr/Makefile
	man/uk/Makefile
	man/zh_CN/Makefile
	man/zh_TW/Makefile
	lib/Makefile
	libsubid/Makefile
	libsubid/subid.h
	src/Makefile
	contrib/Makefile
	etc/Makefile
	etc/pam.d/Makefile
	etc/shadow-maint/Makefile
	tests/unit/Makefile
])
AC_OUTPUT

echo
echo "shadow will be compiled with the following features:"
echo
echo "	auditing support:		$with_audit"
echo "	PAM support:			$with_libpam"
if test "$with_libpam" = "yes"; then
echo "	suid account management tools:	$enable_acct_tools_setuid"
fi
echo "	SELinux support:		$with_selinux"
echo "	BtrFS support:			$with_btrfs"
echo "	ACL support:			$with_acl"
echo "	Extended Attributes support:	$with_attr"
echo "	tcb support (incomplete):	$with_tcb"
echo "	shadow group support:		$enable_shadowgrp"
echo "	S/Key support:			$with_skey"
echo "	SHA passwords encryption:	$with_sha_crypt"
echo "	bcrypt passwords encryption:	$with_bcrypt"
echo "	yescrypt passwords encryption:	$with_yescrypt"
echo "	nscd support:			$with_nscd"
echo "	sssd support:			$with_sssd"
echo "	subordinate IDs support:	$enable_subids"
echo "	enable lastlog:			$enable_lastlog"
echo "	enable logind:			$enable_logind"
echo "	use file caps:			$with_fcaps"
echo "	install su:			$with_su"
echo "	enabled vendor dir:             $enable_vendordir"
echo
