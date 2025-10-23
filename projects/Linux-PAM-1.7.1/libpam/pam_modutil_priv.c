/*
 * $Id$
 *
 * This file provides two functions:
 * pam_modutil_drop_priv:
 *   temporarily lower process fs privileges by switching to another uid/gid,
 * pam_modutil_regain_priv:
 *   regain process fs privileges lowered by pam_modutil_drop_priv().
 */

#include "pam_modutil_private.h"
#include <security/pam_ext.h>
#include <unistd.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <sys/fsuid.h>

/*
 * Two setfsuid() calls in a row are necessary to check
 * whether setfsuid() succeeded or not.
 */
static int change_uid(uid_t uid, uid_t *save)
{
	uid_t tmp = setfsuid(uid);
	if (save)
		*save = tmp;
	return (uid_t) setfsuid(uid) == uid ? 0 : -1;
}
static int change_gid(gid_t gid, gid_t *save)
{
	gid_t tmp = setfsgid(gid);
	if (save)
		*save = tmp;
	return (gid_t) setfsgid(gid) == gid ? 0 : -1;
}

static int cleanup(struct pam_modutil_privs *p)
{
	if (p->allocated) {
		p->allocated = 0;
		free(p->grplist);
	}
	p->grplist = NULL;
	p->number_of_groups = 0;
	return -1;
}

#define PRIV_MAGIC			0x1004000a
#define PRIV_MAGIC_DONOTHING		0xdead000a

int pam_modutil_drop_priv(pam_handle_t *pamh,
			  struct pam_modutil_privs *p,
			  const struct passwd *pw)
{
	int res;

	if (p->is_dropped) {
		pam_syslog(pamh, LOG_CRIT,
			   "pam_modutil_drop_priv: called with dropped privileges");
		return -1;
	}

	/*
	 * If not root, we can do nothing.
	 * If switching to root, we have nothing to do.
	 * That is, in both cases, we do not care.
	 */
	if (geteuid() != 0 || pw->pw_uid == 0) {
		p->is_dropped = PRIV_MAGIC_DONOTHING;
		return 0;
	}

	if (!p->grplist || p->number_of_groups <= 0) {
		pam_syslog(pamh, LOG_CRIT,
			   "pam_modutil_drop_priv: called without room for supplementary groups");
		return -1;
	}
	res = getgroups(0, NULL);
	if (res < 0) {
		pam_syslog(pamh, LOG_ERR,
			   "pam_modutil_drop_priv: getgroups failed: %m");
		return -1;
	}

	p->allocated = 0;
	if (res > p->number_of_groups) {
		p->grplist = calloc(res, sizeof(gid_t));
		if (!p->grplist) {
			pam_syslog(pamh, LOG_CRIT, "out of memory");
			return cleanup(p);
		}
		p->allocated = 1;
		p->number_of_groups = res;
	}

	res = getgroups(p->number_of_groups, p->grplist);
	if (res < 0) {
		pam_syslog(pamh, LOG_ERR,
			   "pam_modutil_drop_priv: getgroups failed: %m");
		return cleanup(p);
	}

	p->number_of_groups = res;

	/*
	 * We should care to leave process credentials in consistent state.
	 * That is, e.g. if change_gid() succeeded but change_uid() failed,
	 * we should try to restore old gid.
	 *
	 * We try to add the supplementary groups on a best-effort
	 * basis.  If it fails, it's not fatal: we fall back to using an
	 * empty list.
	 */
	if (initgroups(pw->pw_name, pw->pw_gid)) {
		pam_syslog(pamh, LOG_WARNING,
			   "pam_modutil_drop_priv: initgroups failed: %m");

		if (setgroups(0, NULL)) {
			pam_syslog(pamh, LOG_ERR,
				   "pam_modutil_drop_priv: setgroups failed: %m");
			return cleanup(p);
		}
	}
	if (change_gid(pw->pw_gid, &p->old_gid)) {
		pam_syslog(pamh, LOG_ERR,
			   "pam_modutil_drop_priv: change_gid failed: %m");
		(void) setgroups(p->number_of_groups, p->grplist);
		return cleanup(p);
	}
	if (change_uid(pw->pw_uid, &p->old_uid)) {
		pam_syslog(pamh, LOG_ERR,
			   "pam_modutil_drop_priv: change_uid failed: %m");
		(void) change_gid(p->old_gid, NULL);
		(void) setgroups(p->number_of_groups, p->grplist);
		return cleanup(p);
	}

	p->is_dropped = PRIV_MAGIC;
	return 0;
}

int pam_modutil_regain_priv(pam_handle_t *pamh,
			  struct pam_modutil_privs *p)
{
	switch (p->is_dropped) {
		case PRIV_MAGIC_DONOTHING:
			p->is_dropped = 0;
			return 0;

		case PRIV_MAGIC:
			break;

		default:
			pam_syslog(pamh, LOG_CRIT,
				   "pam_modutil_regain_priv: called with invalid state");
			return -1;
		}

	if (change_uid(p->old_uid, NULL)) {
		pam_syslog(pamh, LOG_ERR,
			   "pam_modutil_regain_priv: change_uid failed: %m");
		return cleanup(p);
	}
	if (change_gid(p->old_gid, NULL)) {
		pam_syslog(pamh, LOG_ERR,
			   "pam_modutil_regain_priv: change_gid failed: %m");
		return cleanup(p);
	}
	if (setgroups(p->number_of_groups, p->grplist)) {
		pam_syslog(pamh, LOG_ERR,
			   "pam_modutil_regain_priv: setgroups failed: %m");
		return cleanup(p);
	}

	p->is_dropped = 0;
	cleanup(p);
	return 0;
}
