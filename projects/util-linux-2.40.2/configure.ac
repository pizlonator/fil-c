AC_INIT([util-linux],
	m4_esyscmd([tools/git-version-gen .tarball-version]),
	[kzak@redhat.com], [],
	[https://www.kernel.org/pub/linux/utils/util-linux/])


AC_PREREQ([2.64])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
dnl AC_USE_SYSTEM_EXTENSIONS must be called before any macros that run
dnl the compiler (like LT_INIT) to avoid autoconf errors.
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([-Wall -Wno-portability foreign 1.10 tar-pax no-dist-gzip dist-xz subdir-objects])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])],
			    [AC_SUBST([AM_DEFAULT_VERBOSITY], [1])])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_CONFIG_SRCDIR([sys-utils/mount.c])
AC_PREFIX_DEFAULT([/usr])

dnl version details from <major>.<minor>[-<suffix>]
PACKAGE_VERSION_MAJOR=$(echo $PACKAGE_VERSION | awk -F. '{print $1}')
PACKAGE_VERSION_MINOR=$(echo $PACKAGE_VERSION | awk -F. '{
						   sub("-.*","",$2); print $2}')
PACKAGE_VERSION_RELEASE=$(echo $PACKAGE_VERSION | awk -F. '{
		     sub("-.*","",$3); print $3 ~ /^@<:@0-9@:>@+$/ ? $3 : 0}')

dnl libblkid version
LIBBLKID_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBBLKID_DATE="04-Jul-2024"
LIBBLKID_LT_MAJOR=1
LIBBLKID_LT_MINOR=1
LIBBLKID_LT_MICRO=0
LIBBLKID_VERSION_INFO=`expr $LIBBLKID_LT_MAJOR + $LIBBLKID_LT_MINOR`:$LIBBLKID_LT_MICRO:$LIBBLKID_LT_MINOR

dnl libuuid version
LIBUUID_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBUUID_LT_MAJOR=1
LIBUUID_LT_MINOR=3
LIBUUID_LT_MICRO=0
LIBUUID_VERSION_INFO=`expr $LIBUUID_LT_MAJOR + $LIBUUID_LT_MINOR`:$LIBUUID_LT_MICRO:$LIBUUID_LT_MINOR

dnl liblastlog2 version
LIBLASTLOG2_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBLASTLOG2_LT_MAJOR=2
LIBLASTLOG2_LT_MINOR=0
LIBLASTLOG2_LT_MICRO=0
LIBLASTLOG2_VERSION_INFO=`expr $LIBLASTLOG2_LT_MAJOR + $LIBLASTLOG2_LT_MINOR`:$LIBLASTLOG2_LT_MICRO:$LIBLASTLOG2_LT_MINOR

dnl libmount version
LIBMOUNT_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBMOUNT_LT_MAJOR=1
LIBMOUNT_LT_MINOR=1
LIBMOUNT_LT_MICRO=0
LIBMOUNT_VERSION_INFO=`expr $LIBMOUNT_LT_MAJOR + $LIBMOUNT_LT_MINOR`:$LIBMOUNT_LT_MICRO:$LIBMOUNT_LT_MINOR

dnl libsmartcols version
LIBSMARTCOLS_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBSMARTCOLS_LT_MAJOR=1
LIBSMARTCOLS_LT_MINOR=1
LIBSMARTCOLS_LT_MICRO=0
LIBSMARTCOLS_VERSION_INFO=`expr $LIBSMARTCOLS_LT_MAJOR + $LIBSMARTCOLS_LT_MINOR`:$LIBSMARTCOLS_LT_MICRO:$LIBSMARTCOLS_LT_MINOR

dnl libfdisk version
LIBFDISK_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBFDISK_LT_MAJOR=1
LIBFDISK_LT_MINOR=1
LIBFDISK_LT_MICRO=0
LIBFDISK_VERSION_INFO=`expr $LIBFDISK_LT_MAJOR + $LIBFDISK_LT_MINOR`:$LIBFDISK_LT_MICRO:$LIBFDISK_LT_MINOR

# Check whether exec_prefix=/usr:
AS_CASE([$exec_prefix:$prefix],
[NONE:NONE | NONE:/usr | /usr:*],
  [AC_MSG_NOTICE([Default --exec-prefix detected.])
   AS_CASE([$bindir],  ['${exec_prefix}/bin'],  [bindir=/bin;   AC_MSG_NOTICE([  --bindir defaults to /bin])  ])
   AS_CASE([$sbindir], ['${exec_prefix}/sbin'], [sbindir=/sbin; AC_MSG_NOTICE([  --sbindir defaults to /sbin])])
   AS_CASE([$libdir],  ['${exec_prefix}/lib'],  [libdir=/lib;   AC_MSG_NOTICE([  --libdir defaults to /lib])  ])
   AS_CASE([$libdir],  ['${exec_prefix}/lib64'],[libdir=/lib64; AC_MSG_NOTICE([  --libdir defaults to /lib64])  ])
  ]
)

# Check whether prefix=/usr
AS_CASE([$prefix],
[NONE | /usr],
  [AC_MSG_NOTICE([Default --prefix detected.])
   AS_CASE([$localstatedir], [NONE | '${prefix}/var' | "${prefix}"/var ],
	   [localstatedir=/var; AC_MSG_NOTICE([  --localstatedir defaults to /var])])
   AS_CASE([$sysconfdir], [NONE | '${prefix}/etc' | "${prefix}"/etc ],
	   [sysconfdir=/etc; AC_MSG_NOTICE([  --sysconfdir defaults to /etc])])

  ]
)
AC_SUBST([localstatedir])


# default for old versions without $runstatedir
AS_IF([test x"$runstatedir" = x], [runstatedir='${localstatedir}/run'])

# our default if $localstatedir unchanged
AS_CASE([$localstatedir:$runstatedir],
 [NONE:'${localstatedir}/run' | /var:'${localstatedir}/run' | NONE:'/run' ],
   [runstatedir=/run; AC_MSG_NOTICE([  --runstatedir defaults to /run])]
)
AC_SUBST([runstatedir])


# The original default values of {bin,sbin,lib}dir
usrbin_execdir='${exec_prefix}/bin'
AC_SUBST([usrbin_execdir])

usrsbin_execdir='${exec_prefix}/sbin'
AC_SUBST([usrsbin_execdir])

AS_CASE([$libdir],
 ['${exec_prefix}/'* | '${prefix}/'* | /usr/* | "${prefix}"/* ],
   [usrlib_execdir=$libdir],
 [usrlib_execdir='${exec_prefix}'$libdir]
)
AC_SUBST([usrlib_execdir])

usrsecurelib_execdir='${usrlib_execdir}/security'
AC_SUBST([usrsecurelib_execdir])

# static configuration maintaned by packages (e.g. /usr/lib)
AC_ARG_VAR([SYSCONFSTATICDIR],
	   [Path to static system configuration, default ${prefix}/lib])
AS_IF([test "x$SYSCONFSTATICDIR" = x],
      [sysconfstaticdir='${prefix}/lib'],
      [sysconfstaticdir=$SYSCONFSTATICDIR])
AC_SUBST([sysconfstaticdir])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_MKDIR_P
AC_PROG_YACC
AC_PROG_SED

# Don't use autotools integrated LEX/YACC support for libsmartcols
AC_PATH_PROG([FLEX], [flex])
AC_PATH_PROG([BISON], [bison])

AC_CANONICAL_HOST
AC_C_CONST
AC_C_VOLATILE

dnl Compiler warnings
UL_WARN_ADD([-fno-common])
UL_WARN_ADD([-Wall])
UL_WARN_ADD([-Wextra])

UL_WARN_ADD([-Waddress-of-packed-member])
UL_WARN_ADD([-Wdiscarded-qualifiers])
UL_WARN_ADD([-Wembedded-directive])
UL_WARN_ADD([-Wextra-semi])
UL_WARN_ADD([-Wformat-security])
UL_WARN_ADD([-Wimplicit-function-declaration])
UL_WARN_ADD([-Wmissing-declarations])
UL_WARN_ADD([-Wmissing-parameter-type])
UL_WARN_ADD([-Wmissing-prototypes])
UL_WARN_ADD([-Wnested-externs])
UL_WARN_ADD([-Wno-missing-field-initializers])
UL_WARN_ADD([-Wold-style-definition])
UL_WARN_ADD([-Wpointer-arith])
UL_WARN_ADD([-Wredundant-decls])
UL_WARN_ADD([-Wsign-compare])
UL_WARN_ADD([-Wstrict-prototypes])
UL_WARN_ADD([-Wtype-limits])
UL_WARN_ADD([-Wuninitialized])
UL_WARN_ADD([-Wunused-but-set-parameter])
UL_WARN_ADD([-Wunused-but-set-variable])
UL_WARN_ADD([-Wunused-parameter])
UL_WARN_ADD([-Wunused-result])
UL_WARN_ADD([-Wunused-variable])
UL_WARN_ADD([-Wvla])
UL_WARN_ADD([-Walloca])

AC_ARG_ENABLE([werror],
  AS_HELP_STRING([--enable-werror], [make all compiler warnings into errors]),
  [], [enable_werror=no]
)
AS_IF([test "x$enable_werror" = xyes], [
  UL_WARN_ADD([-Werror])
],[
  UL_WARN_ADD([-Werror=sequence-point])
])

AC_SUBST([WARN_CFLAGS])


UL_WARN_ADD([-Wno-clobbered], [BSD_WARN_CFLAGS])
AC_SUBST([BSD_WARN_CFLAGS])

UL_WARN_ADD([-Wno-cast-function-type], [PYTHON_WARN_CFLAGS])
AC_SUBST([PYTHON_WARN_CFLAGS])


AC_ARG_ENABLE([asan],
  AS_HELP_STRING([--enable-asan], [compile with Address Sanitizer]),
  [], [enable_asan=no]
)
AS_IF([test "x$enable_asan" = xyes], [
  UL_WARN_ADD([-fsanitize=address])
  ASAN_LDFLAGS="-fsanitize=address"
])
AC_SUBST([ASAN_LDFLAGS])

AC_ARG_ENABLE([ubsan],
  AS_HELP_STRING([--enable-ubsan], [compile with UBsan]),
  [], [enable_ubsan=no]
)
AS_IF([test "x$enable_ubsan" = xyes], [
  UL_WARN_ADD([-fsanitize=undefined])
  UBSAN_LDFLAGS="-fsanitize=undefined"
])
AC_SUBST([UBSAN_LDFLAGS])

AC_ARG_ENABLE([fuzzing-engine],
  AS_HELP_STRING([--enable-fuzzing-engine], [compile with fuzzing engine]),
  [], [enable_fuzzing_engine=no]
)
AS_IF([test "x$enable_fuzzing_engine" = xyes && test "x$LIB_FUZZING_ENGINE" = x], [
  UL_WARN_ADD([-fno-omit-frame-pointer])
  UL_WARN_ADD([-gline-tables-only])
  UL_WARN_ADD([-fsanitize=fuzzer-no-link])
  FUZZING_ENGINE_LDFLAGS="-fsanitize=fuzzer-no-link"
])
AC_SUBST([FUZZING_ENGINE_LDFLAGS])
AC_PROG_CXX
AM_CONDITIONAL([FUZZING_ENGINE], [test "x$enable_fuzzing_engine" = xyes])
AM_CONDITIONAL([OSS_FUZZ], [test "x$LIB_FUZZING_ENGINE" != x])

AC_ARG_ENABLE([coverage],
  AS_HELP_STRING([--enable-coverage], [compile with gcov]),
  [], [enable_coverage=no]
)
AS_IF([test "x$enable_coverage" = xyes], [
  UL_WARN_ADD([--coverage])
  COVERAGE_LDFLAGS="--coverage"
])
AC_SUBST([COVERAGE_LDFLAGS])
AM_CONDITIONAL([WITH_COVERAGE], [test "x$enable_coverage" = xyes])

dnl libtool-2
LT_INIT

dnl check supported linker flags
AX_CHECK_VSCRIPT

m4_ifndef([PKG_PROG_PKG_CONFIG],
  [m4_fatal([Could not locate the pkg-config autoconf
    macros. These are usually located in /usr/share/aclocal/pkg.m4.
    If your macros are in a different location, try setting the
    environment variable AL_OPTS="-I/other/macro/dir" before running
    ./autogen.sh or autoreconf again. Make sure pkg-config is installed.])])
PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR(['${usrlib_execdir}/pkgconfig'])

GTK_DOC_CHECK([1.10])
AC_PATH_PROG([XSLTPROC], [xsltproc])


linux_os=no
bsd_os=no
AS_CASE([${host_os}],
  [*linux*],
     [linux_os=yes],
  [*darwin*],
     [darwin_os=yes],
  [*bsd*],
     [bsd_os=yes])
AM_CONDITIONAL([LINUX], [test "x$linux_os" = xyes])
AM_CONDITIONAL([DARWIN], [test "x$darwin_os" = xyes])
AM_CONDITIONAL([BSD], [test "x$bsd_os" = xyes])

AS_IF([test  "x$darwin_os" = xyes], [
  AC_DEFINE([_DARWIN_C_SOURCE], [1], [Enable MAP_ANON in sys/mman.h on Mac OS X])
])

dnl define ARCH_<NAME> conditionals
UL_SET_ARCH([I86], [i?86-*])
UL_SET_ARCH([86_64], [x86_64*])
UL_SET_ARCH([IA64], [ia64*])
UL_SET_ARCH([S390], [s390*])
UL_SET_ARCH([SPARC], [sparc*])
UL_SET_ARCH([PPC], [ppc*|powerpc*])
UL_SET_ARCH([M68K], [m68*])
UL_SET_ARCH([MIPS], [mips*])
UL_SET_ARCH([HPPA], [hppa*|parisc*])

AC_SYS_LARGEFILE
AC_HEADER_ASSERT
AC_STRUCT_TIMEZONE
UL_YEAR2038_INIT
AC_SYS_YEAR2038_RECOMMENDED

dnl Don't forget to maintain alternatively allowed versions in autogen.sh!
AM_GNU_GETTEXT_VERSION([0.18.3])
AM_GNU_GETTEXT([external])

AS_IF([test -d "$srcdir/po"], [
  ALL_LINGUAS=`cd $srcdir/po > /dev/null && echo *.po | sed 's/\.po//g'`
], [dnl echo $ALL_LINGUAS | sed 's/ /\n/g' | sort -u | column -xc 72 -o ' ' | sed 's/^/\t/'
  ALL_LINGUAS="
	af      am      ar      as      be      bg      bn      bn_IN   ca
	cs      cy      da      de      el      en_GB   es      et      eu_ES
	fa      fi      fr      gl      gu      he      hi      hr      hu
	hy      id      is      it      ja      ka      kn      ko      ku
	lo      lt      lv      mk      ml      mr      ms      my      nb
	nl      nn      no      nso     or      pa      pl      pt      pt_BR
	ro      ru      si      sk      sl      sq      sr      sr@Latn sv
	ta      te      th      tr      uk      ur      vi      zh_CN   zh_TW
	zu
"])

AC_CHECK_HEADERS([linux/compiler.h linux/blkpg.h linux/major.h], [], [], [
#ifdef HAVE_LINUX_COMPILER_H
#include <linux/compiler.h>
#endif
])
AC_CHECK_HEADERS([ \
	asm-generic/fcntl.h \
	asm/fcntl.h \
	asm/io.h \
	byteswap.h \
	endian.h \
	err.h \
	errno.h \
	fcntl.h \
	getopt.h \
	inttypes.h \
	lastlog.h \
	libutil.h \
	linux/blkzoned.h \
	linux/btrfs.h \
	linux/capability.h \
	linux/cdrom.h \
	linux/falloc.h \
	linux/fd.h \
	linux/fiemap.h \
	linux/landlock.h \
	linux/kcmp.h \
	linux/net_namespace.h \
	linux/nsfs.h \
	linux/pr.h \
	linux/raw.h \
	linux/securebits.h \
	linux/tiocl.h \
	linux/version.h \
	linux/watchdog.h \
	linux/if_alg.h \
	locale.h \
	mntent.h \
	net/if_dl.h \
	net/if.h \
	netinet/in.h \
	paths.h \
	pty.h \
	security/pam_appl.h \
	security/pam_modules.h \
	shadow.h \
	stdint.h \
	stdio_ext.h \
	stdlib.h \
	sys/disk.h \
	sys/disklabel.h \
	sys/endian.h \
	sys/file.h \
	sys/ioccom.h \
	sys/ioctl.h \
	sys/io.h \
	sys/mkdev.h \
	sys/mount.h \
	sys/param.h \
	sys/pidfd.h \
	sys/prctl.h \
	sys/resource.h \
	sys/sendfile.h \
	sys/signalfd.h \
	sys/socket.h \
	sys/sockio.h \
	sys/stat.h \
	sys/statfs.h \
	sys/swap.h \
	sys/syscall.h \
	sys/sysmacros.h \
	sys/time.h \
	sys/timex.h \
	sys/ttydefaults.h \
	sys/types.h \
	sys/ucred.h \
	sys/un.h \
	sys/vfs.h \
	sys/xattr.h \
	unistd.h \
	utmp.h \
	utmpx.h \
])

# There is a collision in old kernel-headers. The both files mount.h and fs.h
# define MS_* macros. Fixed by kernel commit e462ec50cb5fad19f6003a3d8087f4a0945dd2b1.
#
AC_CHECK_HEADERS([linux/fs.h ], [], [],
 [#ifdef HAVE_SYS_MOUNT_H
   # include <linux/fs.h>
   # include <sys/mount.h>
  #endif
])

AC_CHECK_HEADERS([linux/mount.h ], [], [],
 [#ifdef HAVE_LINUX_MOUNT_H
   # include <linux/mount.h>
   #endif
])

AC_CHECK_HEADERS([linux/gsmmux.h ], [], [],
 [#ifdef LINUX_GSMMUX_H
   # include <linux/gsmmux.h>
   #endif
])

AC_CHECK_HEADERS([security/pam_misc.h],
		[AM_CONDITIONAL([HAVE_LINUXPAM], [true])],
		[AM_CONDITIONAL([HAVE_LINUXPAM], [false])], [
#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>
#endif
])

AC_CHECK_DECLS([BLK_ZONE_REP_CAPACITY], [], [], [
	#include <linux/blkzoned.h>
])

AC_CHECK_DECLS([PR_REP_CAPACITY], [], [], [
	#include <linux/pr.h>
])

AC_CHECK_HEADERS([security/openpam.h], [], [], [
#ifdef HAVE_SECURITY_PAM_APPL_H
#include <security/pam_appl.h>
#endif
])

AC_CHECK_HEADERS([langinfo.h],
		[AM_CONDITIONAL([HAVE_LANGINFO_H], [true])],
		[AM_CONDITIONAL([HAVE_LANGINFO_H], [false])])

AC_MSG_CHECKING([whether langinfo.h defines ALTMON_x constants])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	#include <langinfo.h>
]], [[
	char *str;
	str = nl_langinfo (ALTMON_1);
	str = nl_langinfo (ALTMON_2);
	str = nl_langinfo (ALTMON_3);
	str = nl_langinfo (ALTMON_4);
	str = nl_langinfo (ALTMON_5);
	str = nl_langinfo (ALTMON_6);
	str = nl_langinfo (ALTMON_7);
	str = nl_langinfo (ALTMON_8);
	str = nl_langinfo (ALTMON_9);
	str = nl_langinfo (ALTMON_10);
	str = nl_langinfo (ALTMON_11);
	str = nl_langinfo (ALTMON_12);
]])], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_LANGINFO_ALTMON], [1],
		[Define if langinfo.h defines ALTMON_x constants])
], [
	AC_MSG_RESULT([no])
])


AC_MSG_CHECKING([whether langinfo.h defines _NL_ABALTMON_x constants])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	#include <langinfo.h>
]], [[
	char *str;
	str = nl_langinfo (_NL_ABALTMON_1);
	str = nl_langinfo (_NL_ABALTMON_2);
	str = nl_langinfo (_NL_ABALTMON_3);
	str = nl_langinfo (_NL_ABALTMON_4);
	str = nl_langinfo (_NL_ABALTMON_5);
	str = nl_langinfo (_NL_ABALTMON_6);
	str = nl_langinfo (_NL_ABALTMON_7);
	str = nl_langinfo (_NL_ABALTMON_8);
	str = nl_langinfo (_NL_ABALTMON_9);
	str = nl_langinfo (_NL_ABALTMON_10);
	str = nl_langinfo (_NL_ABALTMON_11);
	str = nl_langinfo (_NL_ABALTMON_12);
]])], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_LANGINFO_NL_ABALTMON], [1],
		[Define if langinfo.h defines _NL_ABALTMON_x constants])
], [
	AC_MSG_RESULT([no])
])


dnl Convert some ac_cv_header_* variables to have_*
dnl
have_linux_blkzoned_h=$ac_cv_header_linux_blkzoned_h
have_linux_btrfs_h=$ac_cv_header_linux_btrfs_h
have_linux_capability_h=$ac_cv_header_linux_capability_h
have_linux_kcmp_h=$ac_cv_header_linux_kcmp_h
have_linux_mount_h=$ac_cv_header_linux_mount_h
have_linux_pr_h=$ac_cv_header_linux_pr_h
have_linux_raw_h=$ac_cv_header_linux_raw_h
have_linux_securebits_h=$ac_cv_header_linux_securebits_h
have_linux_version_h=$ac_cv_header_linux_version_h
have_linux_watchdog_h=$ac_cv_header_linux_watchdog_h
have_pty_h=$ac_cv_header_pty_h
have_security_openpam_h=$ac_cv_header_security_openpam_h
have_security_pam_appl_h=$ac_cv_header_security_pam_appl_h
have_security_pam_misc_h=$ac_cv_header_security_pam_misc_h
have_security_pam_modules_h=$ac_cv_header_security_pam_modules_h
have_shadow_h=$ac_cv_header_shadow_h
have_sys_signalfd_h=$ac_cv_header_sys_signalfd_h
have_utmpx_h=$ac_cv_header_utmpx_h
have_mntent_h=$ac_cv_header_mntent_h

AS_CASE([$linux_os:$have_linux_version_h],
  [yes:no],
    [AC_MSG_ERROR([kernel headers required on Linux])]
)

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <time.h>
#include <unistd.h>
]], [[
    int a = 0;
    struct tm *tm = localtime(0);
    if (a == -1)  /* false */
	sleep(tm->tm_gmtoff);
]])],
[AC_DEFINE([HAVE_TM_GMTOFF], [1], [Does struct tm have a field tm_gmtoff?])
])

