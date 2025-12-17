/*
 * Copyright (C) 2004-2012 Free Software Foundation, Inc.
 *
 * Author: Simon Josefsson
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
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

#define CHECK_OK(x, y, z)                                                            \
	if (x >= 0 && y >= 0 && z >= 0) {                                            \
		if (!gnutls_check_version_numeric(x, y, z)) {                        \
			fail("error in gnutls_check_version_numeric %d.%d.%d: %d\n", \
			     x, y, z, __LINE__);                                     \
			exit(1);                                                     \
		}                                                                    \
	}

#define CHECK_FAIL(x, y, z)                                                      \
	if (gnutls_check_version_numeric(x, y, z)) {                             \
		fail("error in neg gnutls_check_version_numeric %d.%d.%d: %d\n", \
		     x, y, z, __LINE__);                                         \
		exit(1);                                                         \
	}

void doit(void)
{
	if (debug) {
		printf("GnuTLS header version %s.\n", GNUTLS_VERSION);
		printf("GnuTLS library version %s.\n",
		       gnutls_check_version(NULL));
	}

	if (!gnutls_check_version_numeric(GNUTLS_VERSION_MAJOR,
					  GNUTLS_VERSION_MINOR,
					  GNUTLS_VERSION_PATCH)) {
		fail("error in gnutls_check_version_numeric 1\n");
		exit(1);
	}

	CHECK_FAIL(99, 9, 9)
	CHECK_FAIL(90, 1, 0)
	CHECK_FAIL(90, 0, 0)
	CHECK_OK(2, 0, 0)
	CHECK_OK(2, 99, 99)
	CHECK_OK(3, 0, 0)
	if (!gnutls_check_version(GNUTLS_VERSION))
		fail("gnutls_check_version ERROR\n");

	{
		const gnutls_pk_algorithm_t *algs;
		size_t i;
		int pk;

		algs = gnutls_pk_list();
		if (!algs)
			fail("gnutls_pk_list return NULL\n");

		for (i = 0; algs[i]; i++) {
			if (debug)
				printf("pk_list[%d] = %d = %s = %d\n", (int)i,
				       algs[i],
				       gnutls_pk_algorithm_get_name(algs[i]),
				       gnutls_pk_get_id(
					       gnutls_pk_algorithm_get_name(
						       algs[i])));
			if (gnutls_pk_get_id(gnutls_pk_algorithm_get_name(
				    algs[i])) != algs[i])
				fail("gnutls_pk id doesn't match\n");
		}

		pk = gnutls_pk_get_id("foo");
		if (pk != GNUTLS_PK_UNKNOWN)
			fail("gnutls_pk unknown test failed (%d)\n", pk);

		if (debug)
			success("gnutls_pk_list ok\n");
	}

	{
		const gnutls_sign_algorithm_t *algs;
		size_t i;
		int pk;

		algs = gnutls_sign_list();
		if (!algs)
			fail("gnutls_sign_list return NULL\n");

		for (i = 0; algs[i]; i++) {
			gnutls_digest_algorithm_t hash;

			if (debug)
				printf("sign_list[%d] = %d = %s = %d\n", (int)i,
				       algs[i],
				       gnutls_sign_algorithm_get_name(algs[i]),
				       gnutls_sign_get_id(
					       gnutls_sign_algorithm_get_name(
						       algs[i])));
			if (gnutls_sign_get_id(gnutls_sign_algorithm_get_name(
				    algs[i])) != algs[i])
				fail("gnutls_sign id for %s doesn't match\n",
				     gnutls_sign_algorithm_get_name(algs[i]));

			hash = gnutls_sign_get_hash_algorithm(algs[i]);
			if (hash != GNUTLS_DIG_UNKNOWN) {
				const char *name = gnutls_digest_get_name(hash);
				gnutls_digest_algorithm_t hash2 =
					gnutls_digest_get_id(name);
				/* gnutls_digest_get_id returns
				 * GNUTLS_DIG_UNKNOWN if the algorithm is not
				 * compiled in.
				 */
				if (hash2 != GNUTLS_DIG_UNKNOWN &&
				    hash2 != hash)
					fail("gnutls_digest id for %s doesn't match %s\n",
					     gnutls_sign_algorithm_get_name(
						     algs[i]),
					     name);
			}
		}

		pk = gnutls_sign_get_id("foo");
		if (pk != GNUTLS_PK_UNKNOWN)
			fail("gnutls_sign unknown test failed (%d)\n", pk);

		if (debug)
			success("gnutls_sign_list ok\n");
	}
}
