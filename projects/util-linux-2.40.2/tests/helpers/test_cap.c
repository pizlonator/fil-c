/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2023 Thomas Weißschuh <thomas@t-8ch.de>
 */

#include <stdlib.h>
#include <stdio.h>

#include <cap-ng.h>

int main(int argc, char **argv)
{
	int cap, r;

	if (argc != 2)
		return EXIT_FAILURE;

	cap = capng_name_to_capability(argv[1]);
	if (cap < 0) {
		fprintf(stderr, "capng_name_to_capability(%s) failed\n", argv[1]);
		return EXIT_FAILURE;
	}

	r = capng_get_caps_process();
	if (r) {
		fprintf(stderr, "capng_get_caps_process() failed\n");
		return EXIT_FAILURE;
	}

	r = capng_have_capability(CAPNG_EFFECTIVE, cap);
	return r ? EXIT_SUCCESS : EXIT_FAILURE;
}
