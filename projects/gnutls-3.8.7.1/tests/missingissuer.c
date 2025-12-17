/*
 * Copyright (C) 2008-2014 Free Software Foundation, Inc.
 *
 * Author: Simon Josefsson, Nikos Mavrogiannopoulos
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

#include "utils.h"
#include "test-chains-issuer.h"

#define DEFAULT_THEN 1605514504
static time_t then = DEFAULT_THEN;

/* GnuTLS internally calls time() to find out the current time when
   verifying certificates.  To avoid a time bomb, we hard code the
   current time.  This should work fine on systems where the library
   call to time is resolved at run-time.  */
static time_t mytime(time_t *t)
{
	if (t)
		*t = then;

	return then;
}

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "|<%d>| %s", level, str);
}

struct getissuer_data {
	const char **insert;
	unsigned int count;
};

static int getissuer_callback(gnutls_x509_trust_list_t tlist,
			      const gnutls_x509_crt_t crt,
			      gnutls_x509_crt_t **issuers,
			      unsigned int *issuers_size)
{
	gnutls_datum_t tmp;
	int ret;
	unsigned int i;
	struct getissuer_data *data;

	data = gnutls_x509_trust_list_get_ptr(tlist);

	tmp.data = (unsigned char *)data->insert[data->count];
	if (!tmp.data) {
		fprintf(stderr,
			"getissuer_callback is called more times than expected\n");
		return -1;
	}

	tmp.size = strlen(data->insert[data->count]);

	data->count++;

	ret = gnutls_x509_crt_list_import2(issuers, issuers_size, &tmp,
					   GNUTLS_X509_FMT_PEM, 0);
	if (ret < 0) {
		fprintf(stderr, "error: %s\n", gnutls_strerror(ret));
		return -1;
	}

	assert(gnutls_x509_crt_print(crt, GNUTLS_CRT_PRINT_ONELINE, &tmp) >= 0);

	if (debug)
		printf("\t Certificate missing issuer is: %.*s\n", tmp.size,
		       tmp.data);
	gnutls_free(tmp.data);

	for (i = 0; i < *issuers_size; i++) {
		assert(gnutls_x509_crt_print((*issuers)[i],
					     GNUTLS_CRT_PRINT_ONELINE,
					     &tmp) >= 0);

		if (debug)
			printf("\t Appended issuer certificate is: %.*s\n",
			       tmp.size, tmp.data);
		gnutls_free(tmp.data);
	}

	return 0;
}

void doit(void)
{
	int exit_val = 0;
	int ret;
	gnutls_x509_trust_list_t tl;
	unsigned int verify_status;
	gnutls_x509_crt_t certs[MAX_CHAIN];
	gnutls_x509_crt_t ca;
	gnutls_datum_t tmp;
	size_t i, j;

	/* The overloading of time() seems to work in linux (ELF?)
	 * systems only. Disable it on windows.
	 */
#ifdef _WIN32
	exit(77);
#endif

	ret = global_init();
	if (ret != 0) {
		fail("%d: %s\n", ret, gnutls_strerror(ret));
		exit(1);
	}

	gnutls_global_set_time_function(mytime);
	gnutls_global_set_log_function(tls_log_func);

	if (debug)
		gnutls_global_set_log_level(4711);

	for (i = 0; chains[i].chain; i++) {
		struct getissuer_data data;

		printf("[%d]: Chain '%s'...\n", (int)i, chains[i].name);

		for (j = 0; chains[i].chain[j]; j++) {
			assert(j < MAX_CHAIN);

			if (debug > 2)
				printf("\tAdding certificate %d...", (int)j);

			ret = gnutls_x509_crt_init(&certs[j]);
			if (ret < 0) {
				fprintf(stderr,
					"gnutls_x509_crt_init[%d]: %s\n",
					(int)j, gnutls_strerror(ret));
				exit(1);
			}

			tmp.data = (unsigned char *)chains[i].chain[j];
			tmp.size = strlen(chains[i].chain[j]);

			ret = gnutls_x509_crt_import(certs[j], &tmp,
						     GNUTLS_X509_FMT_PEM);
			if (debug > 2)
				printf("done\n");
			if (ret < 0) {
				fprintf(stderr,
					"gnutls_x509_crt_import[%d]: %s\n",
					(int)j, gnutls_strerror(ret));
				exit(1);
			}

			gnutls_x509_crt_print(certs[j],
					      GNUTLS_CRT_PRINT_ONELINE, &tmp);
			if (debug)
				printf("\tCertificate %d: %.*s\n", (int)j,
				       tmp.size, tmp.data);
			gnutls_free(tmp.data);
		}

		if (debug > 2)
			printf("\tAdding CA certificate...");

		ret = gnutls_x509_crt_init(&ca);
		if (ret < 0) {
			fprintf(stderr, "gnutls_x509_crt_init: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		tmp.data = (unsigned char *)*chains[i].ca;
		tmp.size = strlen(*chains[i].ca);

		ret = gnutls_x509_crt_import(ca, &tmp, GNUTLS_X509_FMT_PEM);
		if (ret < 0) {
			fprintf(stderr, "gnutls_x509_crt_import: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		if (debug > 2)
			printf("done\n");

		gnutls_x509_crt_print(ca, GNUTLS_CRT_PRINT_ONELINE, &tmp);
		if (debug)
			printf("\tCA Certificate: %.*s\n", tmp.size, tmp.data);
		gnutls_free(tmp.data);

		if (debug)
			printf("\tVerifying...");

		gnutls_x509_trust_list_init(&tl, 0);

		ret = gnutls_x509_trust_list_add_cas(tl, &ca, 1, 0);
		if (ret != 1) {
			fail("gnutls_x509_trust_list_add_trust_mem\n");
			exit(1);
		}

		data.count = 0;
		data.insert = chains[i].insert;

		gnutls_x509_trust_list_set_ptr(tl, &data);
		gnutls_x509_trust_list_set_getissuer_function(
			tl, getissuer_callback);

		ret = gnutls_x509_trust_list_verify_crt(tl, certs, j,
							chains[i].verify_flags,
							&verify_status, NULL);
		if (ret < 0) {
			fprintf(stderr,
				"gnutls_x509_trust_list_verify_crt: %s\n",
				gnutls_strerror(ret));
			exit(1);
		}

		if (verify_status != chains[i].expected_verify_result) {
			gnutls_datum_t out1, out2;
			gnutls_certificate_verification_status_print(
				verify_status, GNUTLS_CRT_X509, &out1, 0);
			gnutls_certificate_verification_status_print(
				chains[i].expected_verify_result,
				GNUTLS_CRT_X509, &out2, 0);
			fail("chain[%s]:\nverify_status: %d: %s\nexpected: %d: %s\n",
			     chains[i].name, verify_status, out1.data,
			     chains[i].expected_verify_result, out2.data);
			gnutls_free(out1.data);
			gnutls_free(out2.data);

		} else if (debug)
			printf("done\n");

		if (debug)
			printf("\tCleanup...");

		gnutls_x509_trust_list_deinit(tl, 0);
		gnutls_x509_crt_deinit(ca);
		for (j = 0; chains[i].chain[j]; j++)
			gnutls_x509_crt_deinit(certs[j]);

		if (debug)
			printf("done\n\n\n");
	}

	gnutls_global_deinit();

	if (debug)
		printf("Exit status...%d\n", exit_val);

	exit(exit_val);
}