AC_CHECK_TYPES([struct mount_attr], [], [], [[#include <linux/mount.h>]])
AC_CHECK_TYPES([enum fsconfig_command], [], [], [[#include <linux/mount.h>]])

AC_CHECK_MEMBERS([struct termios.c_line],,,
    [[#include <termios.h>]])

AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec],,,
    [[#include <sys/stat.h>]])

AC_CHECK_TYPES([struct statx], [], [], [[#include <sys/stat.h>]])
AC_CHECK_MEMBERS([struct statx.stx_mnt_id],,,
    [[#include <sys/stat.h>]])

AC_CHECK_DECLS([_NL_TIME_WEEK_1STDAY],[],[],[[#include <langinfo.h>]])

AC_CHECK_DECL([environ],
	[AC_DEFINE([HAVE_ENVIRON_DECL], [1],
		[Define to 1 if have **environ prototype])],
)

AC_CHECK_DECL([strsignal],
	[AC_DEFINE([HAVE_STRSIGNAL_DECL], [1],
		[Define to 1 if have strsignal function prototype])],
)

AC_CHECK_DECL([TIOCGLCKTRMIOS],
	      [have_tiocglcktrmios=yes], [have_tiocglcktrmios=no],
	      [#include <sys/ioctl.h>])
AC_CHECK_DECL([SOCK_CLOEXEC],
	      [have_sock_cloexec=yes], [have_sock_cloexec=no],
	      [#include <sys/types.h>
	       #include <sys/socket.h>])
AC_CHECK_DECL([SOCK_NONBLOCK],
	      [have_sock_nonblock=yes], [have_sock_nonblock=no],
	      [#include <sys/types.h>
	       #include <sys/socket.h>])
AC_CHECK_DECL([SO_PASSCRED],
	      [have_so_passcred=yes], [have_so_passcred=no],
	      [#include <sys/types.h>
	       #include <sys/socket.h>])

AC_CHECK_FUNCS([ \
	cachestat \
	clearenv \
	close_range \
	eaccess \
	err \
	errx \
	explicit_bzero \
	__fpending \
	__fpurge \
	fpurge \
	fnmatch \
	fsconfig \
	fsmount \
	fsopen \
	fspick \
	fsync \
	getdomainname \
	getdtablesize \
	getexecname \
	getmntinfo \
	getrandom \
	getrlimit \
	getsgnam \
	getttynam \
	inotify_init \
	jrand48 \
	landlock_create_ruleset \
	landlock_add_rule \
	landlock_restrict_self \
	lchown \
	lgetxattr \
	llistxattr \
	llseek \
	newlocale \
	mempcpy \
	mkostemp \
	move_mount \
	mount_setattr \
	nanosleep \
	ntp_gettime \
	open_tree \
	personality \
	pidfd_open \
	pidfd_send_signal \
	posix_fadvise \
	prctl \
	qsort_r \
	reallocarray \
	renameat2 \
	rpmatch \
	scandirat \
	sched_setattr \
	sched_setscheduler \
	__secure_getenv \
	secure_getenv \
	sendfile \
	setprogname \
	setresgid \
	setresuid \
	sigqueue \
	srandom \
	statx \
	strnchr \
	strndup \
	strnlen \
	strtod_l \
	sysconf \
	sysinfo \
	timegm \
	usleep \
	uselocale \
	utimensat \
	vwarnx \
	warn \
	warnx \
])
AC_FUNC_FSEEKO

AC_CHECK_FUNCS([futimens], [have_futimens=yes])
AC_CHECK_FUNCS([getusershell], [have_getusershell=yes],[have_getusershell=no])
AC_CHECK_FUNCS([inotify_init1], [have_inotify_init1=yes])
AC_CHECK_FUNCS([ioperm iopl], [have_io=yes])
AC_CHECK_FUNCS([openat fstatat unlinkat], [have_openat=yes], [have_openat=no])
AC_CHECK_FUNCS([open_memstream], [have_open_memstream=yes],[have_open_memstream=no])
AC_CHECK_FUNCS([reboot], [have_reboot=yes],[have_reboot=no])
AC_CHECK_FUNCS([updwtmpx updwtmpx], [have_gnu_utmpx=yes], [have_gnu_utmpx=no])

AM_CONDITIONAL([HAVE_OPENAT], [test "x$have_openat" = xyes])
AM_CONDITIONAL([HAVE_LINUX_LANDLOCK_H], [test "x$ac_cv_header_linux_landlock_h" = xyes])

have_setns_syscall="yes"
UL_CHECK_SYSCALL([setns])
AS_IF([test "x$ul_cv_syscall_setns" = xno], [
   have_setns_syscall="no"
])

UL_CHECK_SYSCALL([pidfd_open])
UL_CHECK_SYSCALL([pidfd_send_signal])
UL_CHECK_SYSCALL([close_range])
UL_CHECK_SYSCALL([fsconfig])
UL_CHECK_SYSCALL([fsmount])
UL_CHECK_SYSCALL([fsopen])
UL_CHECK_SYSCALL([fspick])
UL_CHECK_SYSCALL([mount_setattr])
UL_CHECK_SYSCALL([move_mount])
UL_CHECK_SYSCALL([open_tree])

AS_IF([test "x$ul_cv_syscall_fsconfig" = xno ||
       test "x$ul_cv_syscall_fsmount" = xno ||
       test "x$ul_cv_syscall_fsopen" = xno ||
       test "x$ul_cv_syscall_fspick" = xno ||
       test "x$ul_cv_syscall_mount_setattr" = xno ||
       test "x$ul_cv_syscall_move_mount" = xno ||
       test "x$ul_cv_syscall_open_tree" = xno],
   [
	have_mountfd_api="no"
   ],[
        have_mountfd_api="yes"
	AC_DEFINE([HAVE_MOUNTFD_API], [1], [Define to 1 if you want mount API based on FDs.])
   ])


AC_CHECK_FUNCS([isnan], [],
	[AC_CHECK_LIB([m], [isnan], [MATH_LIBS="-lm"])]
	[AC_CHECK_LIB([m], [__isnan], [MATH_LIBS="-lm"])]
)
AC_SUBST([MATH_LIBS])


dnl lib/mononotic.c may require -lrt
AC_CHECK_FUNCS([clock_gettime], [],
	[AC_CHECK_LIB([rt], [clock_gettime], [REALTIME_LIBS="-lrt"])]
)

have_timer="no"
AC_CHECK_FUNCS([timer_create],
	[have_timer="yes"],
	[AC_CHECK_LIB([rt], [timer_create], [
		have_timer="yes"
		REALTIME_LIBS="-lrt"
		AC_DEFINE([HAVE_TIMER_CREATE], [1], [Define if timer_create exist in -lrt])
	],[
		AC_SEARCH_LIBS([timer_create], [rt], [
			AC_MSG_RESULT(yes)
			have_timer="yes"
			REALTIME_LIBS="-lrt -lpthread"
			AC_DEFINE([HAVE_TIMER_CREATE], [1], [Define if timer_create exist in -lrt -lpthread])
		],[], [-lpthread]
		)
	])]
)

AC_SUBST([REALTIME_LIBS])

AS_IF([test x"$have_timer" = xno], [
       AC_CHECK_FUNCS([setitimer], [have_timer="yes"], [have_timer="no"])
])


AC_CHECK_LIB([rtas], [rtas_get_sysparm], [
	RTAS_LIBS="-lrtas"
	AC_DEFINE([HAVE_LIBRTAS], [1], [Define if librtas exists])
])
AC_SUBST([RTAS_LIBS])


AC_CHECK_MEMBER([struct sockaddr.sa_len],
		AC_DEFINE([HAVE_SA_LEN], [1], [Define if struct sockaddr contains sa_len]), [],
	[#include <sys/types.h>
	 #include <sys/socket.h>])

SOCKET_LIBS=
AC_SEARCH_LIBS([socket], [socket],
	[AS_IF([test x"$ac_cv_search_socket" != x"none required"],
		[SOCKET_LIBS="$SOCKET_LIBS -lsocket"])
])
AC_SUBST([SOCKET_LIBS])


have_dirfd=no
AC_CHECK_FUNCS([dirfd], [have_dirfd=yes], [have_dirfd=no])
AS_IF([test x"$have_dirfd" = xno], [
	AC_CHECK_DECLS([dirfd],
		[have_dirfd=yes], [have_dirfd=no],
		[#include <sys/types.h>
		 #include <dirent.h>])
])

have_ddfd=no
AS_IF([test x"$have_dirfd" = xno], [
	AC_CHECK_MEMBERS([DIR.dd_fd],
		[have_ddfd=yes], [have_ddfd=no],
		[#include <sys/types.h>
		 #include <dirent.h>])
])

AM_CONDITIONAL([HAVE_DIRFD], [test "x$have_dirfd" = xyes || test "x$have_ddfd" = xyes])

MQ_LIBS=
AC_CHECK_LIB([rt], [mq_open], [MQ_LIBS="-lrt"])
AC_SUBST([MQ_LIBS])

AC_MSG_CHECKING([whether program_invocation_short_name is defined])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	#include <errno.h>
]], [[
	program_invocation_short_name = "test";
]])], [
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_PROGRAM_INVOCATION_SHORT_NAME], [1],
		[Define if program_invocation_short_name is defined])
], [
	AC_MSG_RESULT([no])
])


AC_MSG_CHECKING([whether __progname is defined])
AC_LINK_IFELSE([AC_LANG_PROGRAM([extern char *__progname;],
	   [if (*__progname == 0) return 1;])],
	   AC_DEFINE([HAVE___PROGNAME], [1], [Define if __progname is defined])
	   AC_MSG_RESULT([yes]),
	   AC_MSG_RESULT([no]))

AC_CHECK_DECL([IOC_OPAL_GET_STATUS],
	[AC_DEFINE([HAVE_OPAL_GET_STATUS], [1],
		[Define to 1 if OPAL status ioctl is defined])],,
	      [#include <linux/sed-opal.h>])

dnl Static compilation
m4_define([UL_STATIC_PROGRAMS], [blkid, fdisk, losetup, mount, nsenter, sfdisk, umount, unshare])

AC_ARG_ENABLE([static-programs],
  [AS_HELP_STRING([--enable-static-programs=LIST],
      [link static the programs in LIST (comma-separated,
      supported for ]m4_defn([UL_STATIC_PROGRAMS])[)])])

AS_CASE([$enable_static_programs],
  [yes],
    [enable_static_programs=m4_quote(UL_STATIC_PROGRAMS)],
  [no],
    [enable_static_programs=]
)

dnl Set all the individual AM_CONDITIONALs
m4_foreach([UL_PRG], m4_defn([UL_STATIC_PROGRAMS]), [
  AS_CASE([,$enable_static_programs,],
   [*,UL_PRG,*], [static_[]UL_PRG=yes]
  )
  AS_IF([test "x$static_[]UL_PRG" = xyes], [AC_MSG_NOTICE([enable static build: UL_PRG.])])
  AM_CONDITIONAL([HAVE_STATIC_]m4_toupper(UL_PRG),
    [test "x$static_[]UL_PRG" = xyes])
])


AC_ARG_ENABLE([all-programs],
  AS_HELP_STRING([--disable-all-programs], [disable everything, might be overridden by --enable-<name>]),
  [], [enable_all_programs=undefined]
)

AS_CASE([$enable_all_programs],
  [yes], [AC_MSG_WARN([force to build all programs by default])
	  ul_default_estate=check],
  [no],  [AC_MSG_WARN([disable all programs and man pages by default])
	  ul_default_estate=no]
)


AC_ARG_ENABLE([asciidoc],
  AS_HELP_STRING([--disable-asciidoc], [do not generate man pages from asciidoc]),
  [], [UL_DEFAULT_ENABLE([asciidoc], [check])]
)
UL_BUILD_INIT([asciidoc])
UL_REQUIRES_PROGRAM([asciidoc], [ASCIIDOCTOR], [asciidoctor], [man pages])
AM_CONDITIONAL([ENABLE_ASCIIDOC], [test "x$build_asciidoc" = xyes])
have_asciidoctor_failure_level=no
AS_IF([test "x$build_asciidoc" = xyes], [
       AC_MSG_CHECKING([whether ${ASCIIDOCTOR} supports --failure-level])
       AS_IF([${ASCIIDOCTOR} --help | grep failure-level >/dev/null 2>&1 ], [
         AC_MSG_RESULT([yes])
         have_asciidoctor_failure_level=yes
       ], [
         AC_MSG_RESULT([no])
       ])
])
AM_CONDITIONAL([HAVE_ASCIIDOCTOR_FAILURE_LEVEL], [test "x$have_asciidoctor_failure_level" = xyes])

have_manpages=no
AS_IF([test -f "$srcdir/sys-utils/mount.8"], [
   AC_MSG_NOTICE([re-use already generated man-pages.])
   have_manpages=yes]
)
AM_CONDITIONAL([HAVE_MANPAGES], [test "x$have_manpages" = xyes])


AC_ARG_ENABLE([poman],
  AS_HELP_STRING([--disable-poman], [generate translated man pages]),
  [], [UL_DEFAULT_ENABLE([poman], [check])]
)
UL_BUILD_INIT([poman])
UL_REQUIRES_PROGRAM([poman], [PO4A], [po4a], [translated man pages])
AM_CONDITIONAL([ENABLE_POMAN], [test "x$build_poman" = xyes])


AX_CHECK_TLS

have_pty=no
AC_ARG_WITH([util], AS_HELP_STRING([--without-util], [compile without libutil]),
  [], [with_util=auto]
)
AS_IF([test "x$with_util" = xno], [
  AM_CONDITIONAL([HAVE_UTIL], [false])
  have_util=no
], [
  UL_CHECK_LIB([util], [openpty])
])

AS_IF([test "x$have_pty_h" = xyes && test "x$have_sys_signalfd_h" = xyes && test "x$have_util" = xyes], [
   AM_CONDITIONAL([HAVE_PTY], [true])
   AC_DEFINE([HAVE_PTY], [1], [have PTY support])
   have_pty=yes
], [
   AM_CONDITIONAL([HAVE_PTY], [false])
])

AC_CHECK_TYPES([union semun], [], [], [[
#include <sys/sem.h>
]])
AC_CHECK_TYPES([loff_t])


AC_ARG_ENABLE([widechar],
  AS_HELP_STRING([--disable-widechar], [do not compile wide character support]),
  [], [enable_widechar=check]
)
UL_BUILD_INIT([widechar])
UL_REQUIRES_COMPILE([widechar], [[
  #include <wchar.h>
  #include <wctype.h>
  #include <stdio.h>
  #include <stdlib.h>
  ]], [[
    wchar_t wc;
    wint_t w;
    w = fgetwc(stdin);
    if (w == WEOF) exit(1);
    wc = w;
    fputwc(wc,stdout);
  ]],
  [wchar_t support])

AS_IF([test "x$build_widechar" = xyes ], [
  AC_DEFINE([HAVE_WIDECHAR], [1], [Do we have wide character support?])
])


AC_CHECK_TYPES([cpu_set_t], [have_cpu_set_t=yes], [], [[
#include <sched.h>
]])

AM_CONDITIONAL([HAVE_CPU_SET_T], [test "x$have_cpu_set_t" = xyes])

AC_CHECK_TYPES([sighandler_t], [], [], [[
#include <signal.h>
]])

AC_CHECK_DECLS([CPU_ALLOC], [], [], [[
#include <sched.h>
]])

# on Solaris, you can't mix and match standards, since we use c99
# apparently at this stage, XOPEN_SOURCE will conflict.  As workaround,
# check for crypt.h and use that without XOPEN_SOURCE.
have_crypt=no
AC_CHECK_HEADERS([crypt.h])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#else
#define _XOPEN_SOURCE
#include <unistd.h>
#endif
]], [[
char *c = crypt("abc","pw");
]])],[
  have_libcrypt=no
  have_crypt=yes
],[
  UL_SET_LIBS([-lcrypt])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
  #ifdef HAVE_CRYPT_H
  #include <crypt.h>
  #else
  #define _XOPEN_SOURCE
  #include <unistd.h>
  #endif
  ]], [[
  char *c = crypt("abc","pw");
  ]])],[
  have_libcrypt=yes
  have_crypt=yes
  ],[
  AC_MSG_WARN([crypt() is not available])
  ])
  UL_RESTORE_LIBS
])
AM_CONDITIONAL([HAVE_LIBCRYPT], [test "x$have_libcrypt" = xyes])


AC_ARG_WITH([selinux],
  AS_HELP_STRING([--with-selinux], [compile with SELinux support]),
  [], [with_selinux=no]
)

AS_IF([test "x$with_selinux" = xno], [
  AM_CONDITIONAL([HAVE_SELINUX], [false])
], [
  PKG_CHECK_MODULES([SELINUX], [libselinux >= 2.5], [have_selinux=yes], [have_selinux=no])
  AS_CASE([$with_selinux:$have_selinux],
    [yes:no], [AC_MSG_ERROR([SELinux selected but libselinux not found or too old])]
  )
  AS_IF([test "x$have_selinux" = xyes], [
    AC_DEFINE([HAVE_LIBSELINUX], [1], [Define if SELinux is available])
    UL_PKG_STATIC([SELINUX_LIBS_STATIC], [libselinux])
    AM_CONDITIONAL([HAVE_SELINUX], [true])

    UL_SET_LIBS([$SELINUX_LIBS])
    # This function is missing in old libselinux 1.xx versions
    AC_CHECK_FUNCS([security_get_initial_context])
    UL_RESTORE_LIBS
  ])
])
AC_SUBST([SELINUX_LIBS])
AC_SUBST([SELINUX_LIBS_STATIC])


AC_ARG_WITH([audit],
  AS_HELP_STRING([--with-audit], [compile with audit support]),
  [], [with_audit=no]
)

AS_IF([test "x$with_audit" = xno], [
  AM_CONDITIONAL([HAVE_AUDIT], [false])
], [
  UL_CHECK_LIB([audit], [audit_log_user_message])
  AS_CASE([$with_audit:$have_audit],
    [yes:no],
      [AC_MSG_ERROR([Audit selected but libaudit not found (or does not support audit_log_user_message())])]
  )
])

AC_ARG_WITH([udev], AS_HELP_STRING([--without-udev], [compile without udev support]),
  [], [with_udev=auto]
)

AS_IF([test "x$with_udev" = xno], [
  AM_CONDITIONAL([HAVE_UDEV], [false])
], [
  UL_CHECK_LIB([udev], [udev_new])
  AS_CASE([$with_udev:$have_udev],
    [yes:no],
      [AC_MSG_ERROR([udev selected but libudev not found])]
  )
])


dnl wide-char ncurses
AC_ARG_WITH([ncursesw],
  AS_HELP_STRING([--without-ncursesw], [do not build with wide-char ncurses]),
  [], [with_ncursesw=auto]
)
have_ncursesw=no
have_ncursesw_header=no
AS_IF([test "x$with_ncursesw" != xno], [
  UL_NCURSES_CHECK([ncursesw])
  AS_IF([test "x$have_ncursesw" = xyes], [
    AC_CHECK_HEADERS([ncursesw/ncurses.h], [have_ncursesw_header=yes])
    AC_CHECK_HEADERS([ncursesw/term.h])

    # Define HAVE_NCURSES_H only if check for HAVE_NCURSESW_NCURSES_H is
    # unsuccessful to avoid too many *_H permitations in config.h
    AS_IF([test "x$have_ncursesw_header" = xno], [
	   AC_CHECK_HEADERS([ncurses.h], [have_ncursesw_header=yes])
	   AC_CHECK_HEADERS([term.h])
    ])
    AS_IF([test "x$have_ncursesw_header" = xno], [have_ncursesw=no])
  ])
  AS_IF([test "x$have_ncursesw" = xyes], [
    CURSES_LIB_NAME="ncursesw"
    AC_DEFINE([HAVE_LIBNCURSESW], [1], [Define if ncursesw library available])
  ])
])
AS_CASE([$with_ncursesw:$have_ncursesw],
  [yes:no], [AC_MSG_ERROR([ncursesw selected, but library not found])])


dnl non-wide ncurses
AC_ARG_WITH([ncurses],
  AS_HELP_STRING([--with-ncurses], [build with non-wide ncurses]),
  [], [with_ncurses=auto]
)
have_ncurses=no
have_ncurses_header=no
AS_CASE([$with_ncurses:$build_widechar],
  [yes:yes], [AC_MSG_ERROR([wide-char support enabled, but non-wide ncurses selects])])

AS_IF([test "x$have_ncursesw" = xno && test "x$with_ncurses" != xno ], [
  UL_NCURSES_CHECK([ncurses])
  AS_IF([test "x$have_ncurses" = xyes], [
    AC_CHECK_HEADERS([ncurses/ncurses.h], [have_ncurses_header=yes])
    AC_CHECK_HEADERS([ncurses/term.h])

    # Define HAVE_NCURSES_H only if check for HAVE_NCURSES_NCURSES_H is
    # unsuccessful to avoid too many *_H permitations in config.h
    AS_IF([test "x$have_ncurses_header" = xno], [
	   AC_CHECK_HEADERS([ncurses.h], [have_ncurses_header=yes])
	   AC_CHECK_HEADERS([term.h])
    ])
    AS_IF([test "x$have_ncurses_header" = xno], [have_ncurses=no])
  ])
  AS_IF([test "x$have_ncurses" = xyes], [
    CURSES_LIB_NAME="ncurses"
    AC_DEFINE([HAVE_LIBNCURSES], [1], [Define if ncurses library available])
  ])
])
AS_CASE([$with_ncurses:$have_ncurses],
  [yes:no], [AC_MSG_ERROR([ncurses selected, but library not found])])

AC_SUBST([NCURSES_CFLAGS])
AC_SUBST([NCURSES_LIBS])
AM_CONDITIONAL([HAVE_NCURSES], [test "x$have_ncursesw" = xyes || test "x$have_ncurses" = xyes])


AC_ARG_WITH([slang],
  AS_HELP_STRING([--with-slang], [compile cfdisk with slang rather than ncurses]),
  [], [with_slang=no]
)
have_slang=no
AS_IF([test "x$with_slang" = xyes], [
  AC_CHECK_HEADERS([slang.h slang/slang.h])
  AC_CHECK_HEADERS([slcurses.h slang/slcurses.h],
		   [have_slang=yes], [], [
#ifdef HAVE_SLANG_H
#include <slang.h>
#elif defined(HAVE_SLANG_SLANG_H)
#include <slang/slang.h>
#endif
])
  AS_IF([test "x$have_slang" = xno], [
    AC_MSG_ERROR([slang selected but slcurses.h not found])],
    [CURSES_LIB_NAME=slang
  ])
])
AM_CONDITIONAL([HAVE_SLANG], [test "x$have_slang" = xyes])


AS_IF([test "x$have_slang" = xyes || test "x$have_ncursesw" = xyes || test "x$have_ncurses" = xyes], [
	AC_CHECK_LIB([$CURSES_LIB_NAME], use_default_colors, [
		     AC_DEFINE(HAVE_USE_DEFAULT_COLORS, 1,
			       [Define if curses library has the use_default_colors().])
	])
	AC_CHECK_LIB([$CURSES_LIB_NAME], resizeterm, [
		     AC_DEFINE(HAVE_RESIZETERM, 1,
			       [Define if curses library has the resizeterm().])
	])
])


AC_ARG_WITH([tinfo], AS_HELP_STRING([--without-tinfo], [compile without libtinfo]),
  [], [with_tinfo=auto]
)
have_tinfo=no
have_tinfow=no
AS_IF([test "x$with_tinfo" != xno], [
  AS_IF([test "x$have_ncursesw" = xyes], [
    UL_TINFO_CHECK([tinfow])
  ])
  AS_IF([test "x$have_tinfow" = xno], [
    UL_TINFO_CHECK([tinfo])
  ])
])
AC_SUBST([TINFO_LIBS])
AC_SUBST([TINFO_LIBS_STATIC])
AC_SUBST([TINFO_CFLAGS])
AM_CONDITIONAL([HAVE_TINFO], [test "x$have_tinfo" = xyes || test "x$have_tinfow" = xyes])
AS_IF([test "x$have_tinfo" = xyes || test "x$have_tinfow" = xyes], [
  AC_DEFINE(HAVE_LIBTINFO, 1, [Define if libtinfo or libtinfow available.])
])


AC_ARG_WITH([readline],
  AS_HELP_STRING([--without-readline], [do not build with GNU Readline support]),
  [], [with_readline=auto]
)

AS_IF([test "x$with_readline" = xno], [
  AM_CONDITIONAL([HAVE_READLINE], [false])
], [
  UL_CHECK_LIB([readline], [readline])
  AS_CASE([$with_readline:$have_readline],
    [yes:no],
      [AC_MSG_ERROR([readline selected but libreadline not found ])],
    [*:yes],
      [READLINE_LIBS="-lreadline"
       READLINE_LIBS_STATIC="-lreadline $TINFO_LIBS_STATIC"]
  )
])
AC_SUBST([READLINE_LIBS])
AC_SUBST([READLINE_LIBS_STATIC])


AC_ARG_WITH([utempter],
  AS_HELP_STRING([--with-utempter], [compile script(1) with libutempter]),
  [], [with_utempter=no]
)

AS_IF([test "x$with_utempter" = xyes], [
  UL_CHECK_LIB([utempter], [utempter_add_record])
  AS_IF([test "x$have_utempter" = xno], [
    AC_MSG_ERROR([utempter selected but libutempter not found])
  ])
], [
  AM_CONDITIONAL([HAVE_UTEMPTER], [false])
])


dnl
dnl libuuid
dnl
AC_ARG_ENABLE([libuuid],
  AS_HELP_STRING([--disable-libuuid], [do not build libuuid and uuid utilities]),
  [], [UL_DEFAULT_ENABLE([libuuid], [yes])]
)
UL_BUILD_INIT([libuuid])
AC_SUBST([LIBUUID_VERSION])
AC_SUBST([LIBUUID_VERSION_INFO])
AM_CONDITIONAL(BUILD_LIBUUID, test "x$build_libuuid" = xyes)

AC_ARG_ENABLE([libuuid-force-uuidd],
  AS_HELP_STRING([--enable-libuuid-force-uuidd], [support uuidd even though the daemon is not built]),
  [], [enable_libuuid_force_uuidd=no]
)

AS_IF([test "x$build_libuuid" = xyes], [
  AC_DEFINE(HAVE_LIBUUID, 1, [Define to 1 if you have the -luuid.])
])

dnl
dnl liblastlog2
dnl
AC_ARG_ENABLE([liblastlog2],
  AS_HELP_STRING([--disable-liblastlog2], [do not build liblastlog2 and lastlog2 utilities]),
  [], [UL_DEFAULT_ENABLE([liblastlog2], [yes])]
)
UL_BUILD_INIT([liblastlog2])

have_sqlite3=no
AS_IF([test "x$build_liblastlog2" = xyes], [
  PKG_CHECK_MODULES([SQLITE3], [sqlite3], [have_sqlite3=yes], [have_sqlite3=no])
])
UL_REQUIRES_HAVE([liblastlog2], [sqlite3], [sqlite3 library])
AC_SUBST([SQLITE3_LIBS])

AC_SUBST([LIBLASTLOG2_VERSION])
AC_SUBST([LIBLASTLOG2_VERSION_INFO])
AC_DEFINE_UNQUOTED([LIBLASTLOG2_VERSION], ["$LIBLASTLOG2_VERSION"], [liblastlog2 version string])
AM_CONDITIONAL([BUILD_LIBLASTLOG2], [test "x$build_liblastlog2" = xyes])
AM_CONDITIONAL([BUILD_LIBLASTLOG2_TESTS], [test "x$build_liblastlog2" = xyes && test "x$enable_static" = xyes])


AC_ARG_ENABLE([pam_lastlog2],
  AS_HELP_STRING([--disable-pam-lastlog2], [do not build PAM lastlog2 module]),
  [], [UL_DEFAULT_ENABLE([pam_lastlog2], [check])]
)
UL_BUILD_INIT([pam_lastlog2])
UL_REQUIRES_BUILD([pam_lastlog2], [liblastlog2])
UL_REQUIRES_HAVE([pam_lastlog2], [security_pam_modules_h], [pam_modules.h header file])
AM_CONDITIONAL([BUILD_PAM_LASTLOG2], [test "x$build_pam_lastlog2" = xyes])


dnl
dnl libblkid
dnl
AC_ARG_ENABLE([libblkid],
  AS_HELP_STRING([--disable-libblkid], [do not build libblkid and many related utilities]),
  [], [UL_DEFAULT_ENABLE([libblkid], [check])]
)
UL_BUILD_INIT([libblkid])
UL_REQUIRES_HAVE([libblkid], [openat], [openat functions])
UL_REQUIRES_HAVE([libblkid], [dirfd,ddfd], [dirfd or ddfd function])
AC_SUBST([LIBBLKID_DATE])
AC_SUBST([LIBBLKID_VERSION])
AC_SUBST([LIBBLKID_VERSION_INFO])
AC_DEFINE_UNQUOTED([LIBBLKID_VERSION], ["$LIBBLKID_VERSION"], [libblkid version string])
AC_DEFINE_UNQUOTED([LIBBLKID_DATE], ["$LIBBLKID_DATE"], [libblkid date string])
AS_IF([test "x$build_libblkid" = xyes], [
  AC_DEFINE([HAVE_LIBBLKID], [1], [Define to 1 if you have the -lblkid.])
])
AM_CONDITIONAL([BUILD_LIBBLKID], [test "x$build_libblkid" = xyes])
AM_CONDITIONAL([BUILD_LIBBLKID_TESTS], [test "x$build_libblkid" = xyes && test "x$enable_static" = xyes])

dnl
dnl libmount
dnl
AC_ARG_ENABLE([libmount],
  AS_HELP_STRING([--disable-libmount], [do not build libmount]),
  [], [UL_DEFAULT_ENABLE([libmount], [check])]
)
UL_BUILD_INIT([libmount])
UL_REQUIRES_BUILD([libmount], [libblkid])
UL_REQUIRES_HAVE([libmount], [dirfd,ddfd], [dirfd or ddfd function])
UL_REQUIRES_HAVE([libmount], [mntent_h], [mntent.h header file])
AM_CONDITIONAL([BUILD_LIBMOUNT], [test "x$build_libmount" = xyes])
AM_CONDITIONAL([BUILD_LIBMOUNT_TESTS], [test "x$build_libmount" = xyes && test "x$enable_static" = xyes])
AS_IF([test "x$build_libmount" = xyes], [
  AC_DEFINE(HAVE_LIBMOUNT, 1, [Define if libmount available.])
  AS_IF([test "x$have_setns_syscall" = "xyes"], [
    AC_DEFINE([USE_LIBMOUNT_SUPPORT_NAMESPACES], [1], [Define to 1 if want to support namepaces.])
  ],[
    AC_MSG_WARN([libmount will be compiled without namespaces support])
  ])
])

AC_SUBST([LIBMOUNT_VERSION])
AC_SUBST([LIBMOUNT_MAJOR_VERSION], $PACKAGE_VERSION_MAJOR)
AC_SUBST([LIBMOUNT_MINOR_VERSION], $PACKAGE_VERSION_MINOR)
AC_SUBST([LIBMOUNT_PATCH_VERSION], $PACKAGE_VERSION_RELEASE)
AC_SUBST([LIBMOUNT_VERSION_INFO])
AC_DEFINE_UNQUOTED([LIBMOUNT_VERSION], ["$LIBMOUNT_VERSION"], [libmount version string])

AC_ARG_ENABLE([libmount-mountfd-support],
  AS_HELP_STRING([--disable-libmount-mountfd-support], [do not use new mount API based on FDs]),
  [], [enable_libmount_mountfd_support=check]
)
UL_BUILD_INIT([libmount_mountfd_support])
UL_REQUIRES_BUILD([libmount_mountfd_support], [libmount])
UL_REQUIRES_LINUX([libmount_mountfd_support])
UL_REQUIRES_HAVE([libmount_mountfd_support], [mountfd_api], [mount FDs based API])
UL_REQUIRES_HAVE([libmount_mountfd_support], [linux_mount_h], [linux/mount.h])
AS_IF([test "x$build_libmount_mountfd_support" = xyes ], [
  AC_DEFINE([USE_LIBMOUNT_MOUNTFD_SUPPORT], [1], [Enable support for new mount FD based kernel API])
])


dnl
dnl libsmartcols
dnl
AC_ARG_ENABLE([libsmartcols],
  AS_HELP_STRING([--disable-libsmartcols], [do not build libsmartcols]),
  [], [UL_DEFAULT_ENABLE([libsmartcols], [yes])]
)
UL_BUILD_INIT([libsmartcols])
AM_CONDITIONAL([BUILD_LIBSMARTCOLS], [test "x$build_libsmartcols" = xyes])

AC_SUBST([LIBSMARTCOLS_VERSION])
AC_SUBST([LIBSMARTCOLS_VERSION_INFO])
AC_DEFINE_UNQUOTED([LIBSMARTCOLS_VERSION], ["$LIBSMARTCOLS_VERSION"], [libsmartcols version string])


dnl
dnl libfdisk
dnl
AC_ARG_ENABLE([libfdisk],
  AS_HELP_STRING([--disable-libfdisk], [do not build libfdisk]),
  [], [UL_DEFAULT_ENABLE([libfdisk], [check])]
)
UL_BUILD_INIT([libfdisk])
UL_REQUIRES_BUILD([libfdisk], [libuuid])
AM_CONDITIONAL([BUILD_LIBFDISK], [test "x$build_libfdisk" = xyes])
AM_CONDITIONAL([BUILD_LIBFDISK_TESTS], [test "x$build_libfdisk" = xyes && test "x$enable_static" = xyes])

AS_IF([test "x$build_libblkid" = xyes],
  [LIBFDISK_PC_REQUIRES="uuid blkid"],[LIBFDISK_PC_REQUIRES="uuid"])
AC_SUBST([LIBFDISK_PC_REQUIRES])

AC_SUBST([LIBFDISK_VERSION])
AC_SUBST([LIBFDISK_MAJOR_VERSION], $PACKAGE_VERSION_MAJOR)
AC_SUBST([LIBFDISK_MINOR_VERSION], $PACKAGE_VERSION_MINOR)
AC_SUBST([LIBFDISK_PATCH_VERSION], $PACKAGE_VERSION_RELEASE)
AC_SUBST([LIBFDISK_VERSION_INFO])
AC_DEFINE_UNQUOTED([LIBFDISK_VERSION], ["$LIBFDISK_VERSION"], [libfdisk version string])


AC_ARG_ENABLE([fdisks],
  AS_HELP_STRING([--disable-fdisks], [do not build fdisk(8), sfdisk(8) and cfdisk(8)]),
  [], [UL_DEFAULT_ENABLE([fdisks], [check])]
)
enable_fdisk=$enable_fdisks
enable_sfdisk=$enable_fdisks
enable_cfdisk=$enable_fdisks

UL_BUILD_INIT([fdisk])
UL_REQUIRES_HAVE([fdisk], [openat], [openat functions])
UL_REQUIRES_HAVE([fdisk], [dirfd,ddfd], [dirfd or ddfd function])
UL_REQUIRES_BUILD([fdisk], [libfdisk])
UL_REQUIRES_BUILD([fdisk], [libsmartcols])
AM_CONDITIONAL([BUILD_FDISK], [test "x$build_fdisk" = xyes])


UL_BUILD_INIT([sfdisk])
UL_REQUIRES_HAVE([sfdisk], [openat], [openat functions])
UL_REQUIRES_HAVE([sfdisk], [dirfd,ddfd], [dirfd or ddfd function])
UL_REQUIRES_BUILD([sfdisk], [libfdisk])
UL_REQUIRES_BUILD([sfdisk], [libsmartcols])
AM_CONDITIONAL([BUILD_SFDISK], [test "x$build_sfdisk" = xyes])


UL_BUILD_INIT([cfdisk])
UL_REQUIRES_BUILD([cfdisk], [libfdisk])
UL_REQUIRES_BUILD([cfdisk], [libsmartcols])
UL_REQUIRES_HAVE([cfdisk], [open_memstream], [open_memstream function])
UL_REQUIRES_HAVE([cfdisk], [ncursesw,slang,ncurses], [ncursesw, ncurses or slang library])
AM_CONDITIONAL([BUILD_CFDISK], [test "x$build_cfdisk" = xyes])


AC_ARG_ENABLE([mount],
  AS_HELP_STRING([--disable-mount], [do not build mount(8) and umount(8)]),
  [], [UL_DEFAULT_ENABLE([mount], [check])]
)
UL_BUILD_INIT([mount])
UL_REQUIRES_LINUX([mount])
UL_REQUIRES_BUILD([mount], [libmount])
AM_CONDITIONAL([BUILD_MOUNT], [test "x$build_mount" = xyes])


AC_ARG_ENABLE([losetup],
  AS_HELP_STRING([--disable-losetup], [do not build losetup]),
  [], [UL_DEFAULT_ENABLE([losetup], [check])]
)
UL_BUILD_INIT([losetup])
UL_REQUIRES_LINUX([losetup])
UL_REQUIRES_BUILD([losetup], [libsmartcols])
AM_CONDITIONAL([BUILD_LOSETUP], [test "x$build_losetup" = xyes])

AC_ARG_ENABLE([zramctl],
  AS_HELP_STRING([--disable-zramctl], [do not build zramctl]),
  [], [UL_DEFAULT_ENABLE([zramctl], [check])]
)
UL_BUILD_INIT([zramctl])
UL_REQUIRES_LINUX([zramctl])
UL_REQUIRES_BUILD([zramctl], [libsmartcols])
AM_CONDITIONAL([BUILD_ZRAMCTL], [test "x$build_zramctl" = xyes])

AC_ARG_ENABLE([fsck],
  AS_HELP_STRING([--disable-fsck], [do not build fsck]),
  [], [UL_DEFAULT_ENABLE([fsck], [check])]
)
UL_BUILD_INIT([fsck])
UL_REQUIRES_BUILD([fsck], [libmount])
AM_CONDITIONAL([BUILD_FSCK], [test "x$build_fsck" = xyes])


AC_ARG_ENABLE([partx],
  AS_HELP_STRING([--disable-partx], [do not build addpart, delpart, partx]),
  [], [UL_DEFAULT_ENABLE([partx], [check])]
)
UL_BUILD_INIT([partx])
UL_REQUIRES_LINUX([partx])
UL_REQUIRES_BUILD([partx], [libblkid])
UL_REQUIRES_BUILD([partx], [libsmartcols])
AM_CONDITIONAL([BUILD_PARTX], [test "x$build_partx" = xyes])


AC_ARG_ENABLE([uuidd],
  AS_HELP_STRING([--disable-uuidd], [do not build the uuid daemon]),
  [], [UL_DEFAULT_ENABLE([uuidd], [check])]
)
UL_BUILD_INIT([uuidd])
UL_REQUIRES_BUILD([uuidd], [libuuid])
UL_REQUIRES_HAVE([uuidd], [timer], [timer_create function])
UL_REQUIRES_HAVE([uuidd], [sys_signalfd_h], [sys/signalfd.h header])
AS_IF([test "x$build_uuidd" = xyes || test "x$enable_libuuid_force_uuidd" = xyes], [
  AC_DEFINE([HAVE_UUIDD], [1], [Define to 1 if you want to use uuid daemon.])
])
AM_CONDITIONAL([BUILD_UUIDD], [test "x$build_uuidd" = xyes])


AC_ARG_ENABLE([uuidgen],
  AS_HELP_STRING([--disable-uuidgen], [do not build uuidgen]),
  [], [UL_DEFAULT_ENABLE([uuidgen], [check])]
)
UL_BUILD_INIT([uuidgen])
UL_REQUIRES_BUILD([uuidgen], [libuuid])
AM_CONDITIONAL([BUILD_UUIDGEN], [test "x$build_uuidgen" = xyes])

UL_BUILD_INIT([uuidparse], [check])
UL_REQUIRES_BUILD([uuidparse], [libuuid])
UL_REQUIRES_BUILD([uuidparse], [libsmartcols])
AM_CONDITIONAL([BUILD_UUIDPARSE], [test "x$build_uuidparse" = xyes])

AC_ARG_ENABLE([blkid],
  AS_HELP_STRING([--disable-blkid], [do not build blkid(8)]),
  [], [UL_DEFAULT_ENABLE([blkid], [check])]
)
UL_BUILD_INIT([blkid])
UL_REQUIRES_BUILD([blkid], [libblkid])
AM_CONDITIONAL([BUILD_BLKID], [test "x$build_blkid" = xyes])

UL_BUILD_INIT([findfs], [check])
UL_REQUIRES_BUILD([findfs], [libblkid])
AM_CONDITIONAL([BUILD_FINDFS], [test "x$build_findfs" = xyes])

AC_ARG_ENABLE([wipefs],
  AS_HELP_STRING([--disable-wipefs], [do not build wipefs]),
  [], [UL_DEFAULT_ENABLE([wipefs], [check])]
)
UL_BUILD_INIT([wipefs])
UL_REQUIRES_BUILD([wipefs], [libblkid])
UL_REQUIRES_BUILD([wipefs], [libsmartcols])
AM_CONDITIONAL([BUILD_WIPEFS], [test "x$build_wipefs" = xyes])

UL_BUILD_INIT([findmnt], [check])
UL_REQUIRES_BUILD([findmnt], [libmount])
UL_REQUIRES_BUILD([findmnt], [libblkid])
UL_REQUIRES_BUILD([findmnt], [libsmartcols])
AM_CONDITIONAL([BUILD_FINDMNT], [test "x$build_findmnt" = xyes])


AC_ARG_ENABLE([mountpoint],
  AS_HELP_STRING([--disable-mountpoint], [do not build mountpoint]),
  [], [UL_DEFAULT_ENABLE([mountpoint], [check])]
)
UL_BUILD_INIT([mountpoint])
UL_REQUIRES_BUILD([mountpoint], [libmount])
AM_CONDITIONAL([BUILD_MOUNTPOINT], [test "x$build_mountpoint" = xyes])


AC_ARG_ENABLE([fallocate],
  AS_HELP_STRING([--disable-fallocate], [do not build fallocate]),
  [], [UL_DEFAULT_ENABLE([fallocate], [check])]
)
UL_BUILD_INIT([fallocate])
UL_REQUIRES_LINUX([fallocate])
UL_REQUIRES_SYSCALL_CHECK([fallocate], [UL_CHECK_SYSCALL([fallocate])])
AM_CONDITIONAL([BUILD_FALLOCATE], [test "x$build_fallocate" = xyes])

AS_IF([test "x$build_fallocate" = xyes], [
  dnl check for valid fallocate() function
  dnl with 32 bits glibc 2.10, fallocate() exists but not fallocate64()
  dnl when _FILE_OFFSET_BITS==64, fallocate() is redirect to fallocate64()
  dnl and program can't be linked.
  dnl AC_CHECK_FUNC can't catch such errors since it's redefining
  dnl function prototype.
  AC_MSG_CHECKING([for valid fallocate() function])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_LINUX_FALLOC_H
# include <linux/falloc.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
]],[[
   long ret;
   ret = fallocate(0, FALLOC_FL_KEEP_SIZE, 0xfffffffful, 0xfffffffful);
   if (ret != 0) {
      return 1;
   }
   ]])],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_FALLOCATE], [1], [Have valid fallocate() function])],[
  AC_MSG_RESULT([no])])
])

AS_IF([test "x$build_fallocate" = xyes], [
  dnl check for valid posix_fallocate() function
  AC_MSG_CHECKING([for valid posix_fallocate() function])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_LINUX_FALLOC_H
# include <linux/falloc.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
]],[[
   long ret;
   ret = posix_fallocate(0, 0xfffffffful, 0xfffffffful);
   if (ret != 0) {
      return 1;
   }
   ]])],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_POSIX_FALLOCATE], [1], [Have valid posix_fallocate() function])],[
  AC_MSG_RESULT([no])])
])


AC_ARG_ENABLE([unshare],
  AS_HELP_STRING([--disable-unshare], [do not build unshare]),
  [], [UL_DEFAULT_ENABLE([unshare], [check])]
)
UL_BUILD_INIT([unshare])
UL_REQUIRES_LINUX([unshare])
UL_REQUIRES_SYSCALL_CHECK([unshare], [UL_CHECK_SYSCALL([unshare])])
UL_REQUIRES_HAVE([unshare], [linux_capability_h], [linux/capability.h header file])
AM_CONDITIONAL([BUILD_UNSHARE], [test "x$build_unshare" = xyes])

AC_MSG_CHECKING([for valid unshare() function])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sched.h>
]],[[
   int ret;
   ret = unshare(0);
   if (ret != 0) {
      return 1;
   }
   ]])],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_UNSHARE], [1], [Have valid unshare() function])],
