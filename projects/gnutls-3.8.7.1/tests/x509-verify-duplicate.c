/*
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * Author: Zoltan Fridrich
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS. If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnutls/x509.h>

#include "utils.h"

#define CHECK(X)                                            \
	{                                                   \
		r = X;                                      \
		if (r < 0)                                  \
			fail("error in %d: %s\n", __LINE__, \
			     gnutls_strerror(r));           \
	}

static char cert_pem[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIFLzCCBBegAwIBAgISAycvItcPAZ5yClzMOYYcod4cMA0GCSqGSIb3DQEBCwUA\n"
	"MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD\n"
	"EwJSMzAeFw0yMjA4MjMwNjMzMjlaFw0yMjExMjEwNjMzMjhaMBcxFTATBgNVBAMT\n"
	"DHZvaWRwb2ludC5pbzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANSt\n"
	"AazUWttuU/swyEdt70bpod6knYDJavnFUwicpT4ZfPh84Y2ci9Ay9oTVR8LzVq+o\n"
	"3FIGxXlBFhCtoGA5k5Soao/JB40+gsY+O8LgcNAdejU78m5W4e2qXq4eu/4tFUCw\n"
	"GkcRmqitnc5Jy0bEM+wCZKa42Lx0+WAhNRd/70yWIbzXOrXDnLgGc221JeYJ4it0\n"
	"ajYcf3AZuSHhL3qsTLLzuYorPqWmDy27psUiDDJOIjxVbBCRL+AY40TsQm7CZZhZ\n"
	"8sCkZU7rIvuDv7nf3QpUsF9Zqk9B3F4tTg0vsVuYeL1XCHGwpVeUS83MsZiLP8Zj\n"
	"XGQTM6GiWuOAZ9JJjrsCAwEAAaOCAlgwggJUMA4GA1UdDwEB/wQEAwIFoDAdBgNV\n"
	"HSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDAYDVR0TAQH/BAIwADAdBgNVHQ4E\n"
	"FgQUlw1h3ZwSMKRwkrQ+F4XT3QV/tn8wHwYDVR0jBBgwFoAUFC6zF7dYVsuuUAlA\n"
	"5h+vnYsUwsYwVQYIKwYBBQUHAQEESTBHMCEGCCsGAQUFBzABhhVodHRwOi8vcjMu\n"
	"by5sZW5jci5vcmcwIgYIKwYBBQUHMAKGFmh0dHA6Ly9yMy5pLmxlbmNyLm9yZy8w\n"
	"JwYDVR0RBCAwHoIOKi52b2lkcG9pbnQuaW+CDHZvaWRwb2ludC5pbzBMBgNVHSAE\n"
	"RTBDMAgGBmeBDAECATA3BgsrBgEEAYLfEwEBATAoMCYGCCsGAQUFBwIBFhpodHRw\n"
	"Oi8vY3BzLmxldHNlbmNyeXB0Lm9yZzCCAQUGCisGAQQB1nkCBAIEgfYEgfMA8QB2\n"
	"AN+lXqtogk8fbK3uuF9OPlrqzaISpGpejjsSwCBEXCpzAAABgsme4hAAAAQDAEcw\n"
	"RQIhAP6sPHv1PJez/VRMw5xmAAkNU/q9ydq1mTgp7j5uBB9AAiAxm+teG9utZCLP\n"
	"TTTv89FHwFV9omfZzDNAiNgg8METHwB3ACl5vvCeOTkh8FZzn2Old+W+V32cYAr4\n"
	"+U1dJlwlXceEAAABgsme4gUAAAQDAEgwRgIhAPKWJ7WeuBUSnDqabTAVLKU+PpzA\n"
	"bJJ9sehaCKW9AicZAiEAqphpC0lF4/iz2Gkxgd/DEkl9SyyAmR/lEJ7cWDMFhz8w\n"
	"DQYJKoZIhvcNAQELBQADggEBAC0aCscObAdTerzGUrDsuQR5FuCTAmvdk3Isqjw1\n"
	"dG3WuiwW1Z4ecpqCdvDSIv3toQDWVk6g/oa3fHDnY0/tu//vCwdneDdjK3gCM6cj\n"
	"/q0cwj+rGFx/bEVz8PR5kc3DOHGKkmHPN1BNxeLBVpk4jxziXryAVbIvxq9JrGTE\n"
	"SfWbWcMkHHw/QzpUfyD3B/GI8qw6XhdaNNkLDEDNV0sCPCuZYc5FBZzU4ExB2vMG\n"
	"QVnPfxzKWmxHs10uxXyRZJlOrrbTGU8gi0vnOQZK290dtLzEyU2sdkic1ZSn+fCo\n"
	"k++37mNDkiTnIQa3olRqHkypWqGfj8OyqU4XBV2Mmu4UATc=\n"
	"-----END CERTIFICATE-----\n"
	"-----BEGIN CERTIFICATE-----\n"
	"MIIFLzCCBBegAwIBAgISAycvItcPAZ5yClzMOYYcod4cMA0GCSqGSIb3DQEBCwUA\n"
	"MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD\n"
	"EwJSMzAeFw0yMjA4MjMwNjMzMjlaFw0yMjExMjEwNjMzMjhaMBcxFTATBgNVBAMT\n"
	"DHZvaWRwb2ludC5pbzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANSt\n"
	"AazUWttuU/swyEdt70bpod6knYDJavnFUwicpT4ZfPh84Y2ci9Ay9oTVR8LzVq+o\n"
	"3FIGxXlBFhCtoGA5k5Soao/JB40+gsY+O8LgcNAdejU78m5W4e2qXq4eu/4tFUCw\n"
	"GkcRmqitnc5Jy0bEM+wCZKa42Lx0+WAhNRd/70yWIbzXOrXDnLgGc221JeYJ4it0\n"
	"ajYcf3AZuSHhL3qsTLLzuYorPqWmDy27psUiDDJOIjxVbBCRL+AY40TsQm7CZZhZ\n"
	"8sCkZU7rIvuDv7nf3QpUsF9Zqk9B3F4tTg0vsVuYeL1XCHGwpVeUS83MsZiLP8Zj\n"
	"XGQTM6GiWuOAZ9JJjrsCAwEAAaOCAlgwggJUMA4GA1UdDwEB/wQEAwIFoDAdBgNV\n"
	"HSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDAYDVR0TAQH/BAIwADAdBgNVHQ4E\n"
	"FgQUlw1h3ZwSMKRwkrQ+F4XT3QV/tn8wHwYDVR0jBBgwFoAUFC6zF7dYVsuuUAlA\n"
	"5h+vnYsUwsYwVQYIKwYBBQUHAQEESTBHMCEGCCsGAQUFBzABhhVodHRwOi8vcjMu\n"
	"by5sZW5jci5vcmcwIgYIKwYBBQUHMAKGFmh0dHA6Ly9yMy5pLmxlbmNyLm9yZy8w\n"
	"JwYDVR0RBCAwHoIOKi52b2lkcG9pbnQuaW+CDHZvaWRwb2ludC5pbzBMBgNVHSAE\n"
	"RTBDMAgGBmeBDAECATA3BgsrBgEEAYLfEwEBATAoMCYGCCsGAQUFBwIBFhpodHRw\n"
	"Oi8vY3BzLmxldHNlbmNyeXB0Lm9yZzCCAQUGCisGAQQB1nkCBAIEgfYEgfMA8QB2\n"
	"AN+lXqtogk8fbK3uuF9OPlrqzaISpGpejjsSwCBEXCpzAAABgsme4hAAAAQDAEcw\n"
	"RQIhAP6sPHv1PJez/VRMw5xmAAkNU/q9ydq1mTgp7j5uBB9AAiAxm+teG9utZCLP\n"
	"TTTv89FHwFV9omfZzDNAiNgg8METHwB3ACl5vvCeOTkh8FZzn2Old+W+V32cYAr4\n"
	"+U1dJlwlXceEAAABgsme4gUAAAQDAEgwRgIhAPKWJ7WeuBUSnDqabTAVLKU+PpzA\n"
	"bJJ9sehaCKW9AicZAiEAqphpC0lF4/iz2Gkxgd/DEkl9SyyAmR/lEJ7cWDMFhz8w\n"
	"DQYJKoZIhvcNAQELBQADggEBAC0aCscObAdTerzGUrDsuQR5FuCTAmvdk3Isqjw1\n"
	"dG3WuiwW1Z4ecpqCdvDSIv3toQDWVk6g/oa3fHDnY0/tu//vCwdneDdjK3gCM6cj\n"
	"/q0cwj+rGFx/bEVz8PR5kc3DOHGKkmHPN1BNxeLBVpk4jxziXryAVbIvxq9JrGTE\n"
	"SfWbWcMkHHw/QzpUfyD3B/GI8qw6XhdaNNkLDEDNV0sCPCuZYc5FBZzU4ExB2vMG\n"
	"QVnPfxzKWmxHs10uxXyRZJlOrrbTGU8gi0vnOQZK290dtLzEyU2sdkic1ZSn+fCo\n"
	"k++37mNDkiTnIQa3olRqHkypWqGfj8OyqU4XBV2Mmu4UATc=\n"
	"-----END CERTIFICATE-----\n"
	"-----BEGIN CERTIFICATE-----\n"
	"MIIFFjCCAv6gAwIBAgIRAJErCErPDBinU/bWLiWnX1owDQYJKoZIhvcNAQELBQAw\n"
	"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
	"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjAwOTA0MDAwMDAw\n"
	"WhcNMjUwOTE1MTYwMDAwWjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n"
	"RW5jcnlwdDELMAkGA1UEAxMCUjMwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n"
	"AoIBAQC7AhUozPaglNMPEuyNVZLD+ILxmaZ6QoinXSaqtSu5xUyxr45r+XXIo9cP\n"
	"R5QUVTVXjJ6oojkZ9YI8QqlObvU7wy7bjcCwXPNZOOftz2nwWgsbvsCUJCWH+jdx\n"
	"sxPnHKzhm+/b5DtFUkWWqcFTzjTIUu61ru2P3mBw4qVUq7ZtDpelQDRrK9O8Zutm\n"
	"NHz6a4uPVymZ+DAXXbpyb/uBxa3Shlg9F8fnCbvxK/eG3MHacV3URuPMrSXBiLxg\n"
	"Z3Vms/EY96Jc5lP/Ooi2R6X/ExjqmAl3P51T+c8B5fWmcBcUr2Ok/5mzk53cU6cG\n"
	"/kiFHaFpriV1uxPMUgP17VGhi9sVAgMBAAGjggEIMIIBBDAOBgNVHQ8BAf8EBAMC\n"
	"AYYwHQYDVR0lBBYwFAYIKwYBBQUHAwIGCCsGAQUFBwMBMBIGA1UdEwEB/wQIMAYB\n"
	"Af8CAQAwHQYDVR0OBBYEFBQusxe3WFbLrlAJQOYfr52LFMLGMB8GA1UdIwQYMBaA\n"
	"FHm0WeZ7tuXkAXOACIjIGlj26ZtuMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcw\n"
	"AoYWaHR0cDovL3gxLmkubGVuY3Iub3JnLzAnBgNVHR8EIDAeMBygGqAYhhZodHRw\n"
	"Oi8veDEuYy5sZW5jci5vcmcvMCIGA1UdIAQbMBkwCAYGZ4EMAQIBMA0GCysGAQQB\n"
	"gt8TAQEBMA0GCSqGSIb3DQEBCwUAA4ICAQCFyk5HPqP3hUSFvNVneLKYY611TR6W\n"
	"PTNlclQtgaDqw+34IL9fzLdwALduO/ZelN7kIJ+m74uyA+eitRY8kc607TkC53wl\n"
	"ikfmZW4/RvTZ8M6UK+5UzhK8jCdLuMGYL6KvzXGRSgi3yLgjewQtCPkIVz6D2QQz\n"
	"CkcheAmCJ8MqyJu5zlzyZMjAvnnAT45tRAxekrsu94sQ4egdRCnbWSDtY7kh+BIm\n"
	"lJNXoB1lBMEKIq4QDUOXoRgffuDghje1WrG9ML+Hbisq/yFOGwXD9RiX8F6sw6W4\n"
	"avAuvDszue5L3sz85K+EC4Y/wFVDNvZo4TYXao6Z0f+lQKc0t8DQYzk1OXVu8rp2\n"
	"yJMC6alLbBfODALZvYH7n7do1AZls4I9d1P4jnkDrQoxB3UqQ9hVl3LEKQ73xF1O\n"
	"yK5GhDDX8oVfGKF5u+decIsH4YaTw7mP3GFxJSqv3+0lUFJoi5Lc5da149p90Ids\n"
	"hCExroL1+7mryIkXPeFM5TgO9r0rvZaBFOvV2z0gp35Z0+L4WPlbuEjN/lxPFin+\n"
	"HlUjr8gRsI3qfJOQFy/9rKIJR0Y/8Omwt/8oTWgy1mdeHmmjk7j1nYsvC9JSQ6Zv\n"
	"MldlTTKB3zhThV1+XWYp6rjd5JW1zbVWEkLNxE7GJThEUG3szgBVGP7pSWTUTsqX\n"
	"nLRbwHOoq7hHwg==\n"
	"-----END CERTIFICATE-----\n"
	"-----BEGIN CERTIFICATE-----\n"
	"MIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/\n"
	"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
	"DkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1ow\n"
	"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
	"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEB\n"
	"AQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XC\n"
	"ov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpL\n"
	"wYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+D\n"
	"LtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK\n"
	"4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5\n"
	"bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5y\n"
	"sR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZ\n"
	"Xmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4\n"
	"FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBc\n"
	"SLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2ql\n"
	"PRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TND\n"
	"TwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYw\n"
	"SwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1\n"
	"c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx\n"
	"+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEB\n"
	"ATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQu\n"
	"b3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9E\n"
	"U1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26Ztu\n"
	"MA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC\n"
	"5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW\n"
	"9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuG\n"
	"WCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9O\n"
	"he8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFC\n"
	"Dfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5\n"
	"-----END CERTIFICATE-----\n";

void doit(void)
{
	int r;
	unsigned i, certs_size, out;
	unsigned flags = GNUTLS_VERIFY_DO_NOT_ALLOW_SAME |
			 GNUTLS_VERIFY_DISABLE_TIME_CHECKS;
	gnutls_x509_trust_list_t tl;
	gnutls_x509_crt_t *certs = NULL;
	gnutls_datum_t cert = { (unsigned char *)cert_pem,
				sizeof(cert_pem) - 1 };

	global_init();

	CHECK(gnutls_x509_crt_list_import2(&certs, &certs_size, &cert,
					   GNUTLS_X509_FMT_PEM, 0));
	CHECK(gnutls_x509_trust_list_init(&tl, 0));
	CHECK(gnutls_x509_trust_list_add_cas(tl, certs + certs_size - 1, 1, 0));
	CHECK(gnutls_x509_trust_list_verify_crt(tl, certs, certs_size, flags,
						&out, NULL));

	if (out)
		fail("Not verified\n");

	gnutls_x509_trust_list_deinit(tl, 0);
	for (i = 0; i < certs_size; ++i)
		gnutls_x509_crt_deinit(certs[i]);
	gnutls_free(certs);

	gnutls_global_deinit();
}
