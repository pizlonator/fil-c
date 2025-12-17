/*
 * Copyright (C) 2023 Red Hat, Inc.
 *
 * Author: Daiki Ueno
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

int main(void)
{
	exit(77);
}

#else

#include <string.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <assert.h>

#include "cert-common.h"
#include "pkcs11/softhsm.h"
#include "utils.h"

/* This program tests that CKA_NSS_SERVER_DISTRUST_AFTER is honored
 * while validating certificate chain.
 */

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

#define PIN "1234"

#define CONFIG_NAME "softhsm-distrust-after"
#define CONFIG CONFIG_NAME ".config"

static const unsigned char chain_pem[] =
	"-----BEGIN CERTIFICATE-----"
	"MIID5zCCAp+gAwIBAgIUIXzLE8ObVwBGHepbjMWRwW/NpDgwDQYJKoZIhvcNAQEL"
	"BQAwGTEXMBUGA1UEAxMOR251VExTIHRlc3QgQ0EwIBcNMjMwMzE0MTAwNDAzWhgP"
	"OTk5OTEyMzEyMzU5NTlaMDcxGzAZBgNVBAoTEkdudVRMUyB0ZXN0IHNlcnZlcjEY"
	"MBYGA1UEAxMPdGVzdC5nbnV0bHMub3JnMIIBUjANBgkqhkiG9w0BAQEFAAOCAT8A"
	"MIIBOgKCATEAtGsnmCWvwf8eyrB+9Ni87UOGZ1Rd2rQewpBfgzwCEfwTcoWyiKRl"
	"QQt2XyO+ip/+eUtzOy7HSzy/FsmXVTUX86FySzDC4CeUEvNWAObOgksRXaQem/r6"
	"uRsqTRi1uqXmDMeoqKFtqoiE3JYOsmwcNarnx5Q9+dXHwqINS7NuevcIX8UJzRWT"
	"GveY3ypMZokk7R/QFmOBZaVYO6HNJWKbmYFUCBcY7HwvCKI7KFcynRdHCob7YrFB"
	"meb73qjqIH7zG+666pohZCmS8q1z5RkFnTdT4hGfGF8iuuKLDQCMni+nhz1Avkqi"
	"pZIIDC5hwFh8mpnh1qyDOSXPPhvt66NtncvFON7Bx26bNBS+MD6CkB65Spp25O8z"
	"DEaiMXL2w2EL+KpnifSl5XY3oSmfgHmqdQIDAQABo4GmMIGjMAwGA1UdEwEB/wQC"
	"MAAwGgYDVR0RBBMwEYIPdGVzdC5nbnV0bHMub3JnMCcGA1UdJQQgMB4GCCsGAQUF"
	"BwMBBggrBgEFBQcDAwYIKwYBBQUHAwQwDgYDVR0PAQH/BAQDAgWgMB0GA1UdDgQW"
	"BBRIIzRTCokxOEpa6sq20qbezh0rGDAfBgNVHSMEGDAWgBQedyNtZzEfkQebli/s"
	"/MhG/ozhAzANBgkqhkiG9w0BAQsFAAOCATEAYbQLlr74D62lPEevV/HWLOMG8taY"
	"gPld7Z5VApIhsJa913Jya7AOsW+lz48LX3QNTc8Xgj7FVwQeNP1GtBZXCe6U73KB"
	"Z+qp1rIEwn2cQVmFG+ShxmUA/gxxmWql2BAORNd5ZCVOcZbMh9uwWjhIQN/SImtW"
	"x3ebFgV5N7GPFbw+5NUITLXoLrD7Bixv3iQS8hWwmAmmPZbHAENRauL6jYSjniru"
	"SSFYjzJ1trJB6VgpJ2yWfKdcGZmB3osnGshWbayVOaprbH0AWKwOZ/d7sAldjdVw"
	"ZsaOhA+6NbvpKYZuw6Tdt0+VmUwGC1ATJGpc0dEXRBaFlt/e+gqQ43Mo+YwiMDYq"
	"LDU5nLC6uTSZLtgQHTqb32xmQ/D/y6NkUTH3f4OcxPGxBRVBHjOTk6MhRA=="
	"-----END CERTIFICATE-----"
	"-----BEGIN CERTIFICATE-----"
	"MIIDjTCCAkWgAwIBAgIUejTcfGbOAc9l4IBW+kpAN6A7Sj4wDQYJKoZIhvcNAQEL"
	"BQAwGTEXMBUGA1UEAxMOR251VExTIHRlc3QgQ0EwIBcNMjMwMzE0MDk1NzU1WhgP"
	"OTk5OTEyMzEyMzU5NTlaMBkxFzAVBgNVBAMTDkdudVRMUyB0ZXN0IENBMIIBUjAN"
	"BgkqhkiG9w0BAQEFAAOCAT8AMIIBOgKCATEAnORCsX1unl//fy2d1054XduIg/3C"
	"qVBaT3Hca65SEoDwh0KiPtQoOgZLdKY2cobGs/ojYtOjcs0KnlPYdmtjEh6WEhuJ"
	"U95v4TQdC4OLMiE56eIGq252hZAbHoTL84Q14DxQWGuzQK830iml7fbw2WcIcRQ8"
	"vFGs8SzfXw63+MI6Fq6iMAQIqP08WzGmRRzL5wvCiPhCVkrPmwbXoABub6AAsYwW"
	"PJB91M9/lx5gFH5k9/iPfi3s2Kg3F8MOcppqFYjxDSnsfiz6eMh1+bYVIAo367vG"
	"VYHigXMEZC2FezlwIHaZzpEoFlY3a7LFJ00yrjQ910r8UE+CEMTYzE40D0olCMo7"
	"FA9RCjeO3bUIoYaIdVTUGWEGHWSeoxGei9Gkm6u+ASj8f+i0jxdD2qXsewIDAQAB"
	"o2swaTAPBgNVHRMBAf8EBTADAQH/MCcGA1UdJQQgMB4GCCsGAQUFBwMBBggrBgEF"
	"BQcDAwYIKwYBBQUHAwQwDgYDVR0PAQH/BAQDAgIEMB0GA1UdDgQWBBQedyNtZzEf"
	"kQebli/s/MhG/ozhAzANBgkqhkiG9w0BAQsFAAOCATEAa37UdOTvdUfRGwjrodhE"
	"tEnRnfrwfQ61RMK5GY07UAks7CjdeWFDLoQfv9oP9kH122hEGAA683xg/CH5OeN0"
	"8zrayQKqwcH40SJQDzc748lTgxUIDaf2rrkoF8butpaDaI0fageqjlEvCeZZSuIC"
	"KCfZK9NPN47DknuerjOTwrWxvXYRepfSo8VVbjRj8R4qsgJsmJZYQfrAg0XrnKf/"
	"UibNPXRCYABsxH4ZFtivg93LaQ05z4IrPSWGOTDQxNBoEC0DVGfSc8XElP0MkF/K"
	"BIPsl3Rt2oFNhfViF9Gpzy9Dj1P1kMD6kE7nBDiRBUPNJZBiJSGVTMZTMc2tg42W"
	"QcUYnUUzOpQWg1tcOZy4s+EuJ0bEWhSkFfSN3ENxsHXNCYYHgeadATcGbzTxD6ib"
	"eA=="
	"-----END CERTIFICATE-----";

