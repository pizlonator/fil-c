/*
 * Copyright (C) 2022 Brian Wickman
 *
 * Author: Brian Wickman
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

#include "utils.h"
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/x509-ext.h>

/* Test that UPN OTHERNAME constraints in a CA certificate 
 * are parsed correctly
 *
 * Test that a leaf certificate with a DNSName validates correctly
 * when issued by a CA with UPN OTHERNAME constraints (Issue 1132)
 * (UPN == User Principal Name - used in Active Directory
 * environments using smartcards for authentication)
 */

void verify_upn_constraints(gnutls_x509_name_constraints_t);
void verify_non_upn_leaf(gnutls_x509_name_constraints_t);

static const char _domaincontroller[] = {
	"-----BEGIN CERTIFICATE-----\n"
	"MIIEqTCCA5GgAwIBAgITQAAAAAPX0eQxgcZpHAAAAAAAAzANBgkqhkiG9w0BAQsF\n"
	"ADA0MRUwEwYDVQQKEwxFeGFtcGxlIEluYy4xGzAZBgNVBAMTEkV4YW1wbGUgQ29y\n"
	"cCBBRCBDQTAeFw0yMjA0MTIxNjUzMTFaFw0yNzA0MTExNjUzMTFaMCIxIDAeBgNV\n"
	"BAMTF2V4YW1wbGVkYzAxLmV4YW1wbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOC\n"
	"AQ8AMIIBCgKCAQEAtnYFOqZas9U9GX87w2bvyQh6l3fWJ83JHEHAwP11j9dQu/sa\n"
	"qgMYr/OqH+5tCvsDLt9sI35RCuF+6San3P1m56G+iYaawE46UrbHSYC4PyhinOXx\n"
	"X3xXzaxjTDYhz46Fvfmoqa732zPYG3QQplPsjQbRx96iXOSkdWt8g4mbTJ/eyYdG\n"
	"uXt1mlvL+USz5b39trOgSgTC60cdneBrQsBh7o80rHvaprvjTY5mHS7JNHcsr9Hs\n"
	"xjOOq9t3LdWehXYshINZ6ChxaHipbBUF+0CTvwJW8wvQtSV6MYDl+cbS/47OwJG0\n"
	"OXJxFVQofJWNi4/IrTC42d3fyEWA2ZnP898GeQIDAQABo4IBxDCCAcAwPQYJKwYB\n"
	"BAGCNxUHBDAwLgYmKwYBBAGCNxUIg/iOToSq0GWEhZMhhZ3KIoKY1VocgufIbYTY\n"
	"+3sCAWQCAQIwMgYDVR0lBCswKQYHKwYBBQIDBQYKKwYBBAGCNxQCAgYIKwYBBQUH\n"
	"AwEGCCsGAQUFBwMCMA4GA1UdDwEB/wQEAwIFoDBABgkrBgEEAYI3FQoEMzAxMAkG\n"
	"BysGAQUCAwUwDAYKKwYBBAGCNxQCAjAKBggrBgEFBQcDATAKBggrBgEFBQcDAjAd\n"
	"BgNVHQ4EFgQUjaBu4CsVk5gng+ACWTSqsj1gmVQwNAYDVR0RBC0wK4IXZXhhbXBs\n"
	"ZWRjMDEuZXhhbXBsZS5jb22CEGxkYXAuZXhhbXBsZS5jb20wHwYDVR0jBBgwFoAU\n"
	"aRL34OyTRJUSVVfxMiMjBFHk/WowOwYDVR0fBDQwMjAwoC6gLIYqaHR0cDovL3Br\n"
	"aS5leGFtcGxlLmNvbS9jZHAvRXhhbXBsZUFEQ0EuY3JsMEYGCCsGAQUFBwEBBDow\n"
	"ODA2BggrBgEFBQcwAoYqaHR0cDovL3BraS5leGFtcGxlLmNvbS9haWEvRXhhbXBs\n"
	"ZUFEQ0EuY2VyMA0GCSqGSIb3DQEBCwUAA4IBAQCKr0WQYujcyUOUZp63i27dMihf\n"
	"z+WKd2G+dyGzmNTabFlZSfquFo+MWmSM04UOEYS45tyFZhWEXXaz4OfilelKy5XI\n"
	"tiZRGDvzNzxfb7GQSWDO1mxLHW2yEH+1Cyu/Km0PRhDl1Vy0DFyrdGh/w7qTM7eG\n"
	"BjD0bBtk9/M58IYlnzx7CM53CRGhPHUygontN1vbWf42gDdu+5d+tnls86gTzuRs\n"
	"su4BReayHU9aFqorWhvxCQhgnLx98Ei2BsJe5nbSzjVA5ZhPcL9WDC76aDPEDaZg\n"
	"GnNu9kZJV/UrCaulu0COhJfNocd/LWXZbUStUCenRX01GHCP+4mNmPLJkVh2\n"
	"-----END CERTIFICATE-----"
};

