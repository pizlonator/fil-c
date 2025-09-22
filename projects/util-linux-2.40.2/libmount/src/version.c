/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * This file is part of libmount from util-linux project.
 *
 * Copyright (C) 2008-2018 Karel Zak <kzak@redhat.com>
 *
 * libmount is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

/**
 * SECTION: version-utils
 * @title: Version functions
 * @short_description: functions to get the library version.
 */

#include <ctype.h>

#include "mountP.h"
#include "mount-api-utils.h"

static const char *lib_version = LIBMOUNT_VERSION;
static const char *lib_features[] = {
#ifdef HAVE_LIBSELINUX
	"selinux",
#endif
#ifdef HAVE_SMACK
	"smack",
#endif
#ifdef HAVE_BTRFS_SUPPORT
	"btrfs",
#endif
#ifdef HAVE_CRYPTSETUP
	"verity",
#endif
#ifdef USE_LIBMOUNT_SUPPORT_NAMESPACES
	"namespaces",
#endif
#if defined(HAVE_MOUNTFD_API) && defined(HAVE_LINUX_MOUNT_H)
	"idmapping",
#endif
#ifdef USE_LIBMOUNT_MOUNTFD_SUPPORT
	"fd-based-mount",
#endif
#if defined(HAVE_STATX) && defined(HAVE_STRUCT_STATX) && defined(AT_STATX_DONT_SYNC)
	"statx",
#endif
#if !defined(NDEBUG)
	"assert",	/* libc assert.h stuff */
#endif
	"debug",	/* always enabled */
	NULL
};

/**
 * mnt_parse_version_string:
 * @ver_string: version string (e.g "2.18.0")
 *
 * Returns: release version code.
 */
int mnt_parse_version_string(const char *ver_string)
{
	const char *cp;
	int version = 0;

	assert(ver_string);

	for (cp = ver_string; *cp; cp++) {
		if (*cp == '.')
			continue;
		if (!isdigit(*cp))
			break;
		version = (version * 10) + (*cp - '0');
	}
	return version;
}

/**
 * mnt_get_library_version:
 * @ver_string: return pointer to the static library version string if not NULL
 *
 * Returns: release version number.
 */
int mnt_get_library_version(const char **ver_string)
{
	if (ver_string)
		*ver_string = lib_version;

	return mnt_parse_version_string(lib_version);
}

/**
 * mnt_get_library_features:
 * @features: returns a pointer to the static array of strings, the array is
 *            terminated by NULL.
 *
 * Returns: number of items in the features array not including the last NULL,
 *          or less than zero in case of error
 *
 * Example:
 * <informalexample>
 *   <programlisting>
 *	const char **features;
 *
 *	mnt_get_library_features(&features);
 *	while (features && *features)
 *		printf("%s\n", *features++);
 *   </programlisting>
 * </informalexample>
 *
 */
int mnt_get_library_features(const char ***features)
{
	if (!features)
		return -EINVAL;

	*features = lib_features;
	return ARRAY_SIZE(lib_features) - 1;
}

#ifdef TEST_PROGRAM
static int test_version(struct libmnt_test *ts __attribute__((unused)),
			int argc, char *argv[])
{
	const char *ver;
	const char **features;

	if (argc == 2)
		printf("Your version: %d\n",
				mnt_parse_version_string(argv[1]));

	mnt_get_library_version(&ver);

	printf("Library version: %s\n", ver);
	printf("Library API version: " LIBMOUNT_VERSION "\n");
	printf("Library features:");

	mnt_get_library_features(&features);
	while (features && *features)
		printf(" %s", *features++);

	printf("\n");

	if (mnt_get_library_version(NULL) ==
			mnt_parse_version_string(LIBMOUNT_VERSION))
		return 0;

	return -1;
}

int main(int argc, char *argv[])
{
	struct libmnt_test ts[] = {
		{ "--print", test_version, "print versions" },
		{ NULL }
	};

	return mnt_run_test(ts, argc, argv);
}
#endif
