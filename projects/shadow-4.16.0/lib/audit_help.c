/*
 * SPDX-FileCopyrightText: 2005       , Red Hat, Inc.
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2008       , Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 *  Audit helper functions used throughout shadow
 *
 */

#include <config.h>

#ifdef WITH_AUDIT

#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <libaudit.h>
#include <errno.h>
#include <stdio.h>

#include "attr.h"
#include "prototypes.h"
#include "shadowlog.h"
int audit_fd;

void audit_help_open (void)
{
	audit_fd = audit_open ();
	if (audit_fd < 0) {
		/* You get these only when the kernel doesn't have
		 * audit compiled in. */
		if (   (errno == EINVAL)
		    || (errno == EPROTONOSUPPORT)
		    || (errno == EAFNOSUPPORT)) {
			return;
		}
		(void) fputs (_("Cannot open audit interface - aborting.\n"),
		              log_get_logfd());
		exit (EXIT_FAILURE);
	}
}

/*
 * This function will log a message to the audit system using a predefined
 * message format. Parameter usage is as follows:
 *
 * type - type of message: AUDIT_USER_CHAUTHTOK for changing any account
 *	  attributes.
 * pgname - program's name
 * op  -  operation. "adding user", "changing finger info", "deleting group"
 * name - user's account or group name. If not available use NULL.
 * id  -  uid or gid that the operation is being performed on. This is used
 *	  only when user is NULL.
 */
void audit_logger (int type, MAYBE_UNUSED const char *pgname, const char *op,
                   const char *name, unsigned int id,
                   shadow_audit_result result)
{
	if (audit_fd < 0) {
		return;
	} else {
		audit_log_acct_message (audit_fd, type, NULL, op, name, id,
		                        NULL, NULL, NULL, result);
	}
}

void audit_logger_message (const char *message, shadow_audit_result result)
{
	if (audit_fd < 0) {
		return;
	} else {
		audit_log_user_message (audit_fd,
		                        AUDIT_USYS_CONFIG,
		                        message,
		                        NULL, /* hostname */
		                        NULL, /* addr */
		                        NULL, /* tty */
		                        result);
	}
}

#else				/* WITH_AUDIT */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* WITH_AUDIT */

