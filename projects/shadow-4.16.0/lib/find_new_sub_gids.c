/*
 * SPDX-FileCopyrightText: 2012 Eric Biederman
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>

#ifdef ENABLE_SUBIDS

#include <assert.h>
#include <stdio.h>
#include <errno.h>

#include "prototypes.h"
#include "subordinateio.h"
#include "getdef.h"
#include "shadowlog.h"

/*
 * find_new_sub_gids - Find a new unused range of GIDs.
 *
 * If successful, find_new_sub_gids provides a range of unused
 * user IDs in the [SUB_GID_MIN:SUB_GID_MAX] range.
 *
 * Return 0 on success, -1 if no unused GIDs are available.
 */
int find_new_sub_gids (gid_t *range_start, unsigned long *range_count)
{
	unsigned long min, max;
	unsigned long count;
	gid_t start;

	assert (range_start != NULL);
	assert (range_count != NULL);

	min = getdef_ulong ("SUB_GID_MIN", 100000UL);
	max = getdef_ulong ("SUB_GID_MAX", 600100000UL);
	count = getdef_ulong ("SUB_GID_COUNT", 65536);

	if (min > max || count >= max || (min + count - 1) > max) {
		(void) fprintf (log_get_logfd(),
				_("%s: Invalid configuration: SUB_GID_MIN (%lu),"
				  " SUB_GID_MAX (%lu), SUB_GID_COUNT (%lu)\n"),
			log_get_progname(), min, max, count);
		return -1;
	}

	start = sub_gid_find_free_range(min, max, count);
	if (start == (gid_t)-1) {
		fprintf (log_get_logfd(),
		         _("%s: Can't get unique subordinate GID range\n"),
		         log_get_progname());
		SYSLOG ((LOG_WARN, "no more available subordinate GIDs on the system"));
		return -1;
	}
	*range_start = start;
	*range_count = count;
	return 0;
}
#else				/* !ENABLE_SUBIDS */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* !ENABLE_SUBIDS */