static const char _issuingca[] = {
	/* The intermediate CA with name constraints */
	"-----BEGIN CERTIFICATE-----\n"
	"MIIE0jCCA7qgAwIBAgITLgAAAAK9f34egj9VJAAAAAAAAjANBgkqhkiG9w0BAQsF\n"
	"ADA2MRUwEwYDVQQKEwxFeGFtcGxlIEluYy4xHTAbBgNVBAMTFEV4YW1wbGUgQ29y\n"
	"cCBSb290IENBMCAXDTIyMDQxMjE2Mzk0M1oYDzIwNjcwNDEyMTY0OTQzWjA0MRUw\n"
	"EwYDVQQKEwxFeGFtcGxlIEluYy4xGzAZBgNVBAMTEkV4YW1wbGUgQ29ycCBBRCBD\n"
	"QTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALs2TqehwJfMyrU77MRv\n"
	"4jgwgnsruZMexMGwT6A5oxdjKNhyXnsdiYiH3nFEgrHSCOAxgoCDJYlDLn0jZYdS\n"
	"3j7hMrhzAwHzwUgTrruHaTZ2tShxbfvUAGuuOroSVB4+XzS22RKdgh7g1cv3scWI\n"
	"62M2vfV8iBpehD5xhmqfu2Z9ChNTR32HLHdFdsMFuS+t0Zktszk1qE9AClFa7ttr\n"
	"VKgOyEmjgXlhX/Qld4zgCvxvI/jMPbEKrU2ZFeRV160vGaraAVjF0Oxe9TFH9fLZ\n"
	"E+ERghmfdzzbNOXikgExrsveALNRsbTyIhKmEDRGMN/y12htghHvBamwGDt/gj9q\n"
	"3fECAwEAAaOCAdcwggHTMBAGCSsGAQQBgjcVAQQDAgEAMB0GA1UdDgQWBBRpEvfg\n"
	"7JNElRJVV/EyIyMEUeT9ajAZBgkrBgEEAYI3FAIEDB4KAFMAdQBiAEMAQTALBgNV\n"
	"HQ8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBADAfBgNVHSMEGDAWgBQlQb5lkuye\n"
	"IfoJIi/ctatOBUANSDA7BgNVHR8ENDAyMDCgLqAshipodHRwOi8vcGtpLmV4YW1w\n"
	"bGUuY29tL2NkcC9FeGFtcGxlUm9vdC5jcmwwggEEBgNVHR4BAf8EgfkwgfagajAN\n"
	"ggtleGFtcGxlLmNvbTAOggwuZXhhbXBsZS5jb20wCYIHRVhBTVBMRTAeoBwGCisG\n"
	"AQQBgjcUAgOgDgwMQGV4YW1wbGUuY29tMB6gHAYKKwYBBAGCNxQCA6AODAwuZXhh\n"
	"bXBsZS5jb22hgYcwF4IVc3ViZG9tYWluLmV4YW1wbGUuY29tMBiCFi5zdWJkb21h\n"
	"aW4uZXhhbXBsZS5jb20wKKAmBgorBgEEAYI3FAIDoBgMFkBzdWJkb21haW4uZXhh\n"
	"bXBsZS5jb20wKKAmBgorBgEEAYI3FAIDoBgMFi5zdWJkb21haW4uZXhhbXBsZS5j\n"
	"b20wDQYJKoZIhvcNAQELBQADggEBAG+gD/ZNEaoukBt/U+7tGOwx5bTAdNChYZEU\n"
	"Wzt5XoJ0ZgClfgtKk/hmDxPsUEVOzaYEtUrj8V0qJun5YwEzZsZbHAkbkTOcQ2tC\n"
	"5Jv7czs0IYrSCJIgz7PdNSxTaXyCpipzUvSdZxQj3Bjj+MiYiReEwxhAb6bI/D8h\n"
	"HXk9T5iHiw9f7P6ZTBvx5keUjAePO8sc0CtefOIH+tyRY1oEHAzMSDzqhpeZDAtM\n"
	"N93KZkhnx/kmQhqLXhrck9Ubozw++e2iP83bTojTFSodRiKWPtUKOHAlPvIWQURc\n"
	"YP0dQUsv1tMnNjJgA7COp1+mmqfEUVQqmBwRbJ26ve2iwS/SAgI=\n"
	"-----END CERTIFICATE-----"
};

