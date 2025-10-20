#
# Copyright (c) 2006, 2007 Thorsten Kukuk <kukuk@thkukuk.de>
#

CLEANFILES = *~
MAINTAINERCLEANFILES = $(MANS)

EXTRA_DIST = $(MANS) $(XMLS)

man_MANS = pam.3 PAM.8 pam.8 pam.conf.5 pam.d.5 \
	pam_acct_mgmt.3 pam_authenticate.3 \
	pam_chauthtok.3 pam_close_session.3 pam_conv.3 \
	pam_end.3 pam_error.3 \
	pam_fail_delay.3 pam_xauth_data.3 \
	pam_get_authtok.3 pam_get_authtok_noverify.3 pam_get_authtok_verify.3 \
	pam_get_data.3 pam_get_item.3 pam_get_user.3 \
	pam_getenv.3 pam_getenvlist.3 \
	pam_info.3 \
	pam_open_session.3 \
	pam_prompt.3 pam_putenv.3 \
	pam_set_data.3 pam_set_item.3 pam_syslog.3 \
	pam_setcred.3 pam_sm_acct_mgmt.3 pam_sm_authenticate.3 \
	pam_sm_close_session.3 pam_sm_open_session.3 pam_sm_setcred.3 \
	pam_sm_chauthtok.3 pam_start.3 pam_strerror.3 \
	pam_verror.3 pam_vinfo.3 pam_vprompt.3 pam_vsyslog.3 \
	misc_conv.3 pam_misc_paste_env.3 pam_misc_drop_env.3 \
	pam_misc_setenv.3
XMLS = pam.3.xml pam.8.xml pam.conf.5.xml \
	pam_acct_mgmt.3.xml pam_authenticate.3.xml \
	pam_chauthtok.3.xml pam_close_session.3.xml pam_conv.3.xml \
	pam_end.3.xml pam_error.3.xml pam_fail_delay.3.xml  \
	pam_get_authtok.3.xml pam_get_data.3.xml pam_get_item.3.xml \
	pam_get_user.3.xml pam_getenv.3.xml pam_getenvlist.3.xml \
        pam_info.3.xml pam_misc_drop_env.3.xml pam_misc_paste_env.3.xml \
	pam_misc_setenv.3.xml pam_open_session.3.xml \
	pam_prompt.3.xml pam_putenv.3.xml \
	pam_set_data.3.xml pam_set_item.3.xml pam_setcred.3.xml \
	pam_sm_acct_mgmt.3.xml pam_sm_authenticate.3.xml \
	pam_sm_chauthtok.3.xml pam_sm_close_session.3.xml \
	pam_sm_open_session.3.xml pam_sm_setcred.3.xml \
	pam_start.3.xml pam_strerror.3.xml \
	pam_syslog.3.xml pam_xauth_data.3.xml \
	pam_item_types_std.inc.xml pam_item_types_ext.inc.xml \
	pam.conf-desc.xml pam.conf-dir.xml pam.conf-syntax.xml \
	misc_conv.3.xml


if ENABLE_REGENERATE_MAN
PAM.8: pam.8
pam_get_authtok_noverify.3: pam_get_authtok.3
pam_get_authtok_verify.3: pam_get_authtok.3
pam_verror.3: pam_error.3
pam_vinfo.3: pam_info.3
pam_vprompt.3: pam_prompt.3
pam_vsyslog.3: pam_syslog.3
pam.d.5: pam.conf.5

pam_get_item.3: pam_item_types_std.inc.xml pam_item_types_ext.inc.xml
pam_set_data.3: pam_item_types_std.inc.xml pam_item_types_ext.inc.xml
pam.conf.5: pam.conf-desc.xml pam.conf-dir.xml pam.conf-syntax.xml
-include $(top_builddir)/Make.xml.rules
endif
