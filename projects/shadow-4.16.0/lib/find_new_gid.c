/*
 * SPDX-FileCopyrightText: 1991 - 1994, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 2008 - 2011, Nicolas François
 * SPDX-FileCopyrightText: 2014, Red Hat, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#include <assert.h>
#include <stdio.h>
#include <errno.h>

#include "alloc.h"
#include "prototypes.h"
#include "groupio.h"
#include "getdef.h"
#include "shadowlog.h"

/*
 * get_ranges - Get the minimum and maximum ID ranges for the search
 *
 * This function will return the minimum and maximum ranges for IDs
 *
 * 0: The function completed successfully
 * EINVAL: The provided ranges are impossible (such as maximum < minimum)
 *
 * preferred_min: The special-case minimum value for a specifically-
 * requested ID, which may be lower than the standard min_id
 */
static int get_ranges (bool sys_group, gid_t *min_id, gid_t *max_id,
		       gid_t *preferred_min)
{
	gid_t gid_def_max = 0;

	if (sys_group) {
		/* System groups */

		/* A requested ID is allowed to be below the autoselect range */
		*preferred_min = (gid_t) 1;

		/* Get the minimum ID range from login.defs or default to 101 */
		*min_id = getdef_ulong ("SYS_GID_MIN", 101UL);

		/*
		 * If SYS_GID_MAX is unspecified, we should assume it to be one
		 * less than the GID_MIN (which is reserved for non-system accounts)
		 */
		gid_def_max = getdef_ulong ("GID_MIN", 1000UL) - 1;
		*max_id = getdef_ulong ("SYS_GID_MAX", gid_def_max);

		/* Check that the ranges make sense */
		if (*max_id < *min_id) {
			(void) fprintf (log_get_logfd(),
                            _("%s: Invalid configuration: SYS_GID_MIN (%lu), "
                              "GID_MIN (%lu), SYS_GID_MAX (%lu)\n"),
                            log_get_progname(), (unsigned long) *min_id,
                            getdef_ulong ("GID_MIN", 1000UL),
                            (unsigned long) *max_id);
			return EINVAL;
		}
		/*
		 * Zero is reserved for root and the allocation algorithm does not
		 * work right with it.
		 */
		if (*min_id == 0) {
			*min_id = (gid_t) 1;
		}
	} else {
		/* Non-system groups */

		/* Get the values from login.defs or use reasonable defaults */
		*min_id = getdef_ulong ("GID_MIN", 1000UL);
		*max_id = getdef_ulong ("GID_MAX", 60000UL);

		/*
		 * The preferred minimum should match the standard ID minimum
		 * for non-system groups.
		 */
		*preferred_min = *min_id;

		/* Check that the ranges make sense */
		if (*max_id < *min_id) {
			(void) fprintf (log_get_logfd(),
					_("%s: Invalid configuration: GID_MIN (%lu), "
					  "GID_MAX (%lu)\n"),
					log_get_progname(), (unsigned long) *min_id,
					(unsigned long) *max_id);
			return EINVAL;
		}
	}

	return 0;
}

/*
 * check_gid - See if the requested GID is available
 *
 * On success, return 0
 * If the ID is in use, return EEXIST
 * If the ID might clash with -1, return EINVAL
 * If the ID is outside the range, return ERANGE
 * In other cases, return errno from getgrgid()
 */
static int check_gid (const gid_t gid,
		      const gid_t gid_min,
		      const gid_t gid_max,
		      const bool *used_gids)
{
	/* First test that the preferred ID is in the range */
	if (gid < gid_min || gid > gid_max) {
		return ERANGE;
	}

	/* Check for compatibility with 16b and 32b gid_t error codes */
	if (gid == UINT16_MAX || gid == UINT32_MAX) {
		return EINVAL;
	}

	/*
	 * Check whether we already detected this GID
	 * using the gr_next() loop
	 */
	if (used_gids != NULL && used_gids[gid]) {
		return EEXIST;
	}
	/* Check if the GID exists according to NSS */
	errno = 0;
	if (prefix_getgrgid (gid) != NULL) {
		return EEXIST;
	} else {
		/* getgrgid() was NULL
		 * we have to ignore errors as temporary
		 * failures of remote user identity services
		 * would completely block user/group creation
		 */
	}

	/* If we've made it here, the GID must be available */
	return 0;
}

/*
 * find_new_gid - Find a new unused GID.
 *
 * If successful, find_new_gid provides an unused group ID in the
 * [GID_MIN:GID_MAX] range.
 * This ID should be higher than all the used GID, but if not possible,
 * the lowest unused ID in the range will be returned.
 *
 * Return 0 on success, -1 if no unused GIDs are available.
 */
