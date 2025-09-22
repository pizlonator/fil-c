/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sys/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include "c.h"
#include "caputils.h"
#include "pathnames.h"
#include "procfs.h"
#include "nls.h"

static int test_cap(unsigned int cap)
{
	/* prctl returns 0 or 1 for valid caps, -1 otherwise */
	return prctl(PR_CAPBSET_READ, cap, 0, 0, 0) >= 0;
}

static int cap_last_by_bsearch(int *ret)
{
	/* starting with cap=INT_MAX means we always know
	 * that cap1 is invalid after the first iteration */
	int cap = INT_MAX;
	unsigned int cap0 = 0, cap1 = INT_MAX;

	while ((int)cap0 < cap) {
		if (test_cap(cap))
			cap0 = cap;
		else
			cap1 = cap;

		cap = (cap0 + cap1) / 2U;
	}

	*ret = cap;
	return 0;
}

static int cap_last_by_procfs(int *ret)
{
	FILE *f = fopen(_PATH_PROC_CAPLASTCAP, "r");
	int rc = -EINVAL;

	*ret = 0;

	if (f && fd_is_procfs(fileno(f))) {
		int cap;

		/* we check if the cap after this one really isn't valid */
		if (fscanf(f, "%d", &cap) == 1 &&
		    cap < INT_MAX && !test_cap(cap + 1)) {

			*ret = cap;
			rc = 0;
		}
	}

	if (f)
		fclose(f);
	return rc;
}

int cap_last_cap(void)
{
	static int cap = -1;

	if (cap != -1)
		return cap;
	if (cap_last_by_procfs(&cap) < 0)
		cap_last_by_bsearch(&cap);

	return cap;
}

void cap_permitted_to_ambient(void)
{
	/* We use capabilities system calls to propagate the permitted
	 * capabilities into the ambient set because we may have
	 * already forked so be in async-signal-safe context. */
	struct __user_cap_header_struct header = {
		.version = _LINUX_CAPABILITY_VERSION_3,
		.pid = 0,
	};
	struct __user_cap_data_struct payload[_LINUX_CAPABILITY_U32S_3] = {{ 0 }};
	uint64_t effective, cap;

	if (capget(&header, payload) < 0)
		err(EXIT_FAILURE, _("capget failed"));

	/* In order the make capabilities ambient, we first need to ensure
	 * that they are all inheritable. */
	payload[0].inheritable = payload[0].permitted;
	payload[1].inheritable = payload[1].permitted;

	if (capset(&header, payload) < 0)
		err(EXIT_FAILURE, _("capset failed"));

	effective = ((uint64_t)payload[1].effective << 32) |  (uint64_t)payload[0].effective;

	for (cap = 0; cap < (sizeof(effective) * 8); cap++) {
		/* This is the same check as cap_valid(), but using
		 * the runtime value for the last valid cap. */
		if (cap > (uint64_t) cap_last_cap())
			continue;

		if ((effective & (1ULL << cap))
		    && prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) < 0)
			err(EXIT_FAILURE, _("prctl(PR_CAP_AMBIENT) failed"));
	}
}

#ifdef TEST_PROGRAM_CAPUTILS
int main(int argc, char *argv[])
{
	int rc = 0, cap;

	if (argc < 2) {
		fprintf(stderr, "usage: %1$s --last-by-procfs\n"
				"       %1$s --last-by-bsearch\n"
				"       %1$s --last\n",
				program_invocation_short_name);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "--last-by-procfs") == 0) {
		rc = cap_last_by_procfs(&cap);
		if (rc == 0)
			printf("last cap: %d\n", cap);

	} else if (strcmp(argv[1], "--last-by-bsearch") == 0) {
		rc = cap_last_by_bsearch(&cap);
		if (rc == 0)
			printf("last cap: %d\n", cap);

	} else if (strcmp(argv[1], "--last") == 0)
		printf("last cap: %d\n", cap_last_cap());

	return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