[
  AC_MSG_RESULT([no])
])


AC_ARG_ENABLE([nsenter],
  AS_HELP_STRING([--disable-nsenter], [do not build nsenter]),
  [], [UL_DEFAULT_ENABLE([nsenter], [check])]
)
UL_BUILD_INIT([nsenter])
UL_REQUIRES_LINUX([nsenter])
UL_REQUIRES_HAVE([nsenter], [setns_syscall], [setns syscall])
AM_CONDITIONAL([BUILD_NSENTER], [test "x$build_nsenter" = xyes])

AC_MSG_CHECKING([for valid setns() function])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sched.h>
#include <errno.h>
]],[[
   int ret;
   ret = setns(0, 0);
   if (ret != EINVAL && ret != EBADF && ret != EPERM) {
      return 1;
   }
   ]])],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_SETNS], [1], [Have valid setns() function])],
[
  AC_MSG_RESULT([no])
])


AC_ARG_WITH([cap_ng],
  AS_HELP_STRING([--without-cap-ng], [compile without libcap-ng]),
  [], [with_cap_ng=auto]
)
AS_IF([test "x$with_cap_ng" = xno], [
  AM_CONDITIONAL([HAVE_CAP_NG], [false])
  have_cap_ng=no
],[
  UL_CHECK_LIB([cap-ng], [capng_apply], [cap_ng])
])