static const gnutls_datum_t chain = { (unsigned char *)chain_pem,
				      sizeof(chain_pem) - 1 };

static int pin_func(void *userdata, int attempt, const char *url,
		    const char *label, unsigned flags, char *pin,
		    size_t pin_max)
{
	if (attempt == 0) {
		strcpy(pin, PIN);
		return 0;
	}
	return -1;
}

static void test(const char *provider, const char *purpose, bool succeeds)
{
	int ret;
	gnutls_x509_crt_t *certs;
	unsigned int count;
	gnutls_x509_trust_list_t tl;
	gnutls_typed_vdata_st vdata;
	unsigned int status;

	gnutls_pkcs11_init(GNUTLS_PKCS11_FLAG_MANUAL, NULL);

	success("test with %s for %s\n", provider, purpose);

	if (debug) {
		gnutls_global_set_log_function(tls_log_func);
		gnutls_global_set_log_level(4711);
	}

	/* point to SoftHSM token that libpkcs11mock3.so internally uses */
	setenv(SOFTHSM_ENV, CONFIG, 1);

	gnutls_pkcs11_set_pin_function(pin_func, NULL);

	ret = gnutls_pkcs11_add_provider(provider, "trusted");
	if (ret != 0) {
		fail("gnutls_pkcs11_add_provider: %s\n", gnutls_strerror(ret));
	}

	/* initialize softhsm token */
	ret = gnutls_pkcs11_token_init(SOFTHSM_URL, PIN, "test");
	if (ret < 0) {
		fail("gnutls_pkcs11_token_init: %s\n", gnutls_strerror(ret));
	}

	ret = gnutls_pkcs11_token_set_pin(SOFTHSM_URL, NULL, PIN,
					  GNUTLS_PIN_USER);
	if (ret < 0) {
		fail("gnutls_pkcs11_token_set_pin: %s\n", gnutls_strerror(ret));
	}

	gnutls_x509_trust_list_init(&tl, 0);

	ret = gnutls_x509_trust_list_add_trust_file(tl, SOFTHSM_URL, NULL, 0, 0,
						    0);
	if (ret < 0) {
		fail("gnutls_x509_trust_list_add_trust_file\n");
	}

	ret = gnutls_x509_crt_list_import2(&certs, &count, &chain,
					   GNUTLS_X509_FMT_PEM, 0);
	if (ret < 0) {
		fail("gnutls_x509_crt_import: %s\n", gnutls_strerror(ret));
	}

	assert(count == 2);

	/* Use the ICA (instead of the actual root CA) for simplicity.  */
	ret = gnutls_pkcs11_copy_x509_crt(
		SOFTHSM_URL, certs[1], "ca",
		GNUTLS_PKCS11_OBJ_FLAG_MARK_TRUSTED |
			GNUTLS_PKCS11_OBJ_FLAG_MARK_CA |
			GNUTLS_PKCS11_OBJ_FLAG_LOGIN_SO);
	if (ret < 0) {
		fail("gnutls_pkcs11_copy_x509_crt: %s\n", gnutls_strerror(ret));
	}

	vdata.type = GNUTLS_DT_KEY_PURPOSE_OID;
	vdata.data = (void *)purpose;

	ret = gnutls_x509_trust_list_verify_crt2(tl, certs, 1, &vdata, 1, 0,
						 &status, NULL);
	if (ret < 0) {
		fail("gnutls_x509_trust_list_verify_crt2: %s\n",
		     gnutls_strerror(ret));
	}

	if (succeeds) {
		if (status != 0) {
			fail("verify failed\n");
		}
	} else if (!(status & GNUTLS_CERT_SIGNER_NOT_FOUND)) {
		fail("verify succeeded unexpectedly\n");
	}

	gnutls_x509_trust_list_deinit(tl, 0);
	while (count--) {
		gnutls_x509_crt_deinit(certs[count]);
	}
	gnutls_free(certs);

	gnutls_pkcs11_deinit();
}

