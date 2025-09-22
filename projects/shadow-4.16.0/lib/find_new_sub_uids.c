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
 * find_new_sub_uids - Find a new unused range of UIDs.
 *
 * If successful, find_new_sub_uids provides a range of unused
 * user IDs in the [SUB_UID_MIN:SUB_UID_MAX] range.
 *
 * Return 0 on success, -1 if no unused UIDs are available.
 */
int find_new_sub_uids (uid_t *range_start, unsigned long *range_count)
{
	unsigned long min, max;
	unsigned long count;
	uid_t start;

	assert (range_start != NULL);
	assert (range_count != NULL);

	min = getdef_ulong ("SUB_UID_MIN", 100000UL);
	max = getdef_ulong ("SUB_UID_MAX", 600100000UL);
	count = getdef_ulong ("SUB_UID_COUNT", 65536);

	if (min > max || count >= max || (min + count - 1) > max) {
		(void) fprintf (log_get_logfd(),
				_("%s: Invalid configuration: SUB_UID_MIN (%lu),"
				  " SUB_UID_MAX (%lu), SUB_UID_COUNT (%lu)\n"),
			log_get_progname(), min, max, count);
		return -1;
	}

	start = sub_uid_find_free_range(min, max, count);
	if (start == (uid_t)-1) {
		fprintf (log_get_logfd(),
		         _("%s: Can't get unique subordinate UID range\n"),
		         log_get_progname());
		SYSLOG ((LOG_WARN, "no more available subordinate UIDs on the system"));
		return -1;
	}
	*range_start = start;
	*range_count = count;
	return 0;
}
#else				/* !ENABLE_SUBIDS */
extern int ISO_C_forbids_an_empty_translation_unit;
#endif				/* !ENABLE_SUBIDS */

