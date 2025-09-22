// SPDX-FileCopyrightText: 2017, Chris Lamb
// SPDX-FileCopyrightText: 2023-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#include <config.h>

#ident "$Id$"

#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "atoi/a2i.h"
#include "defines.h"
#include "prototypes.h"
#include "shadowlog.h"


/*
 * gettime() returns the time as the number of seconds since the Epoch
 *
 * Like time(), gettime() returns the time as the number of seconds since the
 * Epoch, 1970-01-01 00:00:00 +0000 (UTC), except that if the SOURCE_DATE_EPOCH
 * environment variable is exported it will use that instead.
 */
/*@observer@*/time_t
gettime(void)
{
	char    *source_date_epoch;
	FILE    *shadow_logfd = log_get_logfd();
	time_t  fallback, epoch;

	fallback = time (NULL);
	source_date_epoch = shadow_getenv ("SOURCE_DATE_EPOCH");

	if (!source_date_epoch)
		return fallback;

	if (a2i(time_t, &epoch, source_date_epoch, NULL, 10, 0, fallback) == -1) {
		fprintf(shadow_logfd,
		        _("Environment variable $SOURCE_DATE_EPOCH: a2i(\"%s\"): %s"),
		        source_date_epoch, strerror(errno));
		return fallback;
	}
	return epoch;
}
