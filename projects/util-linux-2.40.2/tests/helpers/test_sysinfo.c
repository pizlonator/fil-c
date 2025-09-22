/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2007 Karel Zak <kzak@redhat.com>
 *
 * This file is part of util-linux.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <wchar.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include "mount-api-utils.h"

typedef struct {
	const char	*name;
	int		(*fnc)(void);
} mntHlpfnc;

static int hlp_wordsize(void)
{
	printf("%zu\n", CHAR_BIT * sizeof(void*));
	return 0;
}

static int hlp_endianness(void)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	printf("LE\n");
#else
	printf("BE\n");
#endif
	return 0;
}


static int hlp_pagesize(void)
{
	printf("%d\n", getpagesize());
	return 0;
}

static int hlp_int_max(void)
{
	printf("%d\n", INT_MAX);
	return 0;
}

static int hlp_uint_max(void)
{
	printf("%u\n", UINT_MAX);
	return 0;
}

static int hlp_long_max(void)
{
	printf("%ld\n", LONG_MAX);
	return 0;
}

static int hlp_ulong_max(void)
{
	printf("%lu\n", ULONG_MAX);
	return 0;
}

static int hlp_u64_max(void)
{
	printf("%" PRIu64 "\n", UINT64_MAX);
	return 0;
}

static int hlp_ulong_max32(void)
{
#if __WORDSIZE == 64
	printf("%lu\n", ULONG_MAX >> 32);
#else
	printf("%lu\n", ULONG_MAX);
#endif
	return 0;
}

static int hlp_wcsspn_ok(void)
{
	printf("%d\n", wcsspn(L"FOO", L"F") == 1);
	return 0;
}

static int hlp_enotty_ok(void)
{
	errno = 0;
	ioctl(STDOUT_FILENO, 0);

	printf("%d\n", errno != ENOSYS);
	return 0;
}

static int hlp_fsopen_ok(void)
{
#ifdef FSOPEN_CLOEXEC
	errno = 0;
	fsopen(NULL, FSOPEN_CLOEXEC);
#else
	errno = ENOSYS;
#endif
	printf("%d\n", errno != ENOSYS);
	return 0;
}

static int hlp_sz_time(void)
{
	printf("%zu\n", sizeof(time_t));
	return 0;
}

static const mntHlpfnc hlps[] =
{
	{ "WORDSIZE",	hlp_wordsize	},
	{ "pagesize",	hlp_pagesize	},
	{ "INT_MAX",	hlp_int_max	},
	{ "UINT_MAX",   hlp_uint_max	},
	{ "LONG_MAX",   hlp_long_max	},
	{ "ULONG_MAX",  hlp_ulong_max	},
	{ "ULONG_MAX32",hlp_ulong_max32	},
	{ "UINT64_MAX", hlp_u64_max     },
	{ "byte-order", hlp_endianness  },
	{ "wcsspn-ok",  hlp_wcsspn_ok   },
	{ "enotty-ok",  hlp_enotty_ok   },
	{ "fsopen-ok",  hlp_fsopen_ok   },
	{ "sz(time_t)", hlp_sz_time     },
	{ NULL, NULL }
};

int main(int argc, char **argv)
{
	int re = 0;
	const mntHlpfnc *fn;

	if (argc == 1) {
		for (fn = hlps; fn->name; fn++) {
			printf("%15s: ", fn->name);
			re += fn->fnc();
		}
	} else {
		int i;

		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
			printf("%s <option>\n", argv[0]);
			fputs("options:\n", stdout);
			for (fn = hlps; fn->name; fn++)
				printf("\t%s\n", fn->name);
			exit(EXIT_SUCCESS);
		}

		for (i=1; i < argc; i++) {
			for (fn = hlps; fn->name; fn++) {
				if (strcmp(fn->name, argv[i]) == 0)
					re += fn->fnc();
			}
		}
	}

	exit(re ? EXIT_FAILURE : EXIT_SUCCESS);
}

