/*
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include <assert.h>
#include <stdio.h>

#include "attr.h"
#include "defines.h"
#include "pwio.h"
#include "shadowio.h"
#include "prototypes.h"
#include "shadowlog.h"

/*
 * cleanup_report_add_user - Report failure to add a user to the system
 *
 * It should be registered when it is decided to add a user to the system.
 */
void cleanup_report_add_user (void *user_name)
{
	const char *name = user_name;

	SYSLOG ((LOG_ERR, "failed to add user %s", name));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_USER, log_get_progname(),
	              "",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

void cleanup_report_mod_passwd (void *cleanup_info)
{
	const struct cleanup_info_mod *info;
	info = (const struct cleanup_info_mod *)cleanup_info;

	SYSLOG ((LOG_ERR,
	         "failed to change %s (%s)",
	         pw_dbname (),
	         info->action));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_USER_ACCT, log_get_progname(),
	              info->audit_msg,
	              info->name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

/*
 * cleanup_report_add_user_passwd - Report failure to add a user to
 * /etc/passwd
 *
 * It should be registered when it is decided to add a user to the
 * /etc/passwd database.
 */
void cleanup_report_add_user_passwd (void *user_name)
{
	const char *name = user_name;

	SYSLOG ((LOG_ERR, "failed to add user %s to %s", name, pw_dbname ()));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_USER, log_get_progname(),
	              "adding user to /etc/passwd",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

/*
 * cleanup_report_add_user_shadow - Report failure to add a user to
 * /etc/shadow
 *
 * It should be registered when it is decided to add a user to the
 * /etc/shadow database.
 */
void cleanup_report_add_user_shadow (void *user_name)
{
	const char *name = user_name;

	SYSLOG ((LOG_ERR, "failed to add user %s to %s", name, spw_dbname ()));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_USER, log_get_progname(),
	              "adding user to /etc/shadow",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

/*
 * cleanup_unlock_passwd - Unlock the /etc/passwd database
 *
 * It should be registered after the passwd database is successfully locked.
 */
void cleanup_unlock_passwd (MAYBE_UNUSED void *arg)
{
	if (pw_unlock () == 0) {
		fprintf (log_get_logfd(),
		         _("%s: failed to unlock %s\n"),
		         log_get_progname(), pw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", pw_dbname ()));
#ifdef WITH_AUDIT
		audit_logger_message ("unlocking passwd file",
		                      SHADOW_AUDIT_FAILURE);
#endif
	}
}

/*
 * cleanup_unlock_shadow - Unlock the /etc/shadow database
 *
 * It should be registered after the shadow database is successfully locked.
 */
void cleanup_unlock_shadow (MAYBE_UNUSED void *arg)
{
	if (spw_unlock () == 0) {
		fprintf (log_get_logfd(),
		         _("%s: failed to unlock %s\n"),
		         log_get_progname(), spw_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", spw_dbname ()));
#ifdef WITH_AUDIT
		audit_logger_message ("unlocking shadow file",
		                      SHADOW_AUDIT_FAILURE);
#endif
	}
}

