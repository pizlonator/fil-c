/*
 * SPDX-FileCopyrightText: 2013 Eric Biederman
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "defines.h"
#include "prototypes.h"
#include "subordinateio.h"
#include "getdef.h"
#include "idmapping.h"
#include "shadowlog.h"

/*
 * Global variables
 */
static const char Prog[] = "newuidmap";

static bool verify_range(struct passwd *pw, struct map_range *range)
{
	/* An empty range is invalid */
	if (range->count == 0)
		return false;

	/* Test /etc/subuid */
	if (have_sub_uids(pw->pw_name, range->lower, range->count))
		return true;

	/* Allow a process to map its own uid */
	if ((range->count == 1) && (pw->pw_uid == range->lower))
		return true;

	return false;
}

static void verify_ranges(struct passwd *pw, int ranges,
	struct map_range *mappings)
{
	struct map_range *mapping;
	int idx;

	mapping = mappings;
	for (idx = 0; idx < ranges; idx++, mapping++) {
		if (!verify_range(pw, mapping)) {
			fprintf(stderr, _( "%s: uid range [%lu-%lu) -> [%lu-%lu) not allowed\n"),
				Prog,
				mapping->upper,
				mapping->upper + mapping->count,
				mapping->lower,
				mapping->lower + mapping->count);
			exit(EXIT_FAILURE);
		}
	}
}

static void usage(void)
{
	fprintf(stderr, _("usage: %s [<pid>|fd:<pidfd>] <uid> <loweruid> <count> [ <uid> <loweruid> <count> ] ... \n"), Prog);
	exit(EXIT_FAILURE);
}

/*
 * newuidmap - Set the uid_map for the specified process
 */
int main(int argc, char **argv)
{
	char *target_str;
	int proc_dir_fd;
	int ranges;
	struct map_range *mappings;
	struct stat st;
	struct passwd *pw;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	/*
	 * The valid syntax are
	 * newuidmap target_pid
	 */
	if (argc < 2)
		usage();

	target_str = argv[1];
	/* Find the process that needs its user namespace
	 * uid mapping set.
	 */
	if (strlen(target_str) > 3 && strncmp(target_str, "fd:", 3) == 0) {
		/* the user passed in a /proc/pid fd for the process */
		target_str = &target_str[3];
		proc_dir_fd = get_pidfd_from_fd(target_str);
		if (proc_dir_fd < 0)
			usage();
	} else {
		proc_dir_fd = open_pidfd(target_str);
		if (proc_dir_fd < 0)
			usage();
	}

	/* Who am i? */
	pw = get_my_pwent ();
	if (NULL == pw) {
		fprintf (stderr,
			_("%s: Cannot determine your user name.\n"),
			Prog);
		SYSLOG ((LOG_WARN, "Cannot determine the user name of the caller (UID %lu)",
				(unsigned long) getuid ()));
		return EXIT_FAILURE;
	}

	/* Get the effective uid and effective gid of the target process */
	if (fstat(proc_dir_fd, &st) < 0) {
		fprintf(stderr, _("%s: Could not stat directory for target process\n"), Prog);
		return EXIT_FAILURE;
	}

	/* Verify real user and real group matches the password entry
	 * and the effective user and group of the program whose
	 * mappings we have been asked to set.
	 */
	if ((getuid() != pw->pw_uid) ||
	    (!getdef_bool("GRANT_AUX_GROUP_SUBIDS") && (getgid() != pw->pw_gid)) ||
	    (pw->pw_uid != st.st_uid) ||
	    (getgid() != st.st_gid)) {
		fprintf(stderr, _( "%s: Target process is owned by a different user: uid:%lu pw_uid:%lu st_uid:%lu, gid:%lu pw_gid:%lu st_gid:%lu\n" ),
			Prog,
			(unsigned long)getuid(), (unsigned long)pw->pw_uid, (unsigned long)st.st_uid,
			(unsigned long)getgid(), (unsigned long)pw->pw_gid, (unsigned long)st.st_gid);
		return EXIT_FAILURE;
	}

	if (!sub_uid_open(O_RDONLY)) {
		return EXIT_FAILURE;
	}

	ranges = ((argc - 2) + 2) / 3;
	mappings = get_map_ranges(ranges, argc - 2, argv + 2);
	if (!mappings)
		usage();

	verify_ranges(pw, ranges, mappings);

	write_mapping(proc_dir_fd, ranges, mappings, "uid_map", pw->pw_uid);
	sub_uid_close();

	return EXIT_SUCCESS;
}