AC_ARG_ENABLE([setpriv],
  AS_HELP_STRING([--disable-setpriv], [do not build setpriv]),
  [], [UL_DEFAULT_ENABLE([setpriv], [check])]
)
UL_BUILD_INIT([setpriv])
UL_REQUIRES_LINUX([setpriv])
UL_REQUIRES_HAVE([setpriv], [linux_securebits_h], [linux/securebits.h header file])
UL_REQUIRES_HAVE([setpriv], [linux_capability_h], [linux/capability.h header file])
UL_REQUIRES_HAVE([setpriv], [cap_ng], [libcap-ng library])
AM_CONDITIONAL([BUILD_SETPRIV], [test "x$build_setpriv" = xyes])

AC_ARG_ENABLE([hardlink],
  AS_HELP_STRING([--disable-hardlink], [do not build hardlink]),
  [], [UL_DEFAULT_ENABLE([hardlink], [check])]
)
UL_BUILD_INIT([hardlink])
AM_CONDITIONAL([BUILD_HARDLINK], [test "x$build_hardlink" = xyes])


AC_ARG_ENABLE([eject],
  AS_HELP_STRING([--disable-eject], [do not build eject]),
  [], [UL_DEFAULT_ENABLE([eject], [check])]
)
UL_BUILD_INIT([eject])
UL_REQUIRES_LINUX([eject])
UL_REQUIRES_BUILD([eject], [libmount])
AM_CONDITIONAL([BUILD_EJECT], [test "x$build_eject" = xyes])


