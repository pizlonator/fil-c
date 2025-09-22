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
static const char Prog[] = "newgidmap";


static bool verify_range(struct passwd *pw, struct map_range *range, bool *allow_setgroups)
{
	/* An empty range is invalid */
	if (range->count == 0)
		return false;

	/* Test /etc/subgid. If the mapping is valid then we allow setgroups. */
	if (have_sub_gids(pw->pw_name, range->lower, range->count)) {
		*allow_setgroups = true;
		return true;
	}

	/* Allow a process to map its own gid. */
	if ((range->count == 1) && (getgid() == range->lower)) {
		/* noop -- if setgroups is enabled already we won't disable it. */
		return true;
	}

	return false;
}

static void verify_ranges(struct passwd *pw, int ranges,
	struct map_range *mappings, bool *allow_setgroups)
{
	struct map_range *mapping;
	int idx;

	mapping = mappings;
	for (idx = 0; idx < ranges; idx++, mapping++) {
		if (!verify_range(pw, mapping, allow_setgroups)) {
			fprintf(stderr, _( "%s: gid range [%lu-%lu) -> [%lu-%lu) not allowed\n"),
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
	fprintf(stderr, _("usage: %s [<pid|fd:<pidfd>] <gid> <lowergid> <count> [ <gid> <lowergid> <count> ] ... \n"), Prog);
	exit(EXIT_FAILURE);
}

static void write_setgroups(int proc_dir_fd, bool allow_setgroups)
{
	int setgroups_fd;
	const char *policy;
	char policy_buffer[4096];

	/*
	 * Default is "deny", and any "allow" will out-rank a "deny". We don't
	 * forcefully write an "allow" here because the process we are writing
	 * mappings for may have already set themselves to "deny" (and "allow"
	 * is the default anyway). So allow_setgroups == true is a noop.
	 */
	policy = "deny\n";
	if (allow_setgroups)
		return;

	setgroups_fd = openat(proc_dir_fd, "setgroups", O_RDWR|O_CLOEXEC);
	if (setgroups_fd < 0) {
		/*
		 * If it's an ENOENT then we are on too old a kernel for the setgroups
		 * code to exist. Emit a warning and bail on this.
		 */
		if (ENOENT == errno) {
			fprintf(stderr, _("%s: kernel doesn't support setgroups restrictions\n"), Prog);
			goto out;
		}
		fprintf(stderr, _("%s: couldn't open process setgroups: %s\n"),
			Prog,
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	/*
	 * Check whether the policy is already what we want. /proc/self/setgroups
	 * is write-once, so attempting to write after it's already written to will
	 * fail.
	 */
	if (read(setgroups_fd, policy_buffer, sizeof(policy_buffer)) < 0) {
		fprintf(stderr, _("%s: failed to read setgroups: %s\n"),
			Prog,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (!strncmp(policy_buffer, policy, strlen(policy)))
		goto out;

	/* Write the policy. */
	if (lseek(setgroups_fd, 0, SEEK_SET) < 0) {
		fprintf(stderr, _("%s: failed to seek setgroups: %s\n"),
			Prog,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (dprintf(setgroups_fd, "%s", policy) < 0) {
		fprintf(stderr, _("%s: failed to setgroups %s policy: %s\n"),
			Prog,
			policy,
			strerror(errno));
		exit(EXIT_FAILURE);
	}

out:
	close(setgroups_fd);
}

/*
 * newgidmap - Set the gid_map for the specified process
 */
int main(int argc, char **argv)
{
	char *target_str;
	int proc_dir_fd;
	int ranges;
	struct map_range *mappings;
	struct stat st;
	struct passwd *pw;
	bool allow_setgroups = false;

	log_set_progname(Prog);
	log_set_logfd(stderr);

	/*
	 * The valid syntax are
	 * newgidmap target_pid
	 */
	if (argc < 2)
		usage();

	/* Find the process that needs its user namespace
	 * gid mapping set.
	 */

	target_str = argv[1];
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
		fprintf(stderr, _("%s: Could not stat directory for process\n"),
			Prog);
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

	if (!sub_gid_open(O_RDONLY)) {
		return EXIT_FAILURE;
	}

	ranges = ((argc - 2) + 2) / 3;
	mappings = get_map_ranges(ranges, argc - 2, argv + 2);
	if (!mappings)
		usage();

	verify_ranges(pw, ranges, mappings, &allow_setgroups);

	write_setgroups(proc_dir_fd, allow_setgroups);
	write_mapping(proc_dir_fd, ranges, mappings, "gid_map", pw->pw_uid);
	sub_gid_close();

	return EXIT_SUCCESS;
}