const unsigned char example3[] = "@example.com";
const unsigned char example4[] = ".example.com";
const unsigned char subdomain2[] = "@subdomain.example.com";
const unsigned char subdomain3[] = ".subdomain.example.com";

void verify_upn_constraints(gnutls_x509_name_constraints_t name_constraints)
{
	int ret = 0;
	unsigned int type = 0;
	gnutls_datum_t constraint = { NULL, 0 };
	ret = gnutls_x509_name_constraints_get_permitted(name_constraints, 3,
							 &type, &constraint);
	if (ret < 0) {
		fail("Error getting permitted constraint line %d: %s\n",
		     __LINE__, gnutls_strerror(ret));
		exit(1);
	}

	if (type != GNUTLS_SAN_OTHERNAME_MSUSERPRINCIPAL) {
		fail("Error permitted constraint 3 is not UPN line: %d Found: %u\n",
		     __LINE__, type);
		exit(1);
	}

	if ((constraint.size != sizeof(example3) - 1) ||
	    memcmp(constraint.data, example3, sizeof(example3) - 1) != 0) {
		fail("Error permitted constraint 3 was %s expected %s line: %d\n",
		     constraint.data, example3, __LINE__);
		exit(1);
	}

	ret = gnutls_x509_name_constraints_get_permitted(name_constraints, 4,
							 &type, &constraint);
	if (ret < 0) {
		fail("Error getting permitted constraint line %d: %s\n",
		     __LINE__, gnutls_strerror(ret));
		exit(1);
	}

	if (type != GNUTLS_SAN_OTHERNAME_MSUSERPRINCIPAL) {
		fail("Error permitted constraint 4 is not UPN line: %d Found: %u\n",
		     __LINE__, type);
		exit(1);
	}

	if ((constraint.size != sizeof(example4) - 1) ||
	    memcmp(constraint.data, example4, sizeof(example4) - 1) != 0) {
		fail("Error permitted constraint 4 was %s expected %s line: %d\n",
		     constraint.data, example4, __LINE__);
		exit(1);
	}

	ret = gnutls_x509_name_constraints_get_excluded(name_constraints, 2,
							&type, &constraint);
	if (ret < 0) {
		fail("Error getting excluded constraint line %d: %s\n",
		     __LINE__, gnutls_strerror(ret));
		exit(1);
	}

	if (type != GNUTLS_SAN_OTHERNAME_MSUSERPRINCIPAL) {
		fail("Error excluded constraint 2 is not UPN line: %d Found %u\n",
		     __LINE__, type);
		exit(1);
	}

	if ((constraint.size != sizeof(subdomain2) - 1) ||
	    memcmp(constraint.data, subdomain2, sizeof(subdomain2) - 1) != 0) {
		fail("Error excluded constraint 2 was %s expected %s line: %d\n",
		     constraint.data, subdomain2, __LINE__);
		exit(1);
	}

	ret = gnutls_x509_name_constraints_get_excluded(name_constraints, 3,
							&type, &constraint);
	if (ret < 0) {
		fail("Error getting excluded constraint line %d: %s\n",
		     __LINE__, gnutls_strerror(ret));
		exit(1);
	}

	if (type != GNUTLS_SAN_OTHERNAME_MSUSERPRINCIPAL) {
		fail("Error excluded constraint 3 is not UPN line: %d Found %u\n",
		     __LINE__, type);
		exit(1);
	}

	if ((constraint.size != sizeof(subdomain3) - 1) ||
	    memcmp(constraint.data, subdomain3, sizeof(subdomain3) - 1) != 0) {
		fail("Error excluded constraint 3 was %s expected %s line: %d\n",
		     constraint.data, subdomain3, __LINE__);
		exit(1);
	}
}

void verify_non_upn_leaf(gnutls_x509_name_constraints_t name_constraints)
{
	// This test specifically checks for resolution of issue 1132
	int ret = 0;
	gnutls_x509_crt_t domaincontroller;
	gnutls_datum_t domaincontroller_datum = {
		(void *)_domaincontroller, sizeof(_domaincontroller) - 1
	};

	gnutls_x509_crt_init(&domaincontroller);

	ret = gnutls_x509_crt_import(domaincontroller, &domaincontroller_datum,
				     GNUTLS_X509_FMT_PEM);
	if (ret < 0) {
		fail("Error importing domain controller cert line %d: %s\n",
		     __LINE__, gnutls_strerror(ret));
		exit(1);
	}

	ret = gnutls_x509_name_constraints_check_crt(
		name_constraints, GNUTLS_SAN_DNSNAME, domaincontroller);
	if (ret < 0) {
		fail("Error failed to verify leaf cert against constraints line: %d\n",
		     __LINE__);
		exit(1);
	}

	gnutls_x509_crt_deinit(domaincontroller);
}