AC_ARG_ENABLE([agetty],
  AS_HELP_STRING([--disable-agetty], [do not build agetty]),
  [], [UL_DEFAULT_ENABLE([agetty], [check])]
)
UL_BUILD_INIT([agetty])
UL_REQUIRES_HAVE([agetty], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([agetty], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_AGETTY], [test "x$build_agetty" = xyes])
AS_IF([test "x$have_futimens" = xyes && test "x$have_inotify_init1" = xyes ], [
  AC_DEFINE([AGETTY_RELOAD], [1], [Enable agetty --reload feature])
],[
  AC_MSG_WARN([futimens or inotify_init1 not found; agetty(8) will not provide ---reload functionality])
])


AC_ARG_ENABLE([plymouth_support],
  AS_HELP_STRING([--disable-plymouth_support], [do not care about plymouth in sulogin(8) and agetty(8)]),
  [], [enable_plymouth_support=check]
)
UL_BUILD_INIT([plymouth_support])
UL_REQUIRES_HAVE([plymouth_support], [tiocglcktrmios], [TIOCGLCKTRMIOS flag])
UL_REQUIRES_HAVE([plymouth_support], [sock_cloexec], [SOCK_CLOEXEC flag])
UL_REQUIRES_HAVE([plymouth_support], [sock_nonblock], [SOCK_NONBLOCK flag])
UL_REQUIRES_HAVE([plymouth_support], [so_passcred], [SO_PASSCRED flag])
AM_CONDITIONAL([USE_PLYMOUTH_SUPPORT], [test "x$build_plymouth_support" = xyes])
AS_IF([test "x$build_plymouth_support" = xyes ], [
    AC_DEFINE([USE_PLYMOUTH_SUPPORT], [1], [Enable plymouth support feature for sulogin and agetty])
])


AC_ARG_WITH([libz],
  AS_HELP_STRING([--without-libz], [compile without libz]),
  [], [with_libz=auto]
)
AS_IF([test "x$with_libz" = xno], [have_z=no], [
  AC_CHECK_LIB([z], [crc32], [have_z=yes], [have_z=no])
])

AC_ARG_WITH([libmagic],
  AS_HELP_STRING([--without-libmagic], [compile without libmagic]),
  [], [with_libmagic=auto]
)
AS_IF([test "x$with_libmagic" = xno], [have_magic=no], [
  AC_CHECK_LIB([magic], [magic_open], [
    AC_CHECK_HEADER(magic.h, [
      AC_DEFINE([HAVE_MAGIC], [1], [Define to 1 if you have the libmagic present.])
      MAGIC_LIBS="-lmagic"
    ])
  ])
])
AC_SUBST([MAGIC_LIBS])


AC_ARG_ENABLE([cramfs],
  AS_HELP_STRING([--disable-cramfs], [do not build fsck.cramfs, mkfs.cramfs]),
  [], [UL_DEFAULT_ENABLE([cramfs], [check])]
)
UL_BUILD_INIT([cramfs])
UL_REQUIRES_HAVE([cramfs], [z], [z library])
AM_CONDITIONAL([BUILD_CRAMFS], [test "x$build_cramfs" = xyes])


AC_ARG_ENABLE([bfs],
  AS_HELP_STRING([--disable-bfs], [do not build mkfs.bfs]),
  [], [UL_DEFAULT_ENABLE([bfs], [yes])]
)
UL_BUILD_INIT([bfs])
AM_CONDITIONAL([BUILD_BFS], [test "x$build_bfs" = xyes])


AC_ARG_ENABLE([minix],
  AS_HELP_STRING([--disable-minix], [do not build fsck.minix, mkfs.minix]),
  [], [UL_DEFAULT_ENABLE([minix], [yes])]
)
UL_BUILD_INIT([minix])
AM_CONDITIONAL([BUILD_MINIX], [test "x$build_minix" = xyes])


AC_ARG_ENABLE([fdformat],
  AS_HELP_STRING([--enable-fdformat], [build fdformat]),
  [], [UL_DEFAULT_ENABLE([fdformat], [no])]
)
UL_BUILD_INIT([fdformat])
UL_REQUIRES_LINUX([fdformat])
AM_CONDITIONAL([BUILD_FDFORMAT], [test "x$build_fdformat" = xyes])


AC_ARG_ENABLE([hwclock],
  AS_HELP_STRING([--disable-hwclock], [do not build hwclock]),
  [], [UL_DEFAULT_ENABLE([hwclock], [check])]
)
have_linuxdummy=$linux_os

UL_BUILD_INIT([hwclock])
UL_REQUIRES_HAVE([hwclock], [io, linuxdummy], [ioperm iopl function or Linux])
AM_CONDITIONAL([BUILD_HWCLOCK], [test "x$build_hwclock" = xyes])

AC_ARG_ENABLE([hwclock_cmos],
  AS_HELP_STRING([--disable-hwclock-cmos], [do not use CMOS clock]),
  [], [enable_hwclock_cmos=check]
)
UL_BUILD_INIT([hwclock_cmos])
UL_REQUIRES_BUILD([hwclock_cmos], [hwclock])
UL_REQUIRES_ARCH([hwclock_cmos], [i?86-*,x86_64*])
AM_CONDITIONAL([USE_HWCLOCK_CMOS], [test "x$build_hwclock_cmos" = xyes])
AS_IF([test "x$build_hwclock_cmos" = xyes ], [
    AC_DEFINE([USE_HWCLOCK_CMOS], [1], [Define to 1 if want to use CMOS clock.])
])

AC_ARG_ENABLE([hwclock_gplv3],
  AS_HELP_STRING([--disable-hwclock-gplv3], [do not use datetime parsing GPLv3 code]),
  [], [enable_hwclock_gplv3=yes]
)
AM_CONDITIONAL([USE_HWCLOCK_GPLv3_DATETIME], [test "x$enable_hwclock_gplv3" = xyes])
AS_IF([test "x$enable_hwclock_gplv3" = xyes ], [
    AC_DEFINE([USE_HWCLOCK_GPLv3_DATETIME], [1], [use datetime parsing GPLv3 code to hwclock])
])


AC_ARG_ENABLE([mkfs],
  AS_HELP_STRING([--disable-mkfs], [do not build mkfs(8)]),
  [], [UL_DEFAULT_ENABLE([mkfs], [check])]
)
UL_BUILD_INIT([mkfs])
AM_CONDITIONAL([BUILD_MKFS], [test "x$build_mkfs" = xyes])

UL_BUILD_INIT([isosize], [yes])
AM_CONDITIONAL([BUILD_ISOSIZE], [test "x$build_isosize" = xyes])


AC_ARG_ENABLE([fstrim],
  AS_HELP_STRING([--disable-fstrim], [do not build fstrim(8)]),
  [], [UL_DEFAULT_ENABLE([fstrim], [check])]
)
UL_BUILD_INIT([fstrim])
UL_REQUIRES_LINUX([fstrim])
UL_REQUIRES_BUILD([fstrim], [libmount])
AM_CONDITIONAL([BUILD_FSTRIM], [test "x$build_fstrim" = xyes])


AC_ARG_ENABLE([swapon],
  AS_HELP_STRING([--disable-swapon], [do not build swapon(8) and swapoff(8)]),
  [], [UL_DEFAULT_ENABLE([swapon], [check])]
)
UL_BUILD_INIT([swapon])
UL_REQUIRES_LINUX([swapon])
UL_REQUIRES_SYSCALL_CHECK([swapon], [UL_CHECK_SYSCALL([swapon])], [swapon])
UL_REQUIRES_SYSCALL_CHECK([swapon], [UL_CHECK_SYSCALL([swapoff])], [swapoff])
UL_REQUIRES_BUILD([swapon], [libblkid])
UL_REQUIRES_BUILD([swapon], [libmount])
UL_REQUIRES_BUILD([swapon], [libsmartcols])
AM_CONDITIONAL([BUILD_SWAPON], [test "x$build_swapon" = xyes])
AC_CHECK_FUNCS([swapon])
AC_CHECK_FUNCS([swapoff])


UL_BUILD_INIT([lsblk], [check])
UL_REQUIRES_LINUX([lsblk])
UL_REQUIRES_BUILD([lsblk], [libblkid])
UL_REQUIRES_BUILD([lsblk], [libmount])
UL_REQUIRES_BUILD([lsblk], [libsmartcols])
AM_CONDITIONAL([BUILD_LSBLK], [test "x$build_lsblk" = xyes])


AC_ARG_ENABLE([lscpu],
  AS_HELP_STRING([--disable-lscpu], [do not build lscpu]),
  [], [UL_DEFAULT_ENABLE([lscpu], [check])]
)
UL_BUILD_INIT([lscpu])
UL_REQUIRES_LINUX([lscpu])
UL_REQUIRES_BUILD([lscpu], [libsmartcols])
UL_REQUIRES_HAVE([lscpu], [cpu_set_t], [cpu_set_t type])
AM_CONDITIONAL([BUILD_LSCPU], [test "x$build_lscpu" = xyes])

AC_ARG_ENABLE([lsfd],
  AS_HELP_STRING([--disable-lsfd], [do not build lsfd]),
  [], [UL_DEFAULT_ENABLE([lsfd], [check])]
)
UL_BUILD_INIT([lsfd])
UL_REQUIRES_LINUX([lsfd])
UL_REQUIRES_BUILD([lsfd], [libsmartcols])
AM_CONDITIONAL([BUILD_LSFD], [test "x$build_lsfd" = xyes])

AC_ARG_ENABLE([lslogins],
  AS_HELP_STRING([--disable-lslogins], [do not build lslogins]),
  [], [UL_DEFAULT_ENABLE([lslogins], [check])]
)
UL_BUILD_INIT([lslogins])
UL_REQUIRES_BUILD([lslogins], [libsmartcols])
UL_REQUIRES_HAVE([lslogins], [shadow_h], [shadow.h header])
UL_REQUIRES_HAVE([lslogins], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([lslogins], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_LSLOGINS], [test "x$build_lslogins" = xyes])


UL_BUILD_INIT([chcpu], [check])
UL_REQUIRES_LINUX([chcpu])
UL_REQUIRES_HAVE([chcpu], [cpu_set_t], [cpu_set_t type])
AM_CONDITIONAL([BUILD_CHCPU], [test "x$build_chcpu" = xyes])


AC_ARG_ENABLE([wdctl],
  AS_HELP_STRING([--disable-wdctl], [do not build wdctl]),
  [], [UL_DEFAULT_ENABLE([wdctl], [check])]
)
UL_BUILD_INIT([wdctl])
UL_REQUIRES_LINUX([wdctl])
UL_REQUIRES_BUILD([wdctl], [libsmartcols])
UL_REQUIRES_HAVE([wdctl], [linux_watchdog_h], [linux/watchdog.h header file])
AM_CONDITIONAL([BUILD_WDCTL], [test "x$build_wdctl" = xyes])


UL_BUILD_INIT([swaplabel], [check])
UL_REQUIRES_BUILD([swaplabel], [libblkid])
AM_CONDITIONAL([BUILD_SWAPLABEL], [test "x$build_swaplabel" = xyes])


UL_BUILD_INIT([mkswap], [yes])
AM_CONDITIONAL([BUILD_MKSWAP], [test "x$build_mkswap" = xyes])
AS_IF([test "x$build_mkswap" = xyes && test "x$build_libuuid" != xyes], [
  AC_MSG_WARN([uuid library is not found; mkswap(8) will not generate UUIDs])
])


AC_ARG_ENABLE([cal],
  AS_HELP_STRING([--disable-cal], [do not build cal]),
  [], [UL_DEFAULT_ENABLE([cal], [check])]
)
UL_BUILD_INIT([cal])
AM_CONDITIONAL([BUILD_CAL], [test "x$build_cal" = xyes])

AC_ARG_ENABLE([logger],
  AS_HELP_STRING([--disable-logger], [do not build logger]),
  [], [UL_DEFAULT_ENABLE([logger], [check])]
)
UL_BUILD_INIT([logger])
AM_CONDITIONAL([BUILD_LOGGER], [test "x$build_logger" = xyes])

UL_BUILD_INIT([look], [yes])
AM_CONDITIONAL([BUILD_LOOK], [test "x$build_look" = xyes])

UL_BUILD_INIT([mcookie], [yes])
AM_CONDITIONAL([BUILD_MCOOKIE], [test "x$build_mcookie" = xyes])

UL_BUILD_INIT([namei], [yes])
AM_CONDITIONAL([BUILD_NAMEI], [test "x$build_namei" = xyes])

matriplet="$($CC -print-multiarch 2>/dev/null || true)"
if test "x$matriplet" != "x"; then
  AC_DEFINE_UNQUOTED([MULTIARCHTRIPLET], ["$matriplet"],
  ["Multi-arch triplet for whereis library search path"])
