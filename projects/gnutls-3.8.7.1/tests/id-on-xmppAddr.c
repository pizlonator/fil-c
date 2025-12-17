/*
 * Copyright (C) 2021 Steffen Jaeckel
 *
 * Author: Steffen Jaeckel
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

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <stdlib.h>
#include <limits.h>

#include "utils.h"

#define should_succeed(f)                                       \
	do {                                                    \
		int ret##__LINE__ = (f);                        \
		if (ret##__LINE__ < 0) {                        \
			fail(#f " failed %d\n", ret##__LINE__); \
		}                                               \
	} while (0)

void doit(void)
{
	int ret;
	gnutls_x509_crt_t cert;
	gnutls_datum_t data;
	size_t name_len = 128;
	char name[128];
	char path[256];
	const char *src;
	const char *id_on_xmppAddr =
		"very.long.username@so.the.asn1.length.is.a.valid.ascii.character";

	src = getenv("srcdir");
	if (src == NULL)
		src = ".";

	snprintf(path, sizeof(path), "%s/%s", src, "certs/id-on-xmppAddr.pem");

	ret = global_init();
	if (ret < 0)
		fail("init %d\n", ret);

	should_succeed(gnutls_x509_crt_init(&cert));
	should_succeed(gnutls_load_file(path, &data));
	should_succeed(
		gnutls_x509_crt_import(cert, &data, GNUTLS_X509_FMT_PEM));
	ret = gnutls_x509_crt_get_subject_alt_name(cert, 0, name, &name_len,
						   NULL);
	if (ret != GNUTLS_SAN_OTHERNAME_XMPP)
		fail("did not recognize GNUTLS_SAN_OTHERNAME_XMPP");

	if (strcmp(name, id_on_xmppAddr) != 0)
		fail("xmppAddr not decoded correctly: %s", name);

	gnutls_free(data.data);
	gnutls_x509_crt_deinit(cert);
	gnutls_global_deinit();
}
