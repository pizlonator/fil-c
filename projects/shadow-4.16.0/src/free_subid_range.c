/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <unistd.h>
#include "subid.h"
#include "stdlib.h"
#include "prototypes.h"
#include "shadowlog.h"

/* Test program for the subid freeing routine */

static const char Prog[] = "free_subid_range";

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-g] user start count\n", Prog);
	fprintf(stderr, "    Release a user's subuid (or with -g, subgid) range\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int c;
	bool ok;
	struct subordinate_range range;
	bool group = false;   // get subuids by default

	log_set_progname(Prog);
	log_set_logfd(stderr);
	while ((c = getopt(argc, argv, "g")) != EOF) {
		switch(c) {
		case 'g': group = true; break;
		default: usage();
		}
	}
	argv = &argv[optind];
	argc = argc - optind;
	if (argc < 3)
		usage();
	range.owner = argv[0];
	range.start = atoi(argv[1]);
	range.count = atoi(argv[2]);
	if (group)
		ok = subid_ungrant_gid_range(&range);
	else
		ok = subid_ungrant_uid_range(&range);

	if (!ok) {
		fprintf(stderr, "Failed freeing id range\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}