fi
AC_ARG_ENABLE([whereis],
  AS_HELP_STRING([--disable-whereis], [do not build whereis]),
  [], [UL_DEFAULT_ENABLE([whereis], [check])]
)
UL_BUILD_INIT([whereis])
AM_CONDITIONAL([BUILD_WHEREIS], [test "x$build_whereis" = xyes])

AC_ARG_ENABLE([pipesz],
  AS_HELP_STRING([--disable-pipesz], [do not build pipesz]),
  [], [UL_DEFAULT_ENABLE([pipesz], [check])]
)
UL_BUILD_INIT([pipesz])
UL_REQUIRES_LINUX([pipesz])
AM_CONDITIONAL([BUILD_PIPESZ], [test "x$build_pipesz" = xyes])

UL_BUILD_INIT([fadvise], [check])
UL_REQUIRES_LINUX([fadvise])
AM_CONDITIONAL([BUILD_FADVISE], [test "x$build_fadvise" = xyes])

AC_ARG_ENABLE([waitpid],
  AS_HELP_STRING([--disable-waitpid], [do not build waitpid]),
  [], [UL_DEFAULT_ENABLE([waitpid], [check])]
)
UL_BUILD_INIT([waitpid])
UL_REQUIRES_LINUX([waitpid])
UL_REQUIRES_SYSCALL_CHECK([waitpid], [UL_CHECK_SYSCALL([pidfd_open])], [pidfd_open])
AM_CONDITIONAL([BUILD_WAITPID], [test "x$build_waitpid" = xyes])

UL_BUILD_INIT([enosys], [check])
UL_REQUIRES_LINUX([enosys])
AS_IF([test "x$build_enosys" = xyes], [
  AC_MSG_CHECKING([for valid audit arch])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([[#include "${srcdir}/include/audit-arch.h"]])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     build_enosys=no
    ])
])
AM_CONDITIONAL([BUILD_ENOSYS], [test "x$build_enosys" = xyes])

UL_BUILD_INIT([lsclocks], [check])
UL_REQUIRES_LINUX([lsclocks])
UL_REQUIRES_BUILD([lsclocks], [libsmartcols])
AM_CONDITIONAL([BUILD_LSCLOCKS], [test "x$build_lsclocks" = xyes])

UL_BUILD_INIT([getopt], [yes])
AM_CONDITIONAL([BUILD_GETOPT], [test "x$build_getopt" = xyes])


UL_BUILD_INIT([blockdev], [check])
UL_REQUIRES_LINUX([blockdev])
AM_CONDITIONAL([BUILD_BLOCKDEV], [test "x$build_blockdev" = xyes])


UL_BUILD_INIT([prlimit], [check])
UL_REQUIRES_LINUX([prlimit])
UL_REQUIRES_BUILD([prlimit], [libsmartcols])
UL_REQUIRES_SYSCALL_CHECK([prlimit], [UL_CHECK_SYSCALL([prlimit64])], [prlimit64])
AM_CONDITIONAL([BUILD_PRLIMIT], [test "x$build_prlimit" = xyes])
AS_IF([test "x$build_prlimit" = xyes], [
  dnl check for valid prlimit() function
  AC_MSG_CHECKING([for valid prlimit() function])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stddef.h>
]],[[
   int ret;
   ret = prlimit(0, RLIMIT_AS, NULL, NULL);
   if (ret != 0) {
      return 1;
   }
   ]])],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_PRLIMIT], [1], [Have valid prlimit() function])],[
  AC_MSG_RESULT([no])])
])



UL_BUILD_INIT([lslocks], [check])
UL_REQUIRES_LINUX([lslocks])
UL_REQUIRES_BUILD([lslocks], [libmount])
UL_REQUIRES_BUILD([lslocks], [libsmartcols])
AM_CONDITIONAL([BUILD_LSLOCKS], [test "x$build_lslocks" = xyes])


AC_ARG_ENABLE([switch_root],
  AS_HELP_STRING([--disable-switch_root], [do not build switch_root]),
  [], [UL_DEFAULT_ENABLE([switch_root], [check])]
)
UL_BUILD_INIT([switch_root])
UL_REQUIRES_LINUX([switch_root])
UL_REQUIRES_HAVE([switch_root], [openat], [openat function])
UL_REQUIRES_HAVE([switch_root], [dirfd,ddfd], [dirfd or ddfd function])
AM_CONDITIONAL([BUILD_SWITCH_ROOT], [test "x$build_switch_root" = xyes])


AC_ARG_ENABLE([pivot_root],
  AS_HELP_STRING([--disable-pivot_root], [do not build pivot_root]),
  [], [UL_DEFAULT_ENABLE([pivot_root], [check])]
)
UL_BUILD_INIT([pivot_root])
UL_REQUIRES_LINUX([pivot_root])
UL_REQUIRES_SYSCALL_CHECK([pivot_root], [UL_CHECK_SYSCALL([pivot_root])])
AM_CONDITIONAL([BUILD_PIVOT_ROOT], [test "x$build_pivot_root" = xyes])


UL_BUILD_INIT([flock], [check])
UL_REQUIRES_HAVE([flock], [timer], [timer_create/setitimer function])
AM_CONDITIONAL([BUILD_FLOCK], [test "x$build_flock" = xyes])


AC_ARG_ENABLE([lsmem],
  AS_HELP_STRING([--disable-lsmem], [do not build lsmem]),
  [], [UL_DEFAULT_ENABLE([lsmem], [check])]
)
UL_BUILD_INIT([lsmem])
UL_REQUIRES_LINUX([lsmem])
UL_REQUIRES_BUILD([lsmem], [libsmartcols])
AM_CONDITIONAL([BUILD_LSMEM], [test "x$build_lsmem" = xyes])


AC_ARG_ENABLE([chmem],
  AS_HELP_STRING([--disable-chmem], [do not build chmem]),
  [], [UL_DEFAULT_ENABLE([chmem], [check])]
)
UL_BUILD_INIT([chmem])
UL_REQUIRES_LINUX([chmem])
AM_CONDITIONAL([BUILD_CHMEM], [test "x$build_chmem" = xyes])


AC_ARG_ENABLE([ipcmk],
  AS_HELP_STRING([--disable-ipcmk], [do not build ipcmk]),
  [], [UL_DEFAULT_ENABLE([ipcmk], [check])]
)
UL_BUILD_INIT([ipcmk])
UL_EXCLUDE_ARCH([ipcmk], [*darwin*])
AM_CONDITIONAL([BUILD_IPCMK], [test "x$build_ipcmk" = xyes])

AC_ARG_ENABLE([ipcrm],
  AS_HELP_STRING([--disable-ipcrm], [do not build ipcrm]),
  [], [UL_DEFAULT_ENABLE([ipcrm], [check])]
)
UL_BUILD_INIT([ipcrm])
UL_EXCLUDE_ARCH([ipcrm], [*darwin*])
AM_CONDITIONAL([BUILD_IPCRM], [test "x$build_ipcrm" = xyes])

AC_ARG_ENABLE([ipcs],
  AS_HELP_STRING([--disable-ipcs], [do not build ipcs]),
  [], [UL_DEFAULT_ENABLE([ipcs], [check])]
)
UL_BUILD_INIT([ipcs])
UL_EXCLUDE_ARCH([ipcs], [*darwin*])
AM_CONDITIONAL([BUILD_IPCS], [test "x$build_ipcs" = xyes])


AC_ARG_ENABLE([irqtop],
  AS_HELP_STRING([--disable-irqtop], [do not build irqtop]),
  [], [UL_DEFAULT_ENABLE([irqtop], [check])]
)
UL_BUILD_INIT([irqtop])
UL_REQUIRES_LINUX([irqtop])
UL_REQUIRES_BUILD([irqtop], [libsmartcols])
UL_REQUIRES_HAVE([irqtop], [open_memstream], [open_memstream function])
UL_REQUIRES_HAVE([irqtop], [ncursesw,slang,ncurses], [ncursesw, ncurses or slang library])
AM_CONDITIONAL([BUILD_IRQTOP], [test "x$build_irqtop" = xyes])

AC_ARG_ENABLE([lsirq],
  AS_HELP_STRING([--disable-lsirq], [do not build lsirq]),
  [], [UL_DEFAULT_ENABLE([lsirq], [check])]
)
UL_BUILD_INIT([lsirq])
UL_REQUIRES_LINUX([lsirq])
UL_REQUIRES_BUILD([lsirq], [libsmartcols])
AM_CONDITIONAL([BUILD_LSIRQ], [test "x$build_lsirq" = xyes])


UL_BUILD_INIT([choom], [check])
UL_REQUIRES_LINUX([choom])
AM_CONDITIONAL([BUILD_CHOOM], [test "x$build_choom" = xyes])

UL_BUILD_INIT([lsipc], [check])
UL_REQUIRES_LINUX([lsipc])
UL_REQUIRES_BUILD([lsipc], [libsmartcols])
AM_CONDITIONAL([BUILD_LSIPC], [test "x$build_lsipc" = xyes])

AC_ARG_ENABLE([lsns],
  AS_HELP_STRING([--disable-lsns], [do not build lsns]),
  [], [UL_DEFAULT_ENABLE([lsns], [check])]
)
UL_BUILD_INIT([lsns])
UL_REQUIRES_LINUX([lsns])
UL_REQUIRES_BUILD([lsns], [libsmartcols])
UL_REQUIRES_BUILD([lsns], [libmount])
AM_CONDITIONAL([BUILD_LSNS], [test "x$build_lsns" = xyes])

UL_BUILD_INIT([renice], [yes])
AM_CONDITIONAL([BUILD_RENICE], [test "x$build_renice" = xyes])


AC_ARG_ENABLE([rfkill],
  AS_HELP_STRING([--disable-rfkill], [do not build rfkill]),
  [], [UL_DEFAULT_ENABLE([rfkill], [check])]
)
UL_BUILD_INIT([rfkill])
UL_REQUIRES_LINUX([rfkill])
UL_REQUIRES_BUILD([rfkill], [libsmartcols])
AM_CONDITIONAL([BUILD_RFKILL], [test "x$build_rfkill" = xyes])


UL_BUILD_INIT([setpgid], [yes])
AM_CONDITIONAL([BUILD_SETPGID], [test "x$build_setpgid" = xyes])

UL_BUILD_INIT([setsid], [yes])
AM_CONDITIONAL([BUILD_SETSID], [test "x$build_setsid" = xyes])

UL_BUILD_INIT([readprofile], [check])
UL_REQUIRES_LINUX([readprofile])
AM_CONDITIONAL([BUILD_READPROFILE], [test "x$build_readprofile" = xyes])

UL_BUILD_INIT([dmesg], [check])
UL_REQUIRES_LINUX([dmesg])
AM_CONDITIONAL([BUILD_DMESG], [test "x$build_dmesg" = xyes])

UL_BUILD_INIT([ctrlaltdel], [check])
UL_REQUIRES_LINUX([ctrlaltdel])
dnl we assume reboot() to be the 1-argument variant, because even considering
dnl widely used alternative C libraries like uclibc, dietlibc and musl,
dnl reboot() with multiple arguments is yet only found in glibc versions
dnl earlier than 2.x.
UL_REQUIRES_HAVE([ctrlaltdel], [reboot], [reboot function])
AM_CONDITIONAL([BUILD_CTRLALTDEL], [test "x$build_ctrlaltdel" = xyes])

AC_ARG_ENABLE([exch],
  AS_HELP_STRING([--disable-exch], [do not build exch]),
  [], [UL_DEFAULT_ENABLE([exch], [check])]
)
UL_BUILD_INIT([exch])
UL_REQUIRES_LINUX([exch])
UL_REQUIRES_SYSCALL_CHECK([exch], [UL_CHECK_SYSCALL([renameat2])])
AM_CONDITIONAL([BUILD_EXCH], [test "x$build_exch" = xyes])

UL_BUILD_INIT([fincore], [check])
UL_REQUIRES_LINUX([fincore])
UL_REQUIRES_BUILD([fincore], [libsmartcols])
AM_CONDITIONAL([BUILD_FINCORE], [test "x$build_fincore" = xyes])

UL_BUILD_INIT([fsfreeze], [check])
UL_REQUIRES_LINUX([fsfreeze])
AM_CONDITIONAL([BUILD_FSFREEZE], [test "x$build_fsfreeze" = xyes])

UL_BUILD_INIT([blkdiscard], [check])
UL_REQUIRES_LINUX([blkdiscard])
AM_CONDITIONAL([BUILD_BLKDISCARD], [test "x$build_blkdiscard" = xyes])

UL_BUILD_INIT([blkzone], [check])
UL_REQUIRES_LINUX([blkzone])
UL_REQUIRES_HAVE([blkzone], [linux_blkzoned_h], [linux/blkzoned.h header])
AM_CONDITIONAL([BUILD_BLKZONE], [test "x$build_blkzone" = xyes])

UL_BUILD_INIT([blkpr], [check])
UL_REQUIRES_LINUX([blkpr])
UL_REQUIRES_HAVE([blkpr], [linux_pr_h], [linux/pr.h header])
AM_CONDITIONAL([BUILD_BLKPR], [test "x$build_blkpr" = xyes])

UL_BUILD_INIT([ldattach], [check])
UL_REQUIRES_LINUX([ldattach])
AM_CONDITIONAL([BUILD_LDATTACH], [test "x$build_ldattach" = xyes])

UL_BUILD_INIT([rtcwake], [check])
UL_REQUIRES_LINUX([rtcwake])
AM_CONDITIONAL([BUILD_RTCWAKE], [test "x$build_rtcwake" = xyes])

UL_BUILD_INIT([setarch], [check])
UL_REQUIRES_LINUX([setarch])
AM_CONDITIONAL([BUILD_SETARCH], [test "x$build_setarch" = xyes])


# build_scriptutils= is just configure-only variable to control
# script, scriptreplay and scriptlive
AC_ARG_ENABLE([scriptutils],
  AS_HELP_STRING([--disable-scriptutils], [do not build script, scriptreplay and scriptlive]),
  [], [UL_DEFAULT_ENABLE([scriptutils], [check])]
)

UL_ENABLE_ALIAS([script], [scriptutils])
UL_BUILD_INIT([script])
UL_REQUIRES_HAVE([script], [pty], [openpty function (libutil)])
AM_CONDITIONAL([BUILD_SCRIPT], [test "x$build_script" = xyes])

UL_ENABLE_ALIAS([scriptreplay], [scriptutils])
UL_BUILD_INIT([scriptreplay])
AM_CONDITIONAL([BUILD_SCRIPTREPLAY], [test "x$build_scriptreplay" = xyes])

UL_ENABLE_ALIAS([scriptlive], [scriptutils])
UL_BUILD_INIT([scriptlive])
UL_REQUIRES_HAVE([scriptlive], [pty], [openpty function (libutil)])
AM_CONDITIONAL([BUILD_SCRIPTLIVE], [test "x$build_scriptlive" = xyes])