int find_new_gid (bool sys_group,
                 gid_t *gid,
                 /*@null@*/gid_t const *preferred_gid)
{
	bool *used_gids;
	const struct group *grp;
	gid_t gid_min, gid_max, preferred_min;
	gid_t id;
	gid_t lowest_found, highest_found;
	int result;
	int nospam = 0;

	assert(gid != NULL);

	/*
	 * First, figure out what ID range is appropriate for
	 * automatic assignment
	 */
	result = get_ranges (sys_group, &gid_min, &gid_max, &preferred_min);
	if (result == EINVAL) {
		return -1;
	}

	/* Check if the preferred GID is available */
	if (preferred_gid) {
		result = check_gid (*preferred_gid, preferred_min, gid_max, NULL);
		if (result == 0) {
			/*
			 * Make sure the GID isn't queued for use already
			 */
			if (gr_locate_gid (*preferred_gid) == NULL) {
				*gid = *preferred_gid;
				return 0;
			}
			/*
			 * gr_locate_gid() found the GID in an as-yet uncommitted
			 * entry. We'll proceed below and auto-set a GID.
			 */
		} else if (result == EEXIST || result == ERANGE || result == EINVAL) {
			/*
			 * Continue on below. At this time, we won't
			 * treat these three cases differently.
			 */
		} else {
			/*
			 * An unexpected error occurred. We should report
			 * this and fail the group creation.
			 * This differs from the automatic creation
			 * behavior below, since if a specific GID was
			 * requested and generated an error, the user is
			 * more likely to want to stop and address the
			 * issue.
			 */
			fprintf (log_get_logfd(),
				_("%s: Encountered error attempting to use "
				  "preferred GID: %s\n"),
				log_get_progname(), strerror (result));
			return -1;
		}
	}

	/*
	 * Search the entire group file,
	 * looking for the next unused value.
	 *
	 * We first check the local database with gr_rewind/gr_next to find
	 * all local values that are in use.
	 *
	 * We then compare the next free value to all databases (local and
	 * remote) and iterate until we find a free one. If there are free
	 * values beyond the lowest (system groups) or highest (non-system
	 * groups), we will prefer those and avoid potentially reclaiming a
	 * deleted group (which can be a security issue, since it may grant
	 * access to files belonging to that former group).
	 *
	 * If there are no GIDs available at the end of the search, we will
	 * have no choice but to iterate through the range looking for gaps.
	 *
	 */

	/* Create an array to hold all of the discovered GIDs */
	used_gids = CALLOC (gid_max + 1, bool);
	if (NULL == used_gids) {
		fprintf (log_get_logfd(),
			 _("%s: failed to allocate memory: %s\n"),
			 log_get_progname(), strerror (errno));
		return -1;
	}

	/* First look for the lowest and highest value in the local database */
	(void) gr_rewind ();
	highest_found = gid_min;
	lowest_found = gid_max;
	while ((grp = gr_next ()) != NULL) {
		/*
		 * Does this entry have a lower GID than the lowest we've found
		 * so far?
		 */
		if ((grp->gr_gid <= lowest_found) && (grp->gr_gid >= gid_min)) {
			lowest_found = grp->gr_gid - 1;
		}

		/*
		 * Does this entry have a higher GID than the highest we've found
		 * so far?
		 */
		if ((grp->gr_gid >= highest_found) && (grp->gr_gid <= gid_max)) {
			highest_found = grp->gr_gid + 1;
		}

		/* create index of used GIDs */
		if (grp->gr_gid >= gid_min
			&& grp->gr_gid <= gid_max) {

			used_gids[grp->gr_gid] = true;
		}
	}

	if (sys_group) {
		/*
		 * For system groups, we want to start from the
		 * top of the range and work downwards.
		 */

		/*
		 * At the conclusion of the gr_next() search, we will either
		 * have a presumed-free GID or we will be at GID_MIN - 1.
		 */
		if (lowest_found < gid_min) {
			/*
			 * In this case, a GID is in use at GID_MIN.
			 *
			 * We will reset the search to GID_MAX and proceed down
			 * through all the GIDs (skipping those we detected with
			 * used_gids) for a free one. It is a known issue that
			 * this may result in reusing a previously-deleted GID,
			 * so administrators should be instructed to use this
			 * auto-detection with care (and prefer to assign GIDs
			 * explicitly).
			 */
			lowest_found = gid_max;
		}

		/* Search through all of the IDs in the range */
		for (id = lowest_found; id >= gid_min; id--) {
			result = check_gid (id, gid_min, gid_max, used_gids);
			if (result == 0) {
				/* This GID is available. Return it. */
				*gid = id;
				free (used_gids);
				return 0;
			} else if (result == EEXIST || result == EINVAL) {
				/*
				 * This GID is in use or unusable, we'll
				 * continue to the next.
				 */
			} else {
				/*
				 * An unexpected error occurred.
				 *
				 * Only report it the first time to avoid spamming
				 * the logs
				 *
				 */
				if (!nospam) {
					fprintf (log_get_logfd(),
						_("%s: Can't get unique system GID (%s). "
						  "Suppressing additional messages.\n"),
						log_get_progname(), strerror (result));
					SYSLOG ((LOG_ERR,
						"Error checking available GIDs: %s",
						strerror (result)));
					nospam = 1;
				}
				/*
				 * We will continue anyway. Hopefully a later GID
				 * will work properly.
				 */
			}
		}

		/*
		 * If we get all the way through the loop, try again from GID_MAX,
		 * unless that was where we previously started. (NOTE: the worst-case
		 * scenario here is that we will run through (GID_MAX - GID_MIN - 1)
		 * cycles *again* if we fall into this case with lowest_found as
		 * GID_MAX - 1, all groups in the range in use and maintained by
		 * network services such as LDAP.)
		 */
		if (lowest_found != gid_max) {
			for (id = gid_max; id >= gid_min; id--) {
				result = check_gid (id, gid_min, gid_max, used_gids);
				if (result == 0) {
					/* This GID is available. Return it. */
					*gid = id;
					free (used_gids);
					return 0;
				} else if (result == EEXIST || result == EINVAL) {
					/*
					 * This GID is in use or unusable, we'll
					 * continue to the next.
					 */
				} else {
					/*
					 * An unexpected error occurred.
					 *
					 * Only report it the first time to avoid spamming
					 * the logs
					 *
					 */
					if (!nospam) {
						fprintf (log_get_logfd(),
							_("%s: Can't get unique system GID (%s). "
							  "Suppressing additional messages.\n"),
							log_get_progname(), strerror (result));
						SYSLOG ((LOG_ERR,
							"Error checking available GIDs: %s",
							strerror (result)));
						nospam = 1;
					}
					/*
					 * We will continue anyway. Hopefully a later GID
					 * will work properly.
					 */
				}
			}
		}
	} else { /* !sys_group */
		/*
		 * For non-system groups, we want to start from the
		 * bottom of the range and work upwards.
		 */

		/*
		 * At the conclusion of the gr_next() search, we will either
		 * have a presumed-free GID or we will be at GID_MAX + 1.
		 */
		if (highest_found > gid_max) {
			/*
			 * In this case, a GID is in use at GID_MAX.
			 *
			 * We will reset the search to GID_MIN and proceed up
			 * through all the GIDs (skipping those we detected with
			 * used_gids) for a free one. It is a known issue that
			 * this may result in reusing a previously-deleted GID,
			 * so administrators should be instructed to use this
			 * auto-detection with care (and prefer to assign GIDs
			 * explicitly).
			 */
			highest_found = gid_min;
		}

		/* Search through all of the IDs in the range */
		for (id = highest_found; id <= gid_max; id++) {
			result = check_gid (id, gid_min, gid_max, used_gids);
			if (result == 0) {
				/* This GID is available. Return it. */
				*gid = id;
				free (used_gids);
				return 0;
			} else if (result == EEXIST || result == EINVAL) {
				/*
				 * This GID is in use or unusable, we'll
				 * continue to the next.
				 */
			} else {
				/*
				 * An unexpected error occurred.
				 *
				 * Only report it the first time to avoid spamming
				 * the logs
				 *
				 */
				if (!nospam) {
					fprintf (log_get_logfd(),
						_("%s: Can't get unique GID (%s). "
						  "Suppressing additional messages.\n"),
						log_get_progname(), strerror (result));
					SYSLOG ((LOG_ERR,
						"Error checking available GIDs: %s",
						strerror (result)));
					nospam = 1;
				}
				/*
				 * We will continue anyway. Hopefully a later GID
				 * will work properly.
				 */
			}
		}

		/*
		 * If we get all the way through the loop, try again from GID_MIN,
		 * unless that was where we previously started. (NOTE: the worst-case
		 * scenario here is that we will run through (GID_MAX - GID_MIN - 1)
		 * cycles *again* if we fall into this case with highest_found as
		 * GID_MIN + 1, all groups in the range in use and maintained by
		 * network services such as LDAP.)
		 */
		if (highest_found != gid_min) {
			for (id = gid_min; id <= gid_max; id++) {
				result = check_gid (id, gid_min, gid_max, used_gids);
				if (result == 0) {
					/* This GID is available. Return it. */
					*gid = id;
					free (used_gids);
					return 0;
				} else if (result == EEXIST || result == EINVAL) {
					/*
					 * This GID is in use or unusable, we'll
					 * continue to the next.
					 */
				} else {
					/*
					 * An unexpected error occurred.
					 *
					 * Only report it the first time to avoid spamming
					 * the logs
					 *
					 */
					if (!nospam) {
						fprintf (log_get_logfd(),
							_("%s: Can't get unique GID (%s). "
							  "Suppressing additional messages.\n"),
							log_get_progname(), strerror (result));
						SYSLOG ((LOG_ERR,
							"Error checking available GIDs: %s",
							strerror (result)));
						nospam = 1;
					}
					/*
					 * We will continue anyway. Hopefully a later GID
					 * will work properly.
					 */
				}
			}
		}
	}

	/* The code reached here and found no available IDs in the range */
	fprintf (log_get_logfd(),
		_("%s: Can't get unique GID (no more available GIDs)\n"),
		log_get_progname());
	SYSLOG ((LOG_WARN, "no more available GIDs on the system"));
	free (used_gids);
	return -1;
}

