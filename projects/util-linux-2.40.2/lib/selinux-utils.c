/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * Written by Karel Zak <kzak@redhat.com> [January 2021]
 */
#include <selinux/context.h>
#include <selinux/selinux.h>
#include <selinux/label.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "selinux-utils.h"

/* set the SELinux security context used for _creating_ a new file system object
 *
 * returns 0 on success,
 *     or <0 on error
 */
int ul_setfscreatecon_from_file(char *orig_file)
{
	if (is_selinux_enabled() > 0) {
		char *scontext = NULL;

		if (getfilecon(orig_file, &scontext) < 0)
			return -1;
		if (setfscreatecon(scontext) < 0) {
			freecon(scontext);
			return -1;
		}
		freecon(scontext);
	}
	return 0;
}

/* returns 1 if user has access to @class and @perm ("passwd", "chfn")
 *	or 0 on error,
 *	or 0 if has no access -- in this case sets @user_cxt to user-context
 */
int ul_selinux_has_access(const char *classstr, const char *perm, char **user_cxt)
{
	char *user;
	int rc;

	if (user_cxt)
		*user_cxt = NULL;

	if (getprevcon(&user) != 0)
		return 0;

	rc = selinux_check_access(user, user, classstr, perm, NULL);
	if (rc != 0 && user_cxt)
		*user_cxt = user;
	else
		freecon(user);

	return rc == 0 ? 1 : 0;
}

/* Gets the default context for @path and @st_mode.
 *
 * returns 0 on success,
 *     or <0 on error
 */
int ul_selinux_get_default_context(const char *path, int st_mode, char **cxt)
{
	struct selabel_handle *hnd;
	struct selinux_opt options[SELABEL_NOPT] = {};
	int rc = 0;

	*cxt = NULL;

	hnd = selabel_open(SELABEL_CTX_FILE, options, SELABEL_NOPT);
	if (!hnd)
		return -errno;

	if (selabel_lookup(hnd, cxt, path, st_mode) != 0)
		rc = -errno
			;
	selabel_close(hnd);

	return rc;
}
