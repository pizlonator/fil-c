/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2018 Vaclav Dolezal <vdolezal@redhat.com>
 *
 * This file is part of util-linux.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "mbsalign.h"

int main(int argc, char **argv)
{
	int i = 1;
	char *(*encode_fn)(const char *, size_t *) = mbs_safe_encode;

	setlocale(LC_ALL, "");

	if (i < argc) {
		if (!strcmp(argv[i], "--safe")) {
			i++;
			encode_fn = mbs_safe_encode;
		} else if (!strcmp(argv[i], "--invalid")) {
			i++;
			encode_fn = mbs_invalid_encode;
		} else if (!strcmp(argv[i], "--")) {
			i++;
		}
	}

	for (; i < argc; i++) {
		size_t width;
		char *res;
		res = encode_fn(argv[i], &width);
		printf("%zi %s\n", width, res);
		free(res);
	}

	return 0;
}
