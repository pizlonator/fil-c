
AUTOMAKE_OPTIONS = 1.0 foreign

DEFS =

noinst_LTLIBRARIES = libshadow.la

if USE_PAM
LIBCRYPT_PAM = $(LIBCRYPT)
else
LIBCRYPT_PAM =
endif

AM_CPPFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir) $(ECONF_CPPFLAGS)

libshadow_la_CPPFLAGS = $(ECONF_CPPFLAGS)
if HAVE_VENDORDIR
libshadow_la_CPPFLAGS += -DVENDORDIR=\"$(VENDORDIR)\"
endif

libshadow_la_CPPFLAGS += -I$(top_srcdir)
libshadow_la_CFLAGS = $(LIBBSD_CFLAGS) $(LIBCRYPT_PAM) $(LIBSYSTEMD)
libshadow_la_LIBADD = $(LIBADD_DLOPEN)

libshadow_la_SOURCES = \
	addgrps.c \
	adds.c \
	adds.h \
	age.c \
	agetpass.c \
	agetpass.h \
	alloc.c \
	alloc.h \
	atoi/a2i.c \
	atoi/a2i.h \
	atoi/str2i.c \
	atoi/str2i.h \
	atoi/strtoi.c \
	atoi/strtoi.h \
	atoi/strtou_noneg.c \
	atoi/strtou_noneg.h \
	attr.h \
	audit_help.c \
	basename.c \
	bit.c \
	bit.h \
	cast.h \
	chkname.c \
	chkname.h \
	chowndir.c \
	chowntty.c \
	cleanup.c \
	cleanup_group.c \
	cleanup_user.c \
	commonio.c \
	commonio.h \
	console.c \
	copydir.c \
	csrand.c \
	defines.h \
	encrypt.c \
	env.c \
	exitcodes.h \
	faillog.h \
	failure.c \
	failure.h \
	fd.c \
	fields.c \
	find_new_gid.c \
	find_new_uid.c \
	find_new_sub_gids.c \
	find_new_sub_uids.c \
	fputsx.c \
	get_gid.c \
	get_pid.c \
	get_uid.c \
	getdate.h \
	getdate.y \
	getdef.c \
	getdef.h \
	getgr_nam_gid.c \
	getrange.c \
	gettime.c \
	groupio.c \
	groupmem.c \
	groupio.h \
	gshadow.c \
	hushed.c \
	idmapping.h \
	idmapping.c \
	isexpired.c \
	limits.c \
	list.c \
	lockpw.c \
	loginprompt.c \
	mail.c \
	memzero.c \
	memzero.h \
	motd.c \
	must_be.h \
	myname.c \
	nss.c \
	nscd.c \
	nscd.h \
	obscure.c \
	pam_defs.h \
	pam_pass.c \
	pam_pass_non_interactive.c \
	port.c \
	port.h \
	prefix_flag.c \
	prototypes.h \
	pwauth.c \
	pwauth.h \
	pwio.c \
	pwio.h \
	pwd_init.c \
	pwd2spwd.c \
	pwdcheck.c \
	pwmem.c \
	remove_tree.c \
	root_flag.c \
	run_part.h \
	run_part.c \
	salt.c \
	selinux.c \
	semanage.c \
	setugid.c \
	setupenv.c \
	sgetgrent.c \
	sgetpwent.c \
	sgetspent.c \
	sgroupio.c \
	sgroupio.h\
	shadow.c \
	shadowio.c \
	shadowio.h \
	shadowlog.c \
	shadowlog.h \
	shadowlog_internal.h \
	shadowmem.c \
	shell.c \
	sizeof.h \
	spawn.c \
	sssd.c \
	sssd.h \
	string/sprintf.c \
	string/sprintf.h \
	string/stpecpy.c \
	string/stpecpy.h \
	string/stpeprintf.c \
	string/stpeprintf.h \
	string/strftime.c \
	string/strftime.h \
	string/strncpy.h \
	string/strtcpy.c \
	string/strtcpy.h \
	string/zustr2stp.h \
	strtoday.c \
	sub.c \
	subordinateio.h \
	subordinateio.c \
	sulog.c \
	time/day_to_str.c \
	time/day_to_str.h \
	ttytype.c \
	tz.c \
	ulimit.c \
	user_busy.c \
	valid.c \
	write_full.c \
	xgetpwnam.c \
	xprefix_getpwnam.c \
	xgetpwuid.c \
	xgetgrnam.c \
	xgetgrgid.c \
	xgetspnam.c \
	yesno.c

if WITH_TCB
libshadow_la_SOURCES += tcbfuncs.c tcbfuncs.h
endif

if WITH_BTRFS
libshadow_la_SOURCES += btrfs.c
endif

if ENABLE_LASTLOG
libshadow_la_SOURCES += log.c
endif

if ENABLE_LOGIND
libshadow_la_SOURCES += logind.c
else
libshadow_la_SOURCES += utmp.c
endif

if !WITH_LIBBSD
libshadow_la_SOURCES += \
	freezero.h \
	freezero.c \
	readpassphrase.h \
	readpassphrase.c
endif

# These files are unneeded for some reason, listed in
# order of appearance:
#
# sources for dbm support (not yet used)

EXTRA_DIST = \
	.indent.pro \
	gshadow_.h \
	xgetXXbyYY.c
