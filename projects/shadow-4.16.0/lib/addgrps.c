/*
 * SPDX-FileCopyrightText: 1989 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1998, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2001 - 2006, Tomasz Kłoczko
 * SPDX-FileCopyrightText: 2007 - 2009, Nicolas François
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#if defined (HAVE_SETGROUPS) && ! defined (USE_PAM)

#include "prototypes.h"
#include "defines.h"

#include <stdio.h>
#include <grp.h>
#include <errno.h>

#include "alloc.h"
#include "shadowlog.h"

#ident "$Id$"

#define SEP ",:"
/*
 * Add groups with names from LIST (separated by commas or colons)
 * to the supplementary group set.  Silently ignore groups which are
 * already there.  Warning: uses strtok().
 */
int add_groups (const char *list)
{
	GETGROUPS_T *grouplist;
	size_t i;
	int ngroups;
	bool added;
	char *token;
	char buf[1024];
	int ret;
	FILE *shadow_logfd = log_get_logfd();

	if (strlen (list) >= sizeof (buf)) {
		errno = EINVAL;
		return -1;
	}
	strcpy (buf, list);

	i = 16;
	for (;;) {
		grouplist = MALLOC(i, GETGROUPS_T);
		if (NULL == grouplist) {
			return -1;
		}
		ngroups = getgroups (i, grouplist);
		if (   (   (-1 == ngroups)
		        && (EINVAL != errno))
		    || (i > (size_t)ngroups)) {
			/* Unexpected failure of getgroups or successful
			 * reception of the groups */
			break;
		}
		/* not enough room, so try allocating a larger buffer */
		free (grouplist);
		i *= 2;
	}
	if (ngroups < 0) {
		free (grouplist);
		return -1;
	}

	added = false;
	for (token = strtok (buf, SEP); NULL != token; token = strtok (NULL, SEP)) {
		struct group *grp;

		grp = getgrnam (token); /* local, no need for xgetgrnam */
		if (NULL == grp) {
			fprintf (shadow_logfd, _("Warning: unknown group %s\n"),
				 token);
			continue;
		}

		for (i = 0; i < (size_t)ngroups && grouplist[i] != grp->gr_gid; i++);

		if (i < (size_t)ngroups) {
			continue;
		}

		if (ngroups >= sysconf (_SC_NGROUPS_MAX)) {
			fputs (_("Warning: too many groups\n"), shadow_logfd);
			break;
		}
		grouplist = REALLOCF(grouplist, (size_t) ngroups + 1, GETGROUPS_T);
		if (grouplist == NULL) {
			return -1;
		}
		grouplist[ngroups] = grp->gr_gid;
		ngroups++;
		added = true;
	}

	if (added) {
		ret = setgroups (ngroups, grouplist);
		free (grouplist);
		return ret;
	}

	free (grouplist);
	return 0;
}
#else				/* HAVE_SETGROUPS && !USE_PAM */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* HAVE_SETGROUPS && !USE_PAM */