void doit(void)
{
	const char *bin;
	const char *lib;
	char buf[128];

	if (gnutls_fips140_mode_enabled())
		exit(77);

	/* this must be called once in the program */
	global_init();

	/* we call gnutls_pkcs11_init manually */
	gnutls_pkcs11_deinit();

	/* check if softhsm module is loadable */
	lib = softhsm_lib();

	/* initialize SoftHSM token that libpkcs11mock2.so internally uses */
	bin = softhsm_bin();

	set_softhsm_conf(CONFIG);
	snprintf(buf, sizeof(buf),
		 "%s --init-token --slot 0 --label test --so-pin " PIN
		 " --pin " PIN,
		 bin);
	system(buf);

	test(lib, GNUTLS_KP_TLS_WWW_SERVER, true);

	set_softhsm_conf(CONFIG);
	snprintf(buf, sizeof(buf),
		 "%s --init-token --slot 0 --label test --so-pin " PIN
		 " --pin " PIN,
		 bin);
	system(buf);

	test(lib, GNUTLS_KP_EMAIL_PROTECTION, true);

	lib = getenv("P11MOCKLIB3");
	if (lib == NULL) {
		fail("P11MOCKLIB3 is not set\n");
	}

	set_softhsm_conf(CONFIG);
	snprintf(buf, sizeof(buf),
		 "%s --init-token --slot 0 --label test --so-pin " PIN
		 " --pin " PIN,
		 bin);
	system(buf);

	test(lib, GNUTLS_KP_TLS_WWW_SERVER, false);

	set_softhsm_conf(CONFIG);
	snprintf(buf, sizeof(buf),
		 "%s --init-token --slot 0 --label test --so-pin " PIN
		 " --pin " PIN,
		 bin);
	system(buf);

	test(lib, GNUTLS_KP_EMAIL_PROTECTION, true);
}
#endif /* _WIN32 */
