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
#include "groupio.h"
#include "sgroupio.h"
#include "prototypes.h"
#include "shadowlog.h"

/*
 * cleanup_report_add_group - Report failure to add a group to the system
 *
 * It should be registered when it is decided to add a group to the system.
 */
void cleanup_report_add_group (void *group_name)
{
	const char *name = group_name;

	SYSLOG ((LOG_ERR, "failed to add group %s", name));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_GROUP, log_get_progname(),
	              "",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

/*
 * cleanup_report_del_group - Report failure to remove a group from the system
 *
 * It should be registered when it is decided to remove a group from the system.
 */
void cleanup_report_del_group (void *group_name)
{
	const char *name = group_name;

	SYSLOG ((LOG_ERR, "failed to remove group %s", name));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_DEL_GROUP, log_get_progname(),
	              "",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

void cleanup_report_mod_group (void *cleanup_info)
{
	const struct cleanup_info_mod *info;
	info = (const struct cleanup_info_mod *)cleanup_info;

	SYSLOG ((LOG_ERR,
	         "failed to change %s (%s)",
	         gr_dbname (),
	         info->action));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_USER_ACCT, log_get_progname(),
	              info->audit_msg,
	              info->name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

#ifdef SHADOWGRP
void cleanup_report_mod_gshadow (void *cleanup_info)
{
	const struct cleanup_info_mod *info;
	info = (const struct cleanup_info_mod *)cleanup_info;

	SYSLOG ((LOG_ERR,
	         "failed to change %s (%s)",
	         sgr_dbname (),
	         info->action));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_USER_ACCT, log_get_progname(),
	              info->audit_msg,
	              info->name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}
#endif

/*
 * cleanup_report_add_group_group - Report failure to add a group to group
 *
 * It should be registered when it is decided to add a group to the
 * group database.
 */
void cleanup_report_add_group_group (void *group_name)
{
	const char *name = group_name;

	SYSLOG ((LOG_ERR, "failed to add group %s to %s", name, gr_dbname ()));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_GROUP, log_get_progname(),
	              "adding group to /etc/group",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

#ifdef SHADOWGRP
/*
 * cleanup_report_add_group_gshadow - Report failure to add a group to gshadow
 *
 * It should be registered when it is decided to add a group to the
 * gshadow database.
 */
void cleanup_report_add_group_gshadow (void *group_name)
{
	const char *name = group_name;

	SYSLOG ((LOG_ERR, "failed to add group %s to %s", name, sgr_dbname ()));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_GROUP, log_get_progname(),
	              "adding group to /etc/gshadow",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}
#endif

/*
 * cleanup_report_del_group_group - Report failure to remove a group from the
 *                                  regular group database
 *
 * It should be registered when it is decided to remove a group from the
 * regular group database.
 */
void cleanup_report_del_group_group (void *group_name)
{
	const char *name = group_name;

	SYSLOG ((LOG_ERR,
	         "failed to remove group %s from %s",
	         name, gr_dbname ()));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_GROUP, log_get_progname(),
	              "removing group from /etc/group",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}

#ifdef SHADOWGRP
/*
 * cleanup_report_del_group_gshadow - Report failure to remove a group from
 *                                    gshadow
 *
 * It should be registered when it is decided to remove a group from the
 * gshadow database.
 */
void cleanup_report_del_group_gshadow (void *group_name)
{
	const char *name = group_name;

	SYSLOG ((LOG_ERR,
	         "failed to remove group %s from %s",
	         name, sgr_dbname ()));
#ifdef WITH_AUDIT
	audit_logger (AUDIT_ADD_GROUP, log_get_progname(),
	              "removing group from /etc/gshadow",
	              name, AUDIT_NO_ID,
	              SHADOW_AUDIT_FAILURE);
#endif
}
#endif

/*
 * cleanup_unlock_group - Unlock the group file
 *
 * It should be registered after the group file is successfully locked.
 */
void cleanup_unlock_group (MAYBE_UNUSED void *arg)
{
	if (gr_unlock () == 0) {
		fprintf (log_get_logfd(),
		         _("%s: failed to unlock %s\n"),
		         log_get_progname(), gr_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", gr_dbname ()));
#ifdef WITH_AUDIT
		audit_logger_message ("unlocking group file",
		                      SHADOW_AUDIT_FAILURE);
#endif
	}
}

#ifdef SHADOWGRP
/*
 * cleanup_unlock_gshadow - Unlock the gshadow file
 *
 * It should be registered after the gshadow file is successfully locked.
 */
void cleanup_unlock_gshadow (MAYBE_UNUSED void *arg)
{
	if (sgr_unlock () == 0) {
		fprintf (log_get_logfd(),
		         _("%s: failed to unlock %s\n"),
		         log_get_progname(), sgr_dbname ());
		SYSLOG ((LOG_ERR, "failed to unlock %s", sgr_dbname ()));
#ifdef WITH_AUDIT
		audit_logger_message ("unlocking gshadow file",
		                      SHADOW_AUDIT_FAILURE);
#endif
	}
}
#endif