UL_BUILD_INIT([col], [check])
UL_REQUIRES_COMPILE([col], [#include <limits.h>], [__GLIBC__], [building for glibc])
AM_CONDITIONAL([BUILD_COL], [test "x$build_col" = xyes])

UL_BUILD_INIT([colcrt], [yes])
AM_CONDITIONAL([BUILD_COLCRT], [test "x$build_colcrt" = xyes])

UL_BUILD_INIT([colrm], [yes])
AM_CONDITIONAL([BUILD_COLRM], [test "x$build_colrm" = xyes])

UL_BUILD_INIT([column], [check])
UL_REQUIRES_BUILD([column], [libsmartcols])
AM_CONDITIONAL([BUILD_COLUMN], [test "x$build_column" = xyes])

UL_BUILD_INIT([hexdump], [yes])
AM_CONDITIONAL([BUILD_HEXDUMP], [test "x$build_hexdump" = xyes])

UL_BUILD_INIT([rev], [yes])
AM_CONDITIONAL([BUILD_REV], [test "x$build_rev" = xyes])


AC_ARG_ENABLE([tunelp],
  AS_HELP_STRING([--enable-tunelp], [build tunelp]),
  [], [UL_DEFAULT_ENABLE([tunelp], [no])]
)
UL_BUILD_INIT([tunelp])
UL_REQUIRES_LINUX([tunelp])
AM_CONDITIONAL([BUILD_TUNELP], [test "x$build_tunelp" = xyes])


AC_ARG_ENABLE([kill],
  AS_HELP_STRING([--disable-kill], [do not build kill]),
  [], [UL_DEFAULT_ENABLE([kill], [check])]
)
UL_BUILD_INIT([kill])
UL_REQUIRES_LINUX([kill])
AM_CONDITIONAL([BUILD_KILL], [test "x$build_kill" = xyes])


AC_ARG_ENABLE([last],
  AS_HELP_STRING([--disable-last], [do not build last]),
  [], [UL_DEFAULT_ENABLE([last], [check])]
)
UL_BUILD_INIT([last])
UL_REQUIRES_HAVE([last], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([last], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_LAST], [test "x$build_last" = xyes])


AC_ARG_ENABLE([utmpdump],
  AS_HELP_STRING([--disable-utmpdump], [do not build utmpdump]),
  [], [UL_DEFAULT_ENABLE([utmpdump], [check])]
)
UL_BUILD_INIT([utmpdump])
UL_REQUIRES_HAVE([utmpdump], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([utmpdump], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_UTMPDUMP], [test "x$build_utmpdump" = xyes])


AC_ARG_ENABLE([line],
  AS_HELP_STRING([--enable-line], [build line]),
  [], [UL_DEFAULT_ENABLE([line], [no])]
)
UL_BUILD_INIT([line])
AM_CONDITIONAL([BUILD_LINE], [test "x$build_line" = xyes])


AC_ARG_ENABLE([mesg],
  AS_HELP_STRING([--disable-mesg], [do not build mesg]),
  [], [UL_DEFAULT_ENABLE([mesg], [yes])]
)
UL_BUILD_INIT([mesg])
AM_CONDITIONAL([BUILD_MESG], [test "x$build_mesg" = xyes])


AC_ARG_ENABLE([raw],
  AS_HELP_STRING([--disable-raw], [do not build raw]),
  [], [UL_DEFAULT_ENABLE([raw], [check])]
)
UL_BUILD_INIT([raw])
UL_REQUIRES_LINUX([raw])
UL_REQUIRES_HAVE([raw], [linux_raw_h], [raw.h header file])
AM_CONDITIONAL([BUILD_RAW], [test "x$build_raw" = xyes])


AC_ARG_ENABLE([rename],
  AS_HELP_STRING([--disable-rename], [do not build rename]),
  [], [UL_DEFAULT_ENABLE([rename], [yes])]
)
UL_BUILD_INIT([rename])
AM_CONDITIONAL([BUILD_RENAME], [test "x$build_rename" = xyes])


AC_ARG_ENABLE([vipw],
  AS_HELP_STRING([--enable-vipw], [build vipw]),
  [], [UL_DEFAULT_ENABLE([vipw], [no])]
)
UL_BUILD_INIT([vipw])
UL_REQUIRES_HAVE([vipw], [shadow_h], [shadow.h header])
AM_CONDITIONAL([BUILD_VIPW], [test "x$build_vipw" = xyes])


AC_ARG_ENABLE([newgrp],
  AS_HELP_STRING([--enable-newgrp], [build newgrp]),
  [], [UL_DEFAULT_ENABLE([newgrp], [no])]
)
UL_BUILD_INIT([newgrp])
UL_REQUIRES_HAVE([newgrp], [crypt], [crypt function])
AM_CONDITIONAL([BUILD_NEWGRP], [test "x$build_newgrp" = xyes])


AC_ARG_WITH([user], AS_HELP_STRING([--without-user], [compile without libuser (remote chsh)]),
  [], [with_user=check]
)
have_user=no
AS_IF([test "x$with_user" != xno], [
  PKG_CHECK_MODULES(LIBUSER,[libuser >= 0.58], [have_user=yes], [have_user=no])
  AS_CASE([$with_user:$have_user],
    [yes:no],
      [AC_MSG_ERROR([user selected but libuser not found])],
    [*:yes],
      [AC_DEFINE([HAVE_LIBUSER], [1], [Define if libuser is available])]
  )
  # temporary solution, libuser has stupid .pc where are exported all
  # private dependencies to Requires: instead of Requires.private:
  AS_IF([test "x$have_user" = xyes], [
    LIBUSER_LIBS=-luser
  ])
])
AM_CONDITIONAL([HAVE_USER], [test "x$have_user" = xyes])

AC_ARG_ENABLE([chfn-chsh-password],
  AS_HELP_STRING([--disable-chfn-chsh-password], [do not require the user to enter the password in chfn and chsh]),
  [], [enable_chfn_chsh_password=yes]
)

AC_ARG_ENABLE([chfn-chsh],
  AS_HELP_STRING([--disable-chfn-chsh], [do not build chfn and chsh]),
  [], [UL_DEFAULT_ENABLE([chfn_chsh], [check])]
)
UL_BUILD_INIT([chfn_chsh])
UL_REQUIRES_HAVE([chfn_chsh], [shadow_h], [shadow.h header])
UL_REQUIRES_HAVE([chfn_chsh], [getusershell], [getusershell function])

AS_IF([test "x$enable_chfn_chsh_password" = xyes || test "x$have_user" = xyes], [
  UL_REQUIRES_HAVE([chfn_chsh], [security_pam_appl_h], [PAM header file])
  AC_DEFINE([CHFN_CHSH_PASSWORD], [1], [Should chfn and chsh require the user to enter the password?])
])
AM_CONDITIONAL([BUILD_CHFN_CHSH], [test "x$build_chfn_chsh" = xyes])
AM_CONDITIONAL([CHFN_CHSH_PASSWORD], [test "x$enable_chfn_chsh_password" = xyes])

AC_ARG_ENABLE([chsh-only-listed],
  AS_HELP_STRING([--disable-chsh-only-listed], [chsh: allow shells not in /etc/shells]),
  [], enable_chsh_only_listed=yes
)

AS_IF([test "x$enable_chsh_only_listed" = xyes], [
  AC_DEFINE([ONLY_LISTED_SHELLS], [1], [Should chsh allow only shells in /etc/shells?])
])


AC_ARG_ENABLE([login],
  AS_HELP_STRING([--disable-login], [do not build login]),
  [], [UL_DEFAULT_ENABLE([login], [check])]
)
UL_BUILD_INIT([login])
UL_REQUIRES_HAVE([login], [security_pam_appl_h], [PAM header file])
UL_REQUIRES_HAVE([login], [security_pam_misc_h, security_openpam_h], [PAM conversation functions])
UL_REQUIRES_HAVE([login], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([login], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_LOGIN], [test "x$build_login" = xyes])

AC_ARG_ENABLE([login-chown-vcs],
  AS_HELP_STRING([--enable-login-chown-vcs], [let login chown /dev/vcsN]),
  [], [enable_login_chown_vcs=no]
)

AS_IF([test "x$enable_login_chown_vcs" = xyes], [
  AC_DEFINE([LOGIN_CHOWN_VCS], [1], [Should login chown /dev/vcsN?])
])

AC_ARG_ENABLE([login-stat-mail],
  AS_HELP_STRING([--enable-login-stat-mail], [let login stat() the mailbox]),
  [], [enable_login_stat_mail=no]
)

AS_IF([test "x$enable_login_stat_mail" = xyes], [
  AC_DEFINE([LOGIN_STAT_MAIL], [1], [Should login stat() the mailbox?])
])


AC_ARG_ENABLE([nologin],
  AS_HELP_STRING([--disable-nologin], [do not build nologin]),
  [], [UL_DEFAULT_ENABLE([nologin], [yes])]
)
UL_BUILD_INIT([nologin])
AM_CONDITIONAL([BUILD_NOLOGIN], [test "x$build_nologin" = xyes])


AC_ARG_ENABLE([sulogin],
  AS_HELP_STRING([--disable-sulogin], [do not build sulogin]),
  [], [UL_DEFAULT_ENABLE([sulogin], [check])]
)
UL_BUILD_INIT([sulogin])
UL_REQUIRES_HAVE([sulogin], [crypt], [crypt function])
UL_REQUIRES_HAVE([sulogin], [shadow_h], [shadow.h header])
AM_CONDITIONAL([BUILD_SULOGIN], [test "x$build_sulogin" = xyes])


AC_ARG_ENABLE([su],
  AS_HELP_STRING([--disable-su], [do not build su]),
  [], [UL_DEFAULT_ENABLE([su], [check])]
)
UL_BUILD_INIT([su])
UL_REQUIRES_HAVE([su], [security_pam_appl_h], [PAM header file])
UL_REQUIRES_HAVE([su], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([su], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_SU], [test "x$build_su" = xyes])


AC_ARG_ENABLE([runuser],
  AS_HELP_STRING([--disable-runuser], [do not build runuser]),
  [], [UL_DEFAULT_ENABLE([runuser], [check])]
)
UL_BUILD_INIT([runuser])
UL_REQUIRES_HAVE([runuser], [security_pam_appl_h], [PAM header file])
UL_REQUIRES_HAVE([runuser], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([runuser], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_RUNUSER], [test "x$build_runuser" = xyes])


AC_ARG_ENABLE([ul],
  AS_HELP_STRING([--disable-ul], [do not build ul]),
  [], [UL_DEFAULT_ENABLE([ul], [check])]
)
UL_BUILD_INIT([ul])
UL_REQUIRES_HAVE([ul], [ncursesw, ncurses], [ncursesw or ncurses libraries])
AM_CONDITIONAL([BUILD_UL], [test "x$build_ul" = xyes])


AC_ARG_ENABLE([more],
  AS_HELP_STRING([--disable-more], [do not build more]),
  [], [UL_DEFAULT_ENABLE([more], [check])]
)
UL_BUILD_INIT([more])
UL_REQUIRES_HAVE([more], [ncursesw, ncurses], [ncursesw or ncurses libraries])
UL_REQUIRES_LINUX([more])
AM_CONDITIONAL([BUILD_MORE], [test "x$build_more" = xyes])


AC_ARG_ENABLE([pg],
  AS_HELP_STRING([--enable-pg], [build pg]),
  [], [UL_DEFAULT_ENABLE([pg], [no])]
)
UL_BUILD_INIT([pg])
UL_REQUIRES_HAVE([pg], [ncursesw, ncurses], [ncursesw or ncurses library])
AM_CONDITIONAL([BUILD_PG], [test "x$build_pg" = xyes])


AC_ARG_ENABLE([setterm],
  AS_HELP_STRING([--disable-setterm], [do not build setterm]),
  [], [UL_DEFAULT_ENABLE([setterm], [check])]
)
UL_BUILD_INIT([setterm])
UL_REQUIRES_LINUX([setterm])
UL_REQUIRES_HAVE([setterm], [ncursesw, ncurses], [ncursesw or ncurses library])
AM_CONDITIONAL([BUILD_SETTERM], [test "x$build_setterm" = xyes])

# build_schedutils= is just configure-only variable to control
# ionice, taskset and chrt
AC_ARG_ENABLE([schedutils],
  AS_HELP_STRING([--disable-schedutils], [do not build chrt, ionice, taskset]),
  [], [UL_DEFAULT_ENABLE([schedutils], [check])]
)


UL_ENABLE_ALIAS([ionice], [schedutils])
UL_BUILD_INIT([ionice])
UL_REQUIRES_SYSCALL_CHECK([ionice],
	[UL_CHECK_SYSCALL([ioprio_set],
	  [alpha],	[442],
	  [i*86],	[289],
	  [ia64*],	[1274],
	  [powerpc*],	[273],
	  [s390*],	[282],
	  [sparc*],	[196],
	  [sh*],	[288],
	  [x86_64*],	[251])],
	[ioprio_set])

UL_REQUIRES_SYSCALL_CHECK([ionice],
	[UL_CHECK_SYSCALL([ioprio_get],
	  [alpha],	[443],
	  [i*86],	[290],
	  [ia64*],	[1275],
	  [powerpc*],	[274],
	  [s390*],	[283],
	  [sparc*],	[218],
	  [sh*],	[289],
	  [x86_64*],	[252])],
	[ioprio_get])

AM_CONDITIONAL([BUILD_IONICE], [test "x$build_ionice" = xyes])


UL_ENABLE_ALIAS([taskset], [schedutils])
UL_BUILD_INIT([taskset])
UL_REQUIRES_HAVE([taskset], [cpu_set_t], [cpu_set_t type])
UL_REQUIRES_SYSCALL_CHECK([taskset],
	[UL_CHECK_SYSCALL([sched_getaffinity])],
	[sched_getaffinity])
AM_CONDITIONAL([BUILD_TASKSET], [test "x$build_taskset" = xyes])


have_schedsetter=no
AS_IF([test "x$ac_cv_func_sched_setscheduler" = xyes], [have_schedsetter=yes],
      [test "x$ac_cv_func_sched_setattr" = xyes], [have_schedsetter=yes])

UL_ENABLE_ALIAS([chrt], [schedutils])
UL_BUILD_INIT([chrt])
UL_REQUIRES_HAVE([chrt], [schedsetter], [sched_set functions])
AM_CONDITIONAL([BUILD_CHRT], [test "x$build_chrt" = xyes])

AS_IF([test "x$build_chrt" = xyes], [
	UL_CHECK_SYSCALL([sched_setattr])
])

UL_ENABLE_ALIAS([uclampset], [schedutils])
UL_BUILD_INIT([uclampset])
UL_REQUIRES_SYSCALL_CHECK([uclampset],
	[UL_CHECK_SYSCALL([sched_setattr])], [sched_setattr])
AM_CONDITIONAL([BUILD_UCLAMPSET], [test "x$build_uclampset" = xyes])


AC_ARG_ENABLE([wall],
  AS_HELP_STRING([--disable-wall], [do not build wall]),
  [], [UL_DEFAULT_ENABLE([wall], [check])]
)
UL_BUILD_INIT([wall])
UL_REQUIRES_HAVE([wall], [utmpx_h], [utmpx.h header])
AM_CONDITIONAL([BUILD_WALL], [test "x$build_wall" = xyes])


AC_ARG_ENABLE([write],
  AS_HELP_STRING([--enable-write], [build write]),
  [], [UL_DEFAULT_ENABLE([write], [no])]
)
UL_BUILD_INIT([write])
UL_REQUIRES_HAVE([write], [utmpx_h], [utmpx.h header])
UL_REQUIRES_HAVE([write], [gnu_utmpx], [GNU utmpx functions])
AM_CONDITIONAL([BUILD_WRITE], [test "x$build_write" = xyes])


AC_ARG_WITH([btrfs],
  AS_HELP_STRING([--without-btrfs], [do not build with btrfs support]),
  [], [with_btrfs=check]
)
have_btrfs=no
AS_IF([test "x$with_btrfs" != xno], [
  AS_CASE([$with_btrfs:$have_linux_btrfs_h],
    [yes:no],
    [AC_MSG_ERROR([btrfs selected but linux/btrfs.h not found])],
    [check:no],
       [AC_MSG_WARN([linux/btrfs.h not found, do not build with btrfs support])],
    [*:yes],
       [have_btrfs=yes
	AC_DEFINE([HAVE_BTRFS_SUPPORT], [1], [Define if btrfs stuff is available])]
  )
])
AM_CONDITIONAL([HAVE_BTRFS], [test "x$have_btrfs" = xyes])


AC_ARG_WITH([systemd],
  AS_HELP_STRING([--without-systemd], [do not build with systemd support]),
  [], [with_systemd=check]
)

have_systemd=no
AS_IF([test "x$with_systemd" != xno], [
  # new version -- all libsystemd-* libs merged into libsystemd
  PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [have_systemd=yes], [have_systemd=no])
  # old versions
  AS_IF([test "x$have_systemd" != "xyes"], [
    PKG_CHECK_MODULES([SYSTEMD_DAEMON], [libsystemd-daemon],
		      [have_systemd_daemon=yes], [have_systemd_daemon=no])
    PKG_CHECK_MODULES([SYSTEMD_JOURNAL], [libsystemd-journal],
		      [have_systemd_journal=yes], [have_systemd_journal=no])
    AS_IF([test "x$have_systemd_daemon" = "xyes" && test "x$have_systemd_journal" = "xyes" ],[
	   have_systemd=yes])
  ])
  AS_CASE([$with_systemd:$have_systemd],
    [yes:no],
      [AC_MSG_ERROR([systemd expected but libsystemd not found])],
    [*:yes],
       AC_DEFINE([HAVE_LIBSYSTEMD], [1], [Define if libsystemd is available])
       AC_DEFINE([USE_SYSTEMD], [1], [Define if systemd support is wanted ])
       AC_CHECK_DECLS([sd_session_get_username], [], [], [#include <systemd/sd-login.h>])
  )
])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$have_systemd" = xyes])


AC_ARG_WITH([systemdsystemunitdir],
  AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [directory for systemd unit files (requires systemd support)]),
  [], [
  AS_IF([test "x$have_systemd" = xyes], [
    PKG_CHECK_VAR([with_systemdsystemunitdir], [systemd], [systemdsystemunitdir],
      [],
      [with_systemdsystemunitdir=no])
  ])
])
AS_IF([test "x$with_systemdsystemunitdir" != "xno"], [
  AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
])

AC_ARG_WITH([tmpfilesdir],
  AS_HELP_STRING([--with-tmpfilesdir=DIR], [directory for tmpfiles. See tmpfiles.d(5) for details]),
  [], [
  AS_IF([test "x$have_systemd" = xyes], [
    PKG_CHECK_VAR([with_tmpfilesdir], [systemd], [tmpfilesdir],
      [],
      [with_tmpfilesdir=no])
  ])
])
AS_IF([test "x$with_tmpfilesdir" != "xno"], [
  AC_SUBST([tmpfilesdir], [$with_tmpfilesdir])
])

AC_ARG_WITH([smack],
  AS_HELP_STRING([--with-smack], [build with SMACK support]),
  [], [with_smack=no]
)
AS_IF([test "x$with_smack" = xyes], [
  AC_DEFINE([HAVE_SMACK], [1], [Add SMACK support])
])

AC_ARG_WITH([econf],
  AS_HELP_STRING([--without-econf], [do not use libeconf]),
  [], [with_econf=check]
)

have_econf=no
AS_IF([test "x$with_econf" != xno], [
  PKG_CHECK_MODULES([ECONF], [libeconf], [have_econf=yes], [have_econf=no])
    AS_CASE([$with_econf:$have_econf],
    [yes:no],
      [AC_MSG_ERROR([libeconf expected but libeconf not found])],
    [*:yes],
       AC_DEFINE([HAVE_LIBECONF], [1], [Define if libeconf is available])
  )
])
AM_CONDITIONAL([HAVE_ECONF], [test "x$have_econf" = xyes])

AC_ARG_WITH([vendordir],
  AS_HELP_STRING([--with-vendordir=DIR], [Directory for distribution provided configuration files]),
  [], []
)
AC_SUBST([vendordir], [$with_vendordir])
AM_CONDITIONAL([USE_VENDORDIR], [test "x$with_vendordir" != x])
AS_IF([test "x$with_vendordir" != x], [
  AC_DEFINE([USE_VENDORDIR], [1], [Define to 1 to use vendordir])
])


AC_ARG_ENABLE([bash-completion],
  AS_HELP_STRING([--disable-bash-completion], [do not install bash completion files]),
  [], [enable_bash_completion=yes]
)

AM_CONDITIONAL([BUILD_BASH_COMPLETION], [test "x$enable_bash_completion" = xyes])

AC_ARG_WITH([bashcompletiondir],
  AS_HELP_STRING([--with-bashcompletiondir=DIR], [Bash completions directory]),
  [], [
  AS_IF([test "x$enable_bash_completion" = xyes], [
    PKG_CHECK_VAR([with_bashcompletiondir], [bash-completion], [completionsdir],
      [],
      [with_bashcompletiondir=${datadir}/bash-completion/completions])
  ])
])
AS_IF([test "x$enable_bash_completion" = xyes], [
  AC_SUBST([bashcompletiondir], [$with_bashcompletiondir])
])


AC_ARG_WITH([python],
  AS_HELP_STRING([--without-python], [do not build python bindings, use --with-python={2,3} to force version]),
  [], [with_python=check withval=check]
)

have_python=no
have_libpython=no
AS_IF([test "x$with_python" != xno], [
  # We follow distributions default and look for PKG module name "python"
  # (minimal version is 2) but if major version is explicitly specified by
  # --with-python=2 or --with-python=3 then we look for PKG module "python2" or
  # "python3".
  AS_IF([test "x$withval" != xno && test "x$withval" != xyes && test "x$withval" != xcheck],
	[pymajor="$withval"; PYTHON=python${pymajor}], [pymajor="2"])

  # check for python interpreter
  AM_PATH_PYTHON([$pymajor], [have_python=yes], [have_python=no])
  AS_CASE([$with_python:$have_python],
    [yes:no],
      [AC_MSG_ERROR([python selected but python interpreter not found])],
    [check:no],
       [AC_MSG_WARN([python interpreter not found, do not build python bindings])],
    [*:yes],
       # check for python development stuff
       [
         PKG_CHECK_MODULES([PYTHON], [python-$PYTHON_VERSION], [have_libpython=yes], [have_libpython=no])

         # https://github.com/util-linux/util-linux/issues/2366
         AX_COMPARE_VERSION([$PYTHON_VERSION], [ge], [3.12], [
           UL_WARN_ADD([-Wno-error=redundant-decls], [PYTHON_CFLAGS])
         ])
       ]
  )
])


AC_ARG_ENABLE([pylibmount],
  AS_HELP_STRING([--disable-pylibmount], [do not build pylibmount]),
  [], [UL_DEFAULT_ENABLE([pylibmount], [check])]
)
UL_BUILD_INIT([pylibmount])
UL_REQUIRES_HAVE([pylibmount], [libpython], [libpython])
UL_REQUIRES_BUILD([pylibmount], [libmount])
AM_CONDITIONAL([BUILD_PYLIBMOUNT], [test "x$build_pylibmount" = "xyes"])

# We need to introduce a verbatim dependency into the Makefile, without automake
# trying to interpret it, so push it as a AM_SUBST_NOTMAKE variable.
verbatim_extra_dependency='

# pylibmountexec module must be installed after usrlib_exec libraries,
# otherwise the libtool relink step will fail to find libmount.la and
# will try -lmount which is possibly not available.
install-pylibmountexecLTLIBRARIES: install-usrlib_execLTLIBRARIES
install-usrsecurelib_execLTLIBRARIES: install-usrlib_execLTLIBRARIES

'
AC_SUBST([verbatim_extra_dependency])
AM_SUBST_NOTMAKE([verbatim_extra_dependency])

AC_ARG_ENABLE([pg-bell],
  AS_HELP_STRING([--disable-pg-bell], [let pg not ring the bell on invalid keys]),
  [], [enable_pg_bell=yes]
)

AS_IF([test "x$enable_pg_bell" = xyes], [
  AC_DEFINE([PG_BELL], [1], [Should pg ring the bell on invalid keys?])
])


AC_DEFUN([FS_PATHS_DEFAULT], [/sbin:/sbin/fs.d:/sbin/fs])
AC_ARG_ENABLE([fs-paths-default],
  AS_HELP_STRING([--enable-fs-paths-default=paths], [default search path for fs helpers @<:@FS_PATHS_DEFAULT@:>@]),
  [AS_CASE([$enableval],
    [yes], [fs_paths_defaults="FS_PATHS_DEFAULT"],
    [no],  [fs_paths_defaults=""],
    [fs_paths_defaults="$enableval"]
  )],
  [fs_paths_defaults="FS_PATHS_DEFAULT"]
)
AC_ARG_ENABLE([fs-paths-extra],
  AS_HELP_STRING([--enable-fs-paths-extra=paths], [additional search paths for fs helpers]),
  [AS_CASE([$enableval],
     [yes|no], [fs_paths_extra=""],
     [fs_paths_extra="$enableval"]
  )],
  [fs_paths_extra=""]
)
fs_paths="$fs_paths_defaults"
AS_IF([test "x$fs_paths_extra" != "x"], [
  AS_IF([test "x$fs_paths" != "x"], [
    fs_paths="${fs_paths}:"
  ])
  fs_paths="${fs_paths}${fs_paths_extra}"
])
AC_DEFINE_UNQUOTED([FS_SEARCH_PATH], "$fs_paths", [search path for fs helpers])


AC_ARG_ENABLE([use-tty-group],
  AS_HELP_STRING([--disable-use-tty-group], [do not install wall and write setgid tty]),
  [], [enable_use_tty_group=yes]
)
AM_CONDITIONAL([USE_TTY_GROUP], [test "x$enable_use_tty_group" = xyes])

AS_IF([test "x$enable_use_tty_group" = xyes], [
  AC_DEFINE([USE_TTY_GROUP], [1], [Should wall and write be installed setgid tty?])
])


AC_ARG_ENABLE([sulogin-emergency-mount],
  AS_HELP_STRING([--enable-sulogin-emergency-mount],
		 [use emergency mount of /dev and /proc for sulogin]),
  [], [enable_sulogin_emergency_mount=no]
)

AS_IF([test "x$enable_sulogin_emergency_mount" = xyes], [
  AS_IF([test "x$linux_os" = xyes], [ ],
	[AC_MSG_ERROR([--enable-sulogin-emergency-mount selected for non-linux system])])
  AC_DEFINE([USE_SULOGIN_EMERGENCY_MOUNT], [1],
	    [Should sulogin use an emergency mount of /dev and /proc?])
])


AC_ARG_ENABLE([usrdir-path],
  AS_HELP_STRING([--enable-usrdir-path], [use only /usr paths in PATH env. variable (recommended on systems with /bin -> /usr/bin symlinks)]),
  [], [enable_usrdir_path=no]
)

AS_IF([test "x$enable_usrdir_path" = xyes], [
  AC_DEFINE([USE_USRDIR_PATHS_ONLY], [1], [Define to 1 to remove /bin and /sbin from PATH env.variable])
])


AC_ARG_ENABLE([makeinstall-chown],
  AS_HELP_STRING([--disable-makeinstall-chown], [do not do chown-like operations during "make install"]),
  [], [enable_makeinstall_chown=yes]
)
AM_CONDITIONAL([MAKEINSTALL_DO_CHOWN], [test "x$enable_makeinstall_chown" = xyes])


AC_ARG_ENABLE([makeinstall-setuid],
  AS_HELP_STRING([--disable-makeinstall-setuid], [do not do setuid chmod operations during "make install"]),
  [], [enable_makeinstall_setuid=yes]
)
AM_CONDITIONAL([MAKEINSTALL_DO_SETUID], [test "x$enable_makeinstall_setuid" = xyes])


AC_ARG_ENABLE([colors-default],
  AS_HELP_STRING([--disable-colors-default], [do not colorize output from utils by default]),
  [], [enable_colors_default=yes]
)
AS_IF([test "x$enable_colors_default" = xyes], [
  AC_DEFINE([USE_COLORS_BY_DEFAULT], [1], [Enables colorized output from utils by default])
])


AC_ARG_WITH([cryptsetup],
  AS_HELP_STRING([--with-cryptsetup], [compile with cryptsetup support]),
  [], [with_cryptsetup=no]
)

AS_IF([test "x$with_cryptsetup" = xno], [
  AM_CONDITIONAL([HAVE_CRYPTSETUP], [false])
  AM_CONDITIONAL([CRYPTSETUP_VIA_DLOPEN], [false])
], [
  PKG_CHECK_MODULES([CRYPTSETUP], [libcryptsetup],
	[AC_DEFINE([HAVE_CRYPTSETUP], [1], [Define if cryptsetup is available])
	 UL_PKG_STATIC([CRYPTSETUP_LIBS_STATIC], [libcryptsetup])
	 AM_CONDITIONAL([HAVE_CRYPTSETUP], [true])
	 SAVE_CFLAGS="$CFLAGS"
	 CFLAGS="$CRYPTSETUP_CFLAGS $CFLAGS"
	 SAVE_LIBS="$LIBS"
	 LIBS="$CRYPTSETUP_LIBS $LIBS"
	 AC_CHECK_LIB([cryptsetup], [crypt_activate_by_signed_key], [
	  AC_DEFINE([HAVE_CRYPT_ACTIVATE_BY_SIGNED_KEY], [1], [Define if crypt_activate_by_signed_key exist in -lcryptsetup])
	 ])
	 AS_IF([test "x$with_cryptsetup" = xdlopen], [
	  LIBS="-ldl $LIBS"
	  AC_CHECK_LIB([dl], [dlsym], [
	    AC_DEFINE([CRYPTSETUP_VIA_DLOPEN], [1], [Define if cryptsetup is to be loaded via dlopen])
	    AM_CONDITIONAL([CRYPTSETUP_VIA_DLOPEN], [true])
	   ], [AC_MSG_ERROR([libdl required to build with cryptsetup support])])
	 ], [
	  AM_CONDITIONAL([CRYPTSETUP_VIA_DLOPEN], [false])
	 ])
	 CFLAGS="$SAVE_CFLAGS"
	 LIBS="$SAVE_LIBS"
	 have_cryptsetup=yes],
	[have_cryptsetup=no
   AM_CONDITIONAL([HAVE_CRYPTSETUP], [false])
   AM_CONDITIONAL([CRYPTSETUP_VIA_DLOPEN], [false])
  ])

  AS_CASE([$with_cryptsetup:$have_cryptsetup],
    [yes:no], [AC_MSG_WARN([cryptsetup selected but libcryptsetup not found])]
  )
])
AC_SUBST([CRYPTSETUP_LIBS])
AC_SUBST([CRYPTSETUP_LIBS_STATIC])


AC_ARG_VAR([SUID_CFLAGS],
	   [CFLAGS used for binaries which are usually with the suid bit])
AC_ARG_VAR([SUID_LDFLAGS],
	   [LDFLAGS used for binaries which are usually with the suid bit])

AC_ARG_VAR([DAEMON_CFLAGS],
	   [CFLAGS used for binaries which are usually executed as daemons])
AC_ARG_VAR([DAEMON_LDFLAGS],
	   [LDFLAGS used for binaries which are usually executed as daemons])

AC_ARG_VAR([SOLIB_CFLAGS],
	   [CFLAGS used for shared libraries])
AC_ARG_VAR([SOLIB_LDFLAGS],
	   [LDFLAGS used for shared libraries])

AC_ARG_VAR([ADJTIME_PATH],
	   [Path to hwclock adjtime file, default /etc/adjtime])
AS_IF([test "x$ADJTIME_PATH" = x], [ADJTIME_PATH="/etc/adjtime"])
AC_DEFINE_UNQUOTED([CONFIG_ADJTIME_PATH], "$ADJTIME_PATH", [Path to hwclock adjtime file])

if test "x${build_alias}" != "x${host_alias}"; then
  AC_MSG_NOTICE([setting link_all_deplibs=unknown for libtool])
  # work around Debian patch to libtool breaking cross-compiles
  # https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=702737
  # https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=297726
  link_all_deplibs=unknown
fi


LIBS=""


AC_CONFIG_HEADERS([config.h])

#
# Don't use configure.ac to replace install paths! See Makefile PATHFILES for
# more details.
#
AC_CONFIG_FILES([
Makefile
libblkid/docs/Makefile
libblkid/docs/version.xml
libblkid/src/blkid.h
libfdisk/docs/Makefile
libfdisk/docs/version.xml
libfdisk/src/libfdisk.h
libmount/docs/Makefile
libmount/docs/version.xml
libmount/src/libmount.h
libsmartcols/docs/Makefile
libsmartcols/docs/version.xml
libsmartcols/src/libsmartcols.h
po/Makefile.in
po-man/Makefile
])

AC_OUTPUT

AC_MSG_RESULT([
        ${PACKAGE}  ${VERSION}

        prefix:                    ${prefix}
        exec prefix:               ${exec_prefix}

        bindir:                    ${bindir}
        sbindir:                   ${sbindir}
        libdir:                    ${libdir}
        usrbin_execdir:            ${usrbin_execdir}
        usrsbin_execdir:           ${usrsbin_execdir}
        usrlib_execdir:            ${usrlib_execdir}

        localstatedir:             ${localstatedir}
        runstatedir:               ${runstatedir}
        includedir:                ${includedir}
        sysconfdir:                ${sysconfdir}
        sysconfstaticdir:          ${sysconfstaticdir}

        vendordir:                 ${with_vendordir}

        Python:                    ${PYTHON}
        Python version:            ${PYTHON_VERSION}
        Python libs:               ${pyexecdir}

        Bash completions:          ${with_bashcompletiondir}
        Systemd support:           ${have_systemd}
        Systemd unitdir:           ${with_systemdsystemunitdir}
        tmpfilesdir:               ${with_tmpfilesdir}
        libeconf support:          ${have_econf}
        Btrfs support:             ${have_btrfs}
        lastlog2 support:          ${build_liblastlog2}
        Wide-char support:         ${build_widechar}
        libcryptsetup support:     ${have_cryptsetup}

        Manual pages:              ${build_asciidoc}
        Manual pages translated:   ${build_poman}

        compiler:                  ${CC} / ${CXX}
        suid cflags:               ${SUID_CFLAGS}
        ldflags:                   ${LDFLAGS}
        suid ldflags:              ${SUID_LDFLAGS}
        ASAN enabled:              ${enable_asan}
        Fuzzing enabled:           ${enable_fuzzing_engine}
        Coverage enabled           ${enable_coverage}

        cflags:                    ${CFLAGS}

        warnings:

${WARN_CFLAGS}

 Type 'make' or 'make <utilname>' to compile.
])
