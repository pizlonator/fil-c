
# subdirectories for translated manual pages
if USE_NLS
SUBDIRS = po cs da de es fi fr hu id it ja ko pl pt_BR ru sv tr uk zh_CN zh_TW
else
SUBDIRS =
endif

man_MANS = \
	man1/chage.1 \
	man1/chfn.1 \
	man8/chgpasswd.8 \
	man8/chpasswd.8 \
	man1/chsh.1 \
	man1/expiry.1 \
	man5/faillog.5 \
	man8/faillog.8 \
	man3/getspnam.3 \
	man1/gpasswd.1 \
	man8/groupadd.8 \
	man8/groupdel.8 \
	man8/groupmems.8 \
	man8/groupmod.8 \
	man1/groups.1 \
	man8/grpck.8 \
	man8/grpconv.8 \
	man8/grpunconv.8 \
	man5/gshadow.5 \
	man1/login.1 \
	man5/login.defs.5 \
	man8/logoutd.8 \
	man1/newgrp.1 \
	man8/newusers.8 \
	man8/nologin.8 \
	man1/passwd.1 \
	man5/passwd.5 \
	man8/pwck.8 \
	man8/pwconv.8 \
	man8/pwunconv.8 \
	man1/sg.1 \
	man3/shadow.3 \
	man5/shadow.5 \
	man5/suauth.5 \
	man8/useradd.8 \
	man8/userdel.8 \
	man8/usermod.8 \
	man8/vigr.8 \
	man8/vipw.8

if ENABLE_LASTLOG
man_MANS += man8/lastlog.8
endif

man_nopam = \
	man5/limits.5 \
	man5/login.access.5 \
	man5/porttime.5

if WITH_SU
man_MANS += man1/su.1
endif

if !USE_PAM
man_MANS += $(man_nopam)
endif

man_subids = \
	man1/getsubids.1 \
	man1/newgidmap.1 \
	man1/newuidmap.1 \
	man5/subgid.5 \
	man5/subuid.5

if ENABLE_SUBIDS
man_MANS += $(man_subids)
endif

man_XMANS = \
	chage.1.xml \
	chfn.1.xml \
	chgpasswd.8.xml \
	chpasswd.8.xml \
	chsh.1.xml \
	expiry.1.xml \
	faillog.5.xml \
	faillog.8.xml \
	getsubids.1.xml \
	gpasswd.1.xml \
	groupadd.8.xml \
	groupdel.8.xml \
	groupmems.8.xml \
	groupmod.8.xml \
	groups.1.xml \
	grpck.8.xml \
	gshadow.5.xml \
	limits.5.xml \
	login.1.xml \
	login.access.5.xml \
	login.defs.5.xml \
	logoutd.8.xml \
	newgidmap.1.xml \
	newgrp.1.xml \
	newuidmap.1.xml \
	newusers.8.xml \
	nologin.8.xml \
	passwd.1.xml \
	passwd.5.xml \
	porttime.5.xml \
	pwck.8.xml \
	pwconv.8.xml \
	shadow.3.xml \
	shadow.5.xml \
	sg.1.xml \
	su.1.xml \
	suauth.5.xml \
	subgid.5.xml \
	subuid.5.xml \
	useradd.8.xml \
	userdel.8.xml \
	usermod.8.xml \
	vipw.8.xml

if ENABLE_LASTLOG
man_XMANS += lastlog.8.xml
endif

login_defs_v = \
	BCRYPT_MIN_ROUNDS.xml \
	CHFN_AUTH.xml \
	CHFN_RESTRICT.xml \
	CHSH_AUTH.xml \
	CONSOLE.xml \
	CONSOLE_GROUPS.xml \
	CREATE_HOME.xml \
	DEFAULT_HOME.xml \
	ENCRYPT_METHOD.xml \
	ENV_HZ.xml \
	ENVIRON_FILE.xml \
	ENV_PATH.xml \
	ENV_SUPATH.xml \
	ENV_TZ.xml \
	ERASECHAR.xml \
	FAIL_DELAY.xml \
	FAILLOG_ENAB.xml \
	FAKE_SHELL.xml \
	FTMP_FILE.xml \
	GID_MAX.xml \
	HMAC_CRYPTO_ALGO.xml \
	HOME_MODE.xml \
	HUSHLOGIN_FILE.xml \
	ISSUE_FILE.xml \
	KILLCHAR.xml \
	LASTLOG_ENAB.xml \
	LASTLOG_UID_MAX.xml \
	LOGIN_RETRIES.xml \
	LOGIN_STRING.xml \
	LOGIN_TIMEOUT.xml \
	LOG_OK_LOGINS.xml \
	LOG_UNKFAIL_ENAB.xml \
	MAIL_CHECK_ENAB.xml \
	MAIL_DIR.xml \
	MAX_MEMBERS_PER_GROUP.xml \
	MD5_CRYPT_ENAB.xml \
	MOTD_FILE.xml \
	NOLOGINS_FILE.xml \
	NONEXISTENT.xml \
	OBSCURE_CHECKS_ENAB.xml \
	PASS_ALWAYS_WARN.xml \
	PASS_CHANGE_TRIES.xml \
	PASS_MAX_DAYS.xml \
	PASS_MAX_LEN.xml \
	PASS_MIN_DAYS.xml \
	PASS_WARN_AGE.xml \
	PORTTIME_CHECKS_ENAB.xml \
	QUOTAS_ENAB.xml \
	SHA_CRYPT_MIN_ROUNDS.xml \
	SULOG_FILE.xml \
	SU_NAME.xml \
	SU_WHEEL_ONLY.xml \
	SYSLOG_SG_ENAB.xml \
	SYSLOG_SU_ENAB.xml \
	TCB_AUTH_GROUP.xml \
	TCB_SYMLINKS.xml \
	TTYGROUP.xml \
	TTYTYPE_FILE.xml \
	UID_MAX.xml \
	ULIMIT.xml \
	UMASK.xml \
	USERDEL_CMD.xml \
	USERGROUPS_ENAB.xml \
	USE_TCB.xml \
	SUB_GID_COUNT.xml \
	SUB_UID_COUNT.xml \
	SYS_GID_MAX.xml \
	SYS_UID_MAX.xml \
	YESCRYPT_COST_FACTOR.xml

EXTRA_DIST = \
	$(man_MANS) \
	$(man_XMANS) \
	config.xml \
	$(addprefix login.defs.d/,$(login_defs_v)) \
	man1/id.1 \
	id.1.xml \
	man8/sulogin.8 \
	sulogin.8.xml \
	generate_mans.mak \
	generate_translations.mak \
	its.rules \
	shadow-man.xsl

if USE_PAM
EXTRA_DIST += $(man_nopam)
endif

if !ENABLE_SUBIDS
EXTRA_DIST += $(man_subids)
endif

generate_mans.deps: $(man_XMANS)
	echo "# This file is generated" > $@
	awk 'BEGIN{FS="\"";} /^<!ENTITY .* * SYSTEM ".*">$$/{ f=FILENAME; sub(/.xml/,"",f); print "man" substr(f, length (f)) "/" f ": " $$2 }' $< >> $@

if ENABLE_REGENERATE_MAN

@ENABLE_REGENERATE_MAN_TRUE@include generate_mans.deps

include generate_mans.mak

CLEANFILES = $(man_MANS) man1/id.1 man8/sulogin.8

else
$(man_MANS):
	@echo ""
	@echo "Error: you need to run configure with '--enable-man'"
	@echo "       in order to regenerate man pages."
	@echo ""
endif
