/*
 * devname.c --- Support function to translate a user provided string
 * identifying a device to an actual device path
 *
 * Copyright (C) 2022 Red Hat, Inc., Lukas Czerner <lczerner@redhat.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "devname.h"
#include "nls-enable.h"

/*
 *  blkid_get_devname() is primarily intended for parsing "NAME=value"
 *  tokens. It will return the device matching the specified token, NULL if
 *  nothing is found, or copy of the string if it's not in "NAME=value"
 *  format.
 *  get_devname() takes the same parameters and works the same way as
 *  blkid_get_devname() except it can handle '=' in the file name.
 */
char *get_devname(blkid_cache cache, const char *token, const char *value)
{
	int is_file = 0;
	char *ret = NULL;

	if (!token)
		goto out;

	if (value) {
		ret = blkid_get_devname(cache, token, value);
		goto out;
	}

	if (access(token, F_OK) == 0)
		is_file = 1;

	ret = blkid_get_devname(cache, token, NULL);
	if (ret) {
		/*
		 * In case of collision prefer the result from
		 * blkid_get_devname() to avoid a file masking file system with
		 * existing tag.
		 */
		if (is_file && (strcmp(ret, token) != 0)) {
			fprintf(stderr,
				_("Collision found: '%s' refers to both '%s' "
				  "and a file '%s'. Using '%s'!\n"),
				token, ret, token, ret);
		}
		goto out;
	}

	if (is_file)
		ret = strdup(token);
out:
	return ret;
}