void doit(void)
{
	int ret;
	unsigned int critical = 0;
	gnutls_x509_crt_t issuingca;
	gnutls_datum_t issuingca_datum = { (void *)_issuingca,
					   sizeof(_issuingca) - 1 };

	global_init();

	gnutls_x509_crt_init(&issuingca);

	ret = gnutls_x509_crt_import(issuingca, &issuingca_datum,
				     GNUTLS_X509_FMT_PEM);
	if (ret < 0) {
		fail("Error importing issuing CA line %d: %s\n", __LINE__,
		     gnutls_strerror(ret));
		exit(1);
	}

	gnutls_x509_name_constraints_t name_constraints = NULL;

	ret = gnutls_x509_name_constraints_init(&name_constraints);
	if (ret < 0) {
		fail("Error initializing constraints structure line %d: %s\n",
		     __LINE__, gnutls_strerror(ret));
		exit(1);
	}

	ret = gnutls_x509_crt_get_name_constraints(issuingca, name_constraints,
						   0, &critical);
	if (ret < 0) {
		// Failure here is potentially a regression to issue 1132 behavior
		fail("Error loading constraints line: %d\n", __LINE__);
		exit(1);
	}

	verify_upn_constraints(name_constraints);

	verify_non_upn_leaf(name_constraints);

	gnutls_x509_name_constraints_deinit(name_constraints);
	gnutls_x509_crt_deinit(issuingca);

	success("UPN constraints tests completed successfully\n");

	gnutls_global_deinit();
}

/* The following cert is the root CA that signed the intermediate CA used in
 * the tests up above. While it wasn't needed in these tests, it is included
 * here in case it becomes useful in the future:
 *
-----BEGIN CERTIFICATE-----
MIIDSTCCAjGgAwIBAgIQKpl3VjKWEKlMf9Nx+omsZDANBgkqhkiG9w0BAQsFADA2
MRUwEwYDVQQKEwxFeGFtcGxlIEluYy4xHTAbBgNVBAMTFEV4YW1wbGUgQ29ycCBS
b290IENBMCAXDTIyMDQxMjE1NTkyMloYDzIwNzIwNDEyMTYwOTIyWjA2MRUwEwYD
VQQKEwxFeGFtcGxlIEluYy4xHTAbBgNVBAMTFEV4YW1wbGUgQ29ycCBSb290IENB
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA+Stt+MibOCQO69wtINEM
9cdxjQyk2WMKzZflAOmi5lFNUmNsxGBQGcVk8dlyUwiWGUlGshUvBBcEGgnxKnO+
S465gyS6fcFfrpX7B/CQWv2/m0n7rdJ65TnB2vRct2Ni/6AjgZLSJXxwjXiuH72Z
k37vpxY4B6mOCe2XjUu2J8DhG9K4FatzeqsUgpvbiXdO/hD1oWQbFRVOeHAdipBC
+KH6qOL4g7V3V1gW99DuR/ZyJqU9uRrBe8CyP1PxcSUySfFx9hhTB5hSufiCDuR3
KRKTyaXZ/1l0e2MY3wKig/PujBhYdLTLoErnYN6ccP98jBZIHMacE43e4WUCI2Ld
WQIDAQABo1EwTzALBgNVHQ8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQUJUG+ZZLsniH6CSIv3LWrTgVADUgwEAYJKwYBBAGCNxUBBAMCAQAwDQYJKoZI
hvcNAQELBQADggEBAGg86wjyMMGKZ0VSLko7AdNTqmD5jkamol7zPFbjuX8Cc3IQ
KyQkhkrA3v6bDbau8axLeqs40TLO12f3LNRGMcVNN1I8SbCEN3IvX6W0wLkiVxvV
+lVKCuCb2JedmXjOHHkkm4xhlsCZipA3Pz3cOXeIt2DLnoY7G6i7N5cZoAXgxQ3V
jjnsUINFWuwBDbjmLA+H9eGIyAQSXkWPBLI6K7jOV8V3FLv1ACkW3K9agJCcx2uO
kdBFhRm4kl2U5HB/qOZ685ouNQj6kz9xgykOxiabgellz846uUIfMBxsQaoU1dAX
vO7vJHxoQOJiTc9u+eOSFe+eFIeLlCHLz6k59tE=
-----END CERTIFICATE-----
*/
