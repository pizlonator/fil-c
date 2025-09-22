/*
 * version.c - Return the version of the library
 *
 * Copyright (C) 2014 Karel Zak <kzak@redhat.com>
 *
 * See COPYING.libmount for the License of this software.
 */

/**
 * SECTION: version-utils
 * @title: Version functions
 * @short_description: functions to get the library version.
 *
 * Note that library version is not the same thing as SONAME version. The
 * libsmarcols uses symbols versioning and SONAME is not modified for releases.
 *
 * The library version and symbols version follow util-linux package versioning.
 */

#include <ctype.h>

#include "smartcolsP.h"

static const char *lib_version = LIBSMARTCOLS_VERSION;

/**
 * scols_parse_version_string:
 * @ver_string: version string (e.g "2.18.0")
 *
 * Returns: release version code.
 */
int scols_parse_version_string(const char *ver_string)
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
 * scols_get_library_version:
 * @ver_string: return pointer to the static library version string if not NULL
 *
 * Returns: release version number.
 */
int scols_get_library_version(const char **ver_string)
{
	if (ver_string)
		*ver_string = lib_version;

	return scols_parse_version_string(lib_version);
}

