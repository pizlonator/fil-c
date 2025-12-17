/*
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

#ifndef GNUTLS_TESTS_VIRT_TIME_H
#define GNUTLS_TESTS_VIRT_TIME_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include <gnutls/gnutls.h>

/* virtualize time in a test. This freezes the time in the test, except for
 * the advances due to calls to virt_sleep_sec(). This makes the test
 * independent of the test system load, and avoids any long delays. */
static time_t _now;

#define virt_sec_sleep(s)  \
	{                  \
		_now += s; \
	}

#define virt_time_init_at(d)                             \
	{                                                \
		_now = (d);                              \
		gnutls_global_set_time_function(mytime); \
	}

#define virt_time_init() virt_time_init_at(time(0))

static time_t mytime(time_t *t)
{
	if (t)
		*t = _now;

	return _now;
}

#endif /* GNUTLS_TESTS_VIRT_TIME_H */
