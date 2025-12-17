/*
 * Copyright (C) 2020-2022 Red Hat, Inc.
 *
 * Authors: Ondrej Moris, Zoltan Fridrich
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_DL_ITERATE_PHDR

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <link.h>
#include "dirname.h"
#include "errors.h"

#define FORMAT_VERSION 1

/* These only works with the platform where SONAME is part of the ABI. */
#ifndef GNUTLS_LIBRARY_SONAME
#define GNUTLS_LIBRARY_SONAME "none"
#endif

#ifndef NETTLE_LIBRARY_SONAME
#define NETTLE_LIBRARY_SONAME "none"
#endif

#ifndef HOGWEED_LIBRARY_SONAME
#define HOGWEED_LIBRARY_SONAME "none"
#endif

#define HMAC_SIZE 32
#define HMAC_ALGO GNUTLS_MAC_SHA256
#define HMAC_STR_SIZE (2 * HMAC_SIZE + 1)

static int get_hmac(const char *path, char *hmac, size_t hmac_size)
{
	int ret;
	size_t size;
	uint8_t buffer[HMAC_SIZE];
	gnutls_datum_t hex = { buffer, sizeof(buffer) };
	gnutls_datum_t data = { NULL, 0 };

	ret = gnutls_load_file(path, &data);
	if (ret < 0)
		return gnutls_assert_val(ret);

	GNUTLS_FIPS140_SET_LAX_MODE();
	ret = gnutls_hmac_fast(HMAC_ALGO, FIPS_KEY, sizeof(FIPS_KEY) - 1,
			       data.data, data.size, buffer);
	GNUTLS_FIPS140_SET_STRICT_MODE();

	gnutls_free(data.data);
	if (ret < 0)
		return gnutls_assert_val(ret);

	size = hmac_size;
	ret = gnutls_hex_encode(&hex, hmac, &size);
	if (ret < 0)
		return gnutls_assert_val(ret);

	return 0;
}

static int print_lib(const char *path, const char *soname)
{
	int ret;
	char *real_path = NULL;
	char hmac[HMAC_STR_SIZE];

	real_path = canonicalize_file_name(path);
	if (real_path == NULL) {
		fprintf(stderr, "Could not get realpath from %s\n", path);
		ret = GNUTLS_E_FILE_ERROR;
		goto cleanup;
	}

	ret = get_hmac(real_path, hmac, sizeof(hmac));
	if (ret < 0) {
		fprintf(stderr, "Could not calculate HMAC for %s: %s\n",
			last_component(real_path), gnutls_strerror(ret));
		goto cleanup;
	}

	printf("[%s]\n", soname);
	printf("path = %s\n", real_path);
	printf("hmac = %s\n", hmac);

cleanup:
	free(real_path);
	return ret;
}

static int callback(struct dl_phdr_info *info, size_t size, void *data)
{
	const char *path = info->dlpi_name;
	const char *soname = last_component(path);

	if (!strcmp(soname, GNUTLS_LIBRARY_SONAME))
		return print_lib(data ? data : path, soname);
	if (!strcmp(soname, NETTLE_LIBRARY_SONAME))
		return print_lib(path, soname);
	if (!strcmp(soname, HOGWEED_LIBRARY_SONAME))
		return print_lib(path, soname);
#ifdef GMP_LIBRARY_SONAME
	if (!strcmp(soname, GMP_LIBRARY_SONAME))
		return print_lib(path, soname);
#endif
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Usage: %s [gnutls_so_path]\n",
			last_component(argv[0]));
		return EXIT_FAILURE;
	}

	printf("[global]\n");
	printf("format-version = %d\n", FORMAT_VERSION);

	return dl_iterate_phdr(callback, argc == 2 ? argv[1] : NULL);
}

#else

int main(void)
{
	fprintf(stderr, "Function dl_iterate_phdr is missing\n");
	return EXIT_FAILURE;
}

#endif /* HAVE_DL_ITERATE_PHDR */
