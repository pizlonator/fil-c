/*
 * version.c - Return the version of the library
 *
 * Copyright (C) 2015 Karel Zak <kzak@redhat.com>
 *
 */

/**
 * SECTION: version-utils
 * @title: Version functions
 * @short_description: functions to get the library version.
 */

#include <ctype.h>

#include "fdiskP.h"

static const char *lib_version = LIBFDISK_VERSION;
static const char *lib_features[] = {
#if !defined(NDEBUG)	/* libc assert.h stuff */
	"assert",
#endif
	"debug",	/* always enabled */
	NULL
};

/**
 * fdisk_parse_version_string:
 * @ver_string: version string (e.g "2.18.0")
 *
 * Returns: release version code.
 */
int fdisk_parse_version_string(const char *ver_string)
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
 * fdisk_get_library_version:
 * @ver_string: return pointer to the static library version string if not NULL
 *
 * Returns: release version number.
 */
int fdisk_get_library_version(const char **ver_string)
{
	if (ver_string)
		*ver_string = lib_version;

	return fdisk_parse_version_string(lib_version);
}

/**
 * fdisk_get_library_features:
 * @features: returns a pointer to the static array of strings, the array is
 *            terminated by NULL.
 *
 * Returns: number of items in the features array not including the last NULL,
 *          or less than zero in case of error
 *
 * Example:
 * <informalexample>
 *   <programlisting>
 *	const char *features;
 *
 *	fdisk_get_library_features(&features);
 *	while (features && *features)
 *		printf("%s\n", *features++);
 *   </programlisting>
 * </informalexample>
 *
 */
int fdisk_get_library_features(const char ***features)
{
	if (!features)
		return -EINVAL;

	*features = lib_features;
	return ARRAY_SIZE(lib_features) - 1;
}

#ifdef TEST_PROGRAM
static int test_version(struct fdisk_test *ts __attribute__((unused)),
			int argc __attribute__((unused)),
			char *argv[] __attribute__((unused)))
{
	const char *ver;
	const char **features;

	fdisk_get_library_version(&ver);

	printf("Library version: %s\n", ver);
	printf("Library API version: " LIBFDISK_VERSION "\n");
	printf("Library features:");

	fdisk_get_library_features(&features);
	while (features && *features)
		printf(" %s", *features++);

	if (fdisk_get_library_version(NULL) ==
			fdisk_parse_version_string(LIBFDISK_VERSION))
		return 0;

	return -1;
}

int main(int argc, char *argv[])
{
	struct fdisk_test ts[] = {
		{ "--print", test_version, "print versions" },
		{ NULL }
	};

	return fdisk_run_test(ts, argc, argv);
}
#endif
