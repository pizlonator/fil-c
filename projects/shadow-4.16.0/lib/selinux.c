/*
 * SPDX-FileCopyrightText: 2011       , Peter Vrabec <pvrabec@redhat.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ifdef WITH_SELINUX

#include <stdio.h>
#include "defines.h"

#include <selinux/selinux.h>
#include <selinux/label.h>
#include "prototypes.h"

#include "shadowlog_internal.h"

static bool selinux_checked = false;
static bool selinux_enabled;
static /*@null@*/struct selabel_handle *selabel_hnd = NULL;

static void cleanup(void)
{
	if (selabel_hnd) {
		selabel_close(selabel_hnd);
		selabel_hnd = NULL;
	}
}

void reset_selinux_handle (void)
{
    cleanup();
}

/*
 * set_selinux_file_context - Set the security context before any file or
 *                            directory creation.
 *
 *	set_selinux_file_context () should be called before any creation
 *	of file, symlink, directory, ...
 *
 *	Callers may have to Reset SELinux to create files with default
 *	contexts with reset_selinux_file_context
 */
int set_selinux_file_context (const char *dst_name, mode_t mode)
{
	if (!selinux_checked) {
		selinux_enabled = is_selinux_enabled () > 0;
		selinux_checked = true;
	}

	if (selinux_enabled) {
		/* Get the default security context for this file */

		/*@null@*/char *fcontext_raw = NULL;
		int r;

		if (selabel_hnd == NULL) {
			selabel_hnd = selabel_open(SELABEL_CTX_FILE, NULL, 0);
			if (selabel_hnd == NULL) {
				return security_getenforce () != 0;
			}
			(void) atexit(cleanup);
		}

		r = selabel_lookup_raw(selabel_hnd, &fcontext_raw, dst_name, mode);
		if (r < 0) {
			/* No context specified for the searched path */
			if (errno == ENOENT) {
				return 0;
			}

			return security_getenforce () != 0;
		}

		/* Set the security context for the next created file */
		r = setfscreatecon_raw (fcontext_raw);
		freecon (fcontext_raw);
		if (r < 0) {
			return security_getenforce () != 0;
		}
	}
	return 0;
}

/*
 * reset_selinux_file_context - Reset the security context to the default
 *                              policy behavior
 *
 *	reset_selinux_file_context () should be called after the context
 *	was changed with set_selinux_file_context ()
 */
int reset_selinux_file_context (void)
{
	if (!selinux_checked) {
		selinux_enabled = is_selinux_enabled () > 0;
		selinux_checked = true;
	}
	if (selinux_enabled) {
		if (setfscreatecon_raw (NULL) != 0) {
			return security_getenforce () != 0;
		}
	}
	return 0;
}

/*
 *  Log callback for libselinux internal error reporting.
 */
format_attr(printf, 2, 3)
static int selinux_log_cb (int type, const char *fmt, ...) {
	va_list ap;
	char *buf;
	int r;
#ifdef WITH_AUDIT
	static int selinux_audit_fd = -2;
#endif

	va_start (ap, fmt);
	r = vasprintf (&buf, fmt, ap);
	va_end (ap);

	if (r < 0) {
		return 0;
	}

#ifdef WITH_AUDIT
	if (-2 == selinux_audit_fd) {
		selinux_audit_fd = audit_open ();

		if (-1 == selinux_audit_fd) {
			/* You get these only when the kernel doesn't have
			 * audit compiled in. */
			if (   (errno != EINVAL)
			    && (errno != EPROTONOSUPPORT)
			    && (errno != EAFNOSUPPORT)) {

			    (void) fputs (_("Cannot open audit interface.\n"),
			              shadow_logfd);
			    SYSLOG ((LOG_WARN, "Cannot open audit interface."));
			}
		}
	}

	if (-1 != selinux_audit_fd) {
		if (SELINUX_AVC == type) {
			if (audit_log_user_avc_message (selinux_audit_fd,
			    AUDIT_USER_AVC, buf, NULL, NULL,
			    NULL, 0) > 0) {
				goto skip_syslog;
			}
		} else if (SELINUX_ERROR == type) {
			if (audit_log_user_avc_message (selinux_audit_fd,
			    AUDIT_USER_SELINUX_ERR, buf, NULL, NULL,
			    NULL, 0) > 0) {
				goto skip_syslog;
			}
		}
	}
#endif

	SYSLOG ((LOG_WARN, "libselinux: %s", buf));

skip_syslog:
	free (buf);

	return 0;
}

/*
 * check_selinux_permit - Check whether SELinux grants the given
 *                        operation
 *
 *   Parameter is the SELinux permission name, e.g. rootok
 *
 *   Returns 0 when permission is granted
 *                  or something failed but running in
 *                  permissive mode
 */
int check_selinux_permit (const char *perm_name)
{
	char *user_context_raw;
	int r;

	if (0 == is_selinux_enabled ()) {
		return 0;
	}

	selinux_set_callback (SELINUX_CB_LOG, (union selinux_callback) { .func_log = selinux_log_cb });

	if (getprevcon_raw (&user_context_raw) != 0) {
		fprintf (shadow_logfd,
		    _("%s: can not get previous SELinux process context: %s\n"),
		    shadow_progname, strerror (errno));
		SYSLOG ((LOG_WARN,
		    "can not get previous SELinux process context: %s",
		    strerror (errno)));
		return (security_getenforce () != 0);
	}

	r = selinux_check_access (user_context_raw, user_context_raw, "passwd", perm_name, NULL);
	freecon (user_context_raw);
	return r;
}

#else				/* !WITH_SELINUX */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* !WITH_SELINUX */
