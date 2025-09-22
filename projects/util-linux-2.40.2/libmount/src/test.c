/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2010-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 *
 * Routines for TEST_PROGRAMs
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef TEST_PROGRAM
#define TEST_PROGRAM
#endif

#include "mountP.h"

int mnt_run_test(struct libmnt_test *tests, int argc, char *argv[])
{
	int rc = -1;
	struct libmnt_test *ts;

	assert(tests);
	assert(argc);
	assert(argv);

	if (argc < 2 ||
	    strcmp(argv[1], "--help") == 0 ||
	    strcmp(argv[1], "-h") == 0)
		goto usage;

	mnt_init_debug(0);

	for (ts = tests; ts->name; ts++) {
		if (strcmp(ts->name, argv[1]) == 0) {
			rc = ts->body(ts, argc - 1, argv + 1);
			if (rc)
				printf("FAILED [rc=%d]", rc);
			break;
		}
	}

	if (rc < 0 && ts->name == NULL)
		goto usage;

	return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	printf("\nUsage:\n\t%s <test> [testoptions]\nTests:\n",
			program_invocation_short_name);
	for (ts = tests; ts->name; ts++) {
		printf("\t%-15s", ts->name);
		if (ts->usage)
			printf(" %s\n", ts->usage);
	}
	printf("\n");
	return EXIT_FAILURE;
}
