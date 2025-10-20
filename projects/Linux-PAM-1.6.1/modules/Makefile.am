#
# Copyright (c) 2005, 2006, 2008 Thorsten Kukuk <kukuk@thkukuk.de>
#

if COND_BUILD_PAM_KEYINIT
 MAYBE_PAM_KEYINIT = pam_keyinit
endif

if COND_BUILD_PAM_LASTLOG
 MAYBE_PAM_LASTLOG = pam_lastlog
endif

if COND_BUILD_PAM_NAMESPACE
 MAYBE_PAM_NAMESPACE = pam_namespace
endif

if COND_BUILD_PAM_RHOSTS
 MAYBE_PAM_RHOSTS = pam_rhosts
endif

if COND_BUILD_PAM_SELINUX
 MAYBE_PAM_SELINUX = pam_selinux
endif

if COND_BUILD_PAM_SEPERMIT
 MAYBE_PAM_SEPERMIT = pam_sepermit
endif

if COND_BUILD_PAM_SETQUOTA
 MAYBE_PAM_SETQUOTA = pam_setquota
endif

if COND_BUILD_PAM_TTY_AUDIT
 MAYBE_PAM_TTY_AUDIT = pam_tty_audit
endif

if COND_BUILD_PAM_UNIX
 MAYBE_PAM_UNIX = pam_unix
endif

if COND_BUILD_PAM_USERDB
 MAYBE_PAM_USERDB = pam_userdb
endif

SUBDIRS := \
	pam_access \
	pam_canonicalize_user \
	pam_debug \
	pam_deny \
	pam_echo \
	pam_env \
	pam_exec \
	pam_faildelay \
	pam_faillock \
	pam_filter \
	pam_ftp \
	pam_group \
	pam_issue \
	$(MAYBE_PAM_KEYINIT) \
	$(MAYBE_PAM_LASTLOG) \
	pam_limits \
	pam_listfile \
	pam_localuser \
	pam_loginuid \
	pam_mail \
	pam_mkhomedir \
	pam_motd \
	$(MAYBE_PAM_NAMESPACE) \
	pam_nologin \
	pam_permit \
	pam_pwhistory \
	$(MAYBE_PAM_RHOSTS) \
	pam_rootok \
	pam_securetty \
	$(MAYBE_PAM_SELINUX) \
	$(MAYBE_PAM_SEPERMIT) \
	$(MAYBE_PAM_SETQUOTA) \
	pam_shells \
	pam_stress \
	pam_succeed_if \
	pam_time \
	pam_timestamp \
	$(MAYBE_PAM_TTY_AUDIT) \
	pam_umask \
	$(MAYBE_PAM_UNIX) \
	$(MAYBE_PAM_USERDB) \
	pam_usertype \
	pam_warn \
	pam_wheel \
	pam_xauth \
	#

CLEANFILES = *~

EXTRA_DIST = modules.map
