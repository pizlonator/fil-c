/*
 * Copyright (C) 2020 Free Software Foundation, Inc.
 *
 * Author: Ander Juaristi
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

/* Parts copied from other tests */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/x509-ext.h>

#include "utils.h"

static char pem[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIG+jCCBeKgAwIBAgIQPurql+NcbKQ/8rR9djN5DDANBgkqhkiG9w0BAQsFADCB\n"
	"hDELMAkGA1UEBhMCVVMxHTAbBgNVBAoTFFN5bWFudGVjIENvcnBvcmF0aW9uMR8w\n"
	"HQYDVQQLExZTeW1hbnRlYyBUcnVzdCBOZXR3b3JrMTUwMwYDVQQDEyxTeW1hbnRl\n"
	"YyBDbGFzcyAzIFNlY3VyZSBTZXJ2ZXIgU0hBMjU2IFNTTCBDQTAeFw0xNzAzMjAw\n"
	"MDAwMDBaFw0yMDAzMjQyMzU5NTlaMHMxCzAJBgNVBAYTAlVTMREwDwYDVQQIDAhO\n"
	"ZXcgWW9yazERMA8GA1UEBwwITmV3IFlvcmsxJzAlBgNVBAoMHkFtZXJpY2FuIENp\n"
	"dmlsIExpYmVydGllcyBVbmlvbjEVMBMGA1UEAwwMd3d3LmFjbHUub3JnMIIBIjAN\n"
	"BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAui9XzdLmI2JH+05y4WAV7jHn2Vyk\n"
	"k+92pm/mdQcfJDyNR6gotaLBvBy9n9SeDj03eGlYUKZ1lgBeHhM17FMWuWoazETl\n"
	"EU2Iq1ugHn3V+Rr2IkQ8f00RcNXRlYCQOiL0WYrrXPHZUNh1aQ4kwFaFGT0iNsKS\n"
	"kGwf56b1goJujqwtLIBzRdHOLzWGCq1Kn/VeDTi2QQyTVQLWsDZzZApUXMyoc1xv\n"
	"go7r1lvHWbJ04up0YwXssC67lw4SKK+/2lZF0Fu0baooHQOlQ5jk0DQhQ6Hsgp/t\n"
	"UYhrv56cVf9MWrBEbVBg79yiyWb+rrXhk9KeMbFFsxNEWiA5TREejEhVXwIDAQAB\n"
	"o4IDdjCCA3IwMgYDVR0RBCswKYIPYWN0aW9uLmFjbHUub3Jnggx3d3cuYWNsdS5v\n"
	"cmeCCGFjbHUub3JnMAkGA1UdEwQCMAAwDgYDVR0PAQH/BAQDAgWgMGEGA1UdIARa\n"
	"MFgwVgYGZ4EMAQICMEwwIwYIKwYBBQUHAgEWF2h0dHBzOi8vZC5zeW1jYi5jb20v\n"
	"Y3BzMCUGCCsGAQUFBwICMBkMF2h0dHBzOi8vZC5zeW1jYi5jb20vcnBhMCsGA1Ud\n"
	"HwQkMCIwIKAeoByGGmh0dHA6Ly9zZy5zeW1jYi5jb20vc2cuY3JsMB0GA1UdJQQW\n"
	"MBQGCCsGAQUFBwMBBggrBgEFBQcDAjAfBgNVHSMEGDAWgBTbYiD7fQKJfNI7b8fk\n"
	"MmwFUh2tsTBXBggrBgEFBQcBAQRLMEkwHwYIKwYBBQUHMAGGE2h0dHA6Ly9zZy5z\n"
	"eW1jZC5jb20wJgYIKwYBBQUHMAKGGmh0dHA6Ly9zZy5zeW1jYi5jb20vc2cuY3J0\n"
	"MIIB9gYKKwYBBAHWeQIEAgSCAeYEggHiAeAAdQDd6x0reg1PpiCLga2BaHB+Lo6d\n"
	"AdVciI09EcTNtuy+zAAAAVrspLZKAAAEAwBGMEQCIEuVHq0xyXfN8XP0Ly8eTEJB\n"
	"3XMAKhaercn0EqjtgNUPAiBN+/pUJ9EwF4yh2hRU1U3KkdzTI+KDerLZCl004ADF\n"
	"cgB3AKS5CZC0GFgUh7sTosxncAo8NZgE+RvfuON3zQ7IDdwQAAABWuyktugAAAQD\n"
	"AEgwRgIhAPoMU/iz0Ne4vgM4cQol7zLVS8xEc18natc9EgLpOqvvAiEAtyt6nMg7\n"
	"18/axecg1fk2dcDKCP8EpEJWnabnDRwMb00AdgDuS723dc5guuFCaR+r4Z5mow9+\n"
	"X7By2IMAxHuJeqj9ywAAAVrspLadAAAEAwBHMEUCIH8pZN5die2lOt6i2NS57xxd\n"
	"uo12mGZ4Xt6cPARVZCipAiEAxOGXK63gCml0jZnlBAN/41YMCNF0cCl9rMIRmEOe\n"
	"ffYAdgC8eOHfxfY8aEZJM02hD6FfCXlpIAnAgbTz9pF/Ptm4pQAAAVrspLdAAAAE\n"
	"AwBHMEUCIDtHn+q93n/mGGzdFZb9PImevE3t8yi4FpEKQh3fN+HdAiEA618tN/lR\n"
	"9m8dh0BVfHAJ9o3CAT53sWjO37sFhHPNOT4wDQYJKoZIhvcNAQELBQADggEBALEV\n"
	"pKCM+njCWn74ThjbBEH23rWDYNU3Dl4O5J1U0wJdp4uTvZQbTHlvuAYnQr2WUBX5\n"
	"OOvZdVLKDZJqZ/KJ2TLjBUQGWHylu6kE2PWuOTrJ6eC2UnR8zj0RavELHXuGUmQt\n"
	"p5UESDjGI6IUDfI6IdxIKydnIStQLuKlaGsz3bsD1yc8XfCNjkmxf3DfC2qnnO6q\n"
	"0i2o1SkjCesCqrgPQuVM95vF5I+dRcrk1nHOLCgDLYeoOSFpkPzk5EF7gDrfuLHn\n"
	"a7MqZSlOcbf6XcGmsOPH0SCYLyNiJwuBX2W3fw2rP9adpWniGK5kyIEU6Nrkgc31\n"
	"ESMyYNL3A9igh1jySzg=\n"
	"-----END CERTIFICATE-----\n";

static unsigned char ct_extension_der[486] = {
	0x04, 0x82, 0x01, 0xe2, 0x01, 0xe0, 0x00, 0x75, 0x00, 0xdd, 0xeb, 0x1d,
	0x2b, 0x7a, 0x0d, 0x4f, 0xa6, 0x20, 0x8b, 0x81, 0xad, 0x81, 0x68, 0x70,
	0x7e, 0x2e, 0x8e, 0x9d, 0x01, 0xd5, 0x5c, 0x88, 0x8d, 0x3d, 0x11, 0xc4,
	0xcd, 0xb6, 0xec, 0xbe, 0xcc, 0x00, 0x00, 0x01, 0x5a, 0xec, 0xa4, 0xb6,
	0x4a, 0x00, 0x00, 0x04, 0x03, 0x00, 0x46, 0x30, 0x44, 0x02, 0x20, 0x4b,
	0x95, 0x1e, 0xad, 0x31, 0xc9, 0x77, 0xcd, 0xf1, 0x73, 0xf4, 0x2f, 0x2f,
	0x1e, 0x4c, 0x42, 0x41, 0xdd, 0x73, 0x00, 0x2a, 0x16, 0x9e, 0xad, 0xc9,
	0xf4, 0x12, 0xa8, 0xed, 0x80, 0xd5, 0x0f, 0x02, 0x20, 0x4d, 0xfb, 0xfa,
	0x54, 0x27, 0xd1, 0x30, 0x17, 0x8c, 0xa1, 0xda, 0x14, 0x54, 0xd5, 0x4d,
	0xca, 0x91, 0xdc, 0xd3, 0x23, 0xe2, 0x83, 0x7a, 0xb2, 0xd9, 0x0a, 0x5d,
	0x34, 0xe0, 0x00, 0xc5, 0x72, 0x00, 0x77, 0x00, 0xa4, 0xb9, 0x09, 0x90,
	0xb4, 0x18, 0x58, 0x14, 0x87, 0xbb, 0x13, 0xa2, 0xcc, 0x67, 0x70, 0x0a,
	0x3c, 0x35, 0x98, 0x04, 0xf9, 0x1b, 0xdf, 0xb8, 0xe3, 0x77, 0xcd, 0x0e,
	0xc8, 0x0d, 0xdc, 0x10, 0x00, 0x00, 0x01, 0x5a, 0xec, 0xa4, 0xb6, 0xe8,
	0x00, 0x00, 0x04, 0x03, 0x00, 0x48, 0x30, 0x46, 0x02, 0x21, 0x00, 0xfa,
	0x0c, 0x53, 0xf8, 0xb3, 0xd0, 0xd7, 0xb8, 0xbe, 0x03, 0x38, 0x71, 0x0a,
	0x25, 0xef, 0x32, 0xd5, 0x4b, 0xcc, 0x44, 0x73, 0x5f, 0x27, 0x6a, 0xd7,
	0x3d, 0x12, 0x02, 0xe9, 0x3a, 0xab, 0xef, 0x02, 0x21, 0x00, 0xb7, 0x2b,
	0x7a, 0x9c, 0xc8, 0x3b, 0xd7, 0xcf, 0xda, 0xc5, 0xe7, 0x20, 0xd5, 0xf9,
	0x36, 0x75, 0xc0, 0xca, 0x08, 0xff, 0x04, 0xa4, 0x42, 0x56, 0x9d, 0xa6,
	0xe7, 0x0d, 0x1c, 0x0c, 0x6f, 0x4d, 0x00, 0x76, 0x00, 0xee, 0x4b, 0xbd,
	0xb7, 0x75, 0xce, 0x60, 0xba, 0xe1, 0x42, 0x69, 0x1f, 0xab, 0xe1, 0x9e,
	0x66, 0xa3, 0x0f, 0x7e, 0x5f, 0xb0, 0x72, 0xd8, 0x83, 0x00, 0xc4, 0x7b,
	0x89, 0x7a, 0xa8, 0xfd, 0xcb, 0x00, 0x00, 0x01, 0x5a, 0xec, 0xa4, 0xb6,
	0x9d, 0x00, 0x00, 0x04, 0x03, 0x00, 0x47, 0x30, 0x45, 0x02, 0x20, 0x7f,
	0x29, 0x64, 0xde, 0x5d, 0x89, 0xed, 0xa5, 0x3a, 0xde, 0xa2, 0xd8, 0xd4,
	0xb9, 0xef, 0x1c, 0x5d, 0xba, 0x8d, 0x76, 0x98, 0x66, 0x78, 0x5e, 0xde,
	0x9c, 0x3c, 0x04, 0x55, 0x64, 0x28, 0xa9, 0x02, 0x21, 0x00, 0xc4, 0xe1,
	0x97, 0x2b, 0xad, 0xe0, 0x0a, 0x69, 0x74, 0x8d, 0x99, 0xe5, 0x04, 0x03,
	0x7f, 0xe3, 0x56, 0x0c, 0x08, 0xd1, 0x74, 0x70, 0x29, 0x7d, 0xac, 0xc2,
	0x11, 0x98, 0x43, 0x9e, 0x7d, 0xf6, 0x00, 0x76, 0x00, 0xbc, 0x78, 0xe1,
	0xdf, 0xc5, 0xf6, 0x3c, 0x68, 0x46, 0x49, 0x33, 0x4d, 0xa1, 0x0f, 0xa1,
	0x5f, 0x09, 0x79, 0x69, 0x20, 0x09, 0xc0, 0x81, 0xb4, 0xf3, 0xf6, 0x91,
	0x7f, 0x3e, 0xd9, 0xb8, 0xa5, 0x00, 0x00, 0x01, 0x5a, 0xec, 0xa4, 0xb7,
	0x40, 0x00, 0x00, 0x04, 0x03, 0x00, 0x47, 0x30, 0x45, 0x02, 0x20, 0x3b,
	0x47, 0x9f, 0xea, 0xbd, 0xde, 0x7f, 0xe6, 0x18, 0x6c, 0xdd, 0x15, 0x96,
	0xfd, 0x3c, 0x89, 0x9e, 0xbc, 0x4d, 0xed, 0xf3, 0x28, 0xb8, 0x16, 0x91,
	0x0a, 0x42, 0x1d, 0xdf, 0x37, 0xe1, 0xdd, 0x02, 0x21, 0x00, 0xeb, 0x5f,
	0x2d, 0x37, 0xf9, 0x51, 0xf6, 0x6f, 0x1d, 0x87, 0x40, 0x55, 0x7c, 0x70,
	0x09, 0xf6, 0x8d, 0xc2, 0x01, 0x3e, 0x77, 0xb1, 0x68, 0xce, 0xdf, 0xbb,
	0x05, 0x84, 0x73, 0xcd, 0x39, 0x3e
};

static void check_scts(const gnutls_datum_t *ext)
{
	int ret;
	unsigned int i, version;
	time_t timestamp;
	gnutls_datum_t logid, sig, xder, ext_out;
	gnutls_sign_algorithm_t sigalg;
	gnutls_x509_ct_scts_t scts;
#define EXPECTED_LOGID_SIZE 32
#define NUM_EXPECTED_SCTS 4
	struct sct_data {
		unsigned char logid[EXPECTED_LOGID_SIZE];
		/* time_t timestamp; */
		gnutls_sign_algorithm_t sigalg;
		gnutls_datum_t sig;
	} expected_data[NUM_EXPECTED_SCTS] = {
		{ .logid =
			  "\xdd\xeb\x1d\x2b\x7a\x0d\x4f\xa6\x20\x8b\x81\xad\x81\x68\x70\x7e"
			  "\x2e\x8e\x9d\x01\xd5\x5c\x88\x8d\x3d\x11\xc4\xcd\xb6\xec\xbe\xcc",
		  .sigalg = GNUTLS_SIGN_ECDSA_SHA256,
		  .sig = { .size = 70,
			   .data = (unsigned char
					    *)"\x30\x44\x02\x20\x4b\x95\x1e\xad\x31\xc9\x77\xcd\xf1\x73\xf4\x2f"
					      "\x2f\x1e\x4c\x42\x41\xdd\x73\x00\x2a\x16\x9e\xad\xc9\xf4\x12\xa8"
					      "\xed\x80\xd5\x0f\x02\x20\x4d\xfb\xfa\x54\x27\xd1\x30\x17\x8c\xa1"
					      "\xda\x14\x54\xd5\x4d\xca\x91\xdc\xd3\x23\xe2\x83\x7a\xb2\xd9\x0a"
					      "\x5d\x34\xe0\x00\xc5\x72" } },
		{ .logid =
			  "\xa4\xb9\x09\x90\xb4\x18\x58\x14\x87\xbb\x13\xa2\xcc\x67\x70\x0a"
			  "\x3c\x35\x98\x04\xf9\x1b\xdf\xb8\xe3\x77\xcd\x0e\xc8\x0d\xdc\x10",
		  .sigalg = GNUTLS_SIGN_ECDSA_SHA256,
		  .sig = { .size = 72,
			   .data = (unsigned char
					    *)"\x30\x46\x02\x21\x00\xfa\x0c\x53\xf8\xb3\xd0\xd7\xb8\xbe\x03\x38"
					      "\x71\x0a\x25\xef\x32\xd5\x4b\xcc\x44\x73\x5f\x27\x6a\xd7\x3d\x12"
					      "\x02\xe9\x3a\xab\xef\x02\x21\x00\xb7\x2b\x7a\x9c\xc8\x3b\xd7\xcf"
					      "\xda\xc5\xe7\x20\xd5\xf9\x36\x75\xc0\xca\x08\xff\x04\xa4\x42\x56"
					      "\x9d\xa6\xe7\x0d\x1c\x0c\x6f\x4d" } },
		{ .logid =
			  "\xee\x4b\xbd\xb7\x75\xce\x60\xba\xe1\x42\x69\x1f\xab\xe1\x9e\x66"
			  "\xa3\x0f\x7e\x5f\xb0\x72\xd8\x83\x00\xc4\x7b\x89\x7a\xa8\xfd\xcb",
		  .sigalg = GNUTLS_SIGN_ECDSA_SHA256,
		  .sig = { .size = 71,
			   .data = (unsigned char
					    *)"\x30\x45\x02\x20\x7f\x29\x64\xde\x5d\x89\xed\xa5\x3a\xde\xa2\xd8"
					      "\xd4\xb9\xef\x1c\x5d\xba\x8d\x76\x98\x66\x78\x5e\xde\x9c\x3c\x04"
					      "\x55\x64\x28\xa9\x02\x21\x00\xc4\xe1\x97\x2b\xad\xe0\x0a\x69\x74"
					      "\x8d\x99\xe5\x04\x03\x7f\xe3\x56\x0c\x08\xd1\x74\x70\x29\x7d\xac"
					      "\xc2\x11\x98\x43\x9e\x7d\xf6" } },
		{ .logid =
			  "\xbc\x78\xe1\xdf\xc5\xf6\x3c\x68\x46\x49\x33\x4d\xa1\x0f\xa1\x5f"
			  "\x09\x79\x69\x20\x09\xc0\x81\xb4\xf3\xf6\x91\x7f\x3e\xd9\xb8\xa5",
		  .sigalg = GNUTLS_SIGN_ECDSA_SHA256,
		  .sig = { .size = 71,
			   .data = (unsigned char
					    *)"\x30\x45\x02\x20\x3b\x47\x9f\xea\xbd\xde\x7f\xe6\x18\x6c\xdd\x15"
					      "\x96\xfd\x3c\x89\x9e\xbc\x4d\xed\xf3\x28\xb8\x16\x91\x0a\x42\x1d"
					      "\xdf\x37\xe1\xdd\x02\x21\x00\xeb\x5f\x2d\x37\xf9\x51\xf6\x6f\x1d"
					      "\x87\x40\x55\x7c\x70\x09\xf6\x8d\xc2\x01\x3e\x77\xb1\x68\xce\xdf"
					      "\xbb\x05\x84\x73\xcd\x39\x3e" } }
	};

	ret = gnutls_x509_ext_ct_scts_init(&scts);
	if (ret < 0)
		fail("gnutls_x509_ext_ct_scts_init");

	ret = gnutls_x509_ext_ct_import_scts(ext, scts, 0);
	if (ret < 0)
		fail("gnutls_x509_ext_ct_import_scts");

	for (i = 0; i < NUM_EXPECTED_SCTS; i++) {
		ret = gnutls_x509_ct_sct_get_version(scts, i, &version);
		if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			break;
		if (ret < 0)
			fail("gnutls_x509_ct_sct_get_version");
		if (version != 1)
			fail("invalid version");
		if (gnutls_x509_ct_sct_get(scts, i, &timestamp, &logid, &sigalg,
					   &sig) < 0)
			fail("gnutls_x509_ct_sct_v1_get");
		if (logid.size != EXPECTED_LOGID_SIZE)
			fail("Log ID sizes do not match for SCT %d", i);
		if (memcmp(logid.data, expected_data[i].logid,
			   EXPECTED_LOGID_SIZE) != 0)
			fail("Log IDs do not match for SCT %d", i);
		if (sigalg != expected_data[i].sigalg)
			fail("Signature algorithms for SCT %d do not match", i);
		if (sig.size != expected_data[i].sig.size)
			fail("Signature sizes for SCT %d do not match", i);
		if (memcmp(sig.data, expected_data[i].sig.data, sig.size) != 0)
			fail("Signatures for SCT %d do not match", i);

		gnutls_free(logid.data);
		gnutls_free(sig.data);
		logid.size = 0;
		sig.size = 0;
	}

	if (i != NUM_EXPECTED_SCTS)
		fail("Less than expected SCTs were seen");

	/* Now export the whole SCT list as DER, and check if it matches
	 * our expected DER. */
	ret = gnutls_x509_ext_ct_export_scts(scts, &ext_out);
	if (ret < 0)
		fail("gnutls_x509_ext_ct_export_scts");

	xder.data = ct_extension_der;
	xder.size = sizeof(ct_extension_der);
	if (ext_out.size != xder.size ||
	    memcmp(ext_out.data, xder.data, xder.size) != 0)
		fail("DERs do not match");

	gnutls_free(ext_out.data);

	gnutls_x509_ext_ct_scts_deinit(scts);
}

#define MAX_DATA_SIZE 1024

void doit(void)
{
	int ret;
	bool scts_printed = 0;
	size_t oidlen;
	char oid[MAX_DATA_SIZE];
	gnutls_x509_crt_t cert;
	gnutls_datum_t ext, xpem = { (void *)pem, sizeof(pem) - 1 };

	global_init();
	if (debug)
		gnutls_global_set_log_level(5);

	gnutls_x509_crt_init(&cert);
	ret = gnutls_x509_crt_import(cert, &xpem, GNUTLS_X509_FMT_PEM);
	if (ret < 0)
		fail("gnutls_x509_crt_import");

	for (unsigned i = 0;; i++) {
		oidlen = MAX_DATA_SIZE;

		ret = gnutls_x509_crt_get_extension_oid(cert, i, oid, &oidlen);
		if (ret == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
			break;
		if (ret < 0)
			fail("gnutls_x509_crt_get_extension_oid");

		if (strcmp(oid, GNUTLS_X509EXT_OID_CT_SCT_V1) == 0) {
			ret = gnutls_x509_crt_get_extension_data2(cert, i,
								  &ext);
			if (ret < 0)
				fail("gnutls_x509_crt_get_extension_data2");
			check_scts(&ext);
			scts_printed = 1;
			gnutls_free(ext.data);
			break;
		}
	}

	if (!scts_printed)
		fail("SCT extension not found\n");

	gnutls_x509_crt_deinit(cert);

	gnutls_global_deinit();
	if (debug)
		success("success");
}
