/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This test program prints errno messages to allow for portable
 * verification of error messages.
 *
 * Copyright (C) 2019 Patrick Steinhardt <ps@pks.im
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define E(x) { #x, x }
static struct {
	const char *str;
	int error;
} errors[] = {
	E(ENOENT),
	E(ENOTTY),
	E(EILSEQ),
	E(EINVAL),
};

int main(int argc, const char *argv[])
{
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s <errno>\n", argv[0]);
		return -1;
	}

	for (i = 0; i < sizeof(errors)/sizeof(*errors); i++) {
		if (strcmp(errors[i].str, argv[1]) != 0)
			continue;
		puts(strerror(errors[i].error));
		return 0;
	}

	fprintf(stderr, "Invalid errno: %s\n", argv[1]);
	return -1;
}
