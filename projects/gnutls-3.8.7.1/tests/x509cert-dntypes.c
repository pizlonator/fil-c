/*
 * Copyright (C) 2020 Pierre Ossman for Cendio AB
 *
 * Author: Pierre Ossman
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
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "utils.h"

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "<%d>| %s", level, str);
}

/* the issuer/subject connection between the server cert and the CA
 * cert uses different ASN.1 types, which is uncommon but allowed */

static unsigned char server_pem[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIDZTCCAc2gAwIBAgIUB7aVTQvtbBpOEtKELkBkLViM0eIwDQYJKoZIhvcNAQEL\n"
	"BQAwEjEQMA4GA1UEAwwHVGVzdCBDQTAeFw0yMDAzMjYxMDE4NTdaFw0yMTAzMjYx\n"
	"MDE4NTdaMBYxFDASBgNVBAMMC1Rlc3QgY2xpZW50MIIBIjANBgkqhkiG9w0BAQEF\n"
	"AAOCAQ8AMIIBCgKCAQEAviqj5S/xe39agbMnq/oPAQmdIhalB17Ewc3AZlD8n+zQ\n"
	"scPDNvnk4gxSeSXePtXmh0OaGcBKbMAkjiyo2gPBmV3ay34LQuk97nJxE2TUAWMm\n"
	"S8yFwP3yoE+GZ5eYjv+HGQxeAP9uHLjho/jHjVGgUOCVv1QjsKyRx8Tuvy9TH3ON\n"
	"DuMPw3Jmnq0OhLy2+SjU0ug5jxfWJvnfeGoFzRgalmWGyoAQsH9bqha/D44QSen+\n"
	"Zbbt/A4uNIILAENYuHXEfvpmBuZPpocOb6h2huGbp6iHZfdZUHso37UmWT6PXh+2\n"
	"dASPaCpAr3bURBhnEsQM43njb8METZewMeoQxwZC0QIDAQABoy8wLTAMBgNVHRMB\n"
	"Af8EAjAAMB0GA1UdDgQWBBSb3h7ZbajS/2RWx2a7hTVSkur0FDANBgkqhkiG9w0B\n"
	"AQsFAAOCAYEAPfwyvOwNEjIvlifjBVhiWmrtZAS2YaY9jqFnaA2PvYY2FVyC3AMu\n"
	"3BGAorau/4DL3P92/9SlygEmBQpqCq+AJnQRH6WKFT4avAOmw3yc0++st+DhGK0I\n"
	"6Cr69WccVi0Kmxi1XP4dpPDWSuVCOP6rGc3ulgEH83xF4ZL+3qVA9Fihsie3ZZme\n"
	"7mqWOznVO1MZHLDFIUEoRdOSin5bIkl7FPOCZqMsWRM41GuA1h4aX/X5dLeqRW1c\n"
	"mJ5CNRWwPIPcwgqeldFnx07svCv9QseUDaIw+C9vZOlgfIgp0qeYoR6fsD38WcUC\n"
	"eJPsOUwhdhMcw+/PM16iwzd89dI+PCecFY9FeLh9YeihZm0DnG8L0To1Y2ry+WRf\n"
	"w5knR3FReHPcelymvSKZSEG0d/KKHXBeKWgcrCrdnn4ya71eblsNzO3vnxB5k0Zj\n"
	"WcQ3wfeftQKDEIuaRHUP6B4zx2teJWMWvJLcXuavoqo0z3L5EN74RztCpnP9ykSH\n"
	"ZsYWoJ3aelFv\n"
	"-----END CERTIFICATE-----\n";

static unsigned char ca_pem[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIID5DCCAkygAwIBAgIUB4lnLAeQ20wlYbqt5ykgvWOPNzgwDQYJKoZIhvcNAQEL\n"
	"BQAwEjEQMA4GA1UEAxMHVGVzdCBDQTAeFw0yMDAzMjYxMDI0MjhaFw0yMDAzMjcx\n"
	"MDI0MjhaMBIxEDAOBgNVBAMTB1Rlc3QgQ0EwggGiMA0GCSqGSIb3DQEBAQUAA4IB\n"
	"jwAwggGKAoIBgQCt9z/noU7qCPquzzgwNvu/rwXyIvxmqdWhpfpBOmVq8wpgUDUU\n"
	"cQ94F65UfTo3EcYXCoDs43E4Wo8KmF5YQM2xK+LrH28XmpL3z+NoQGaZoUVrMWp6\n"
	"rbIeoGZvITaaGn2uEbGT7iRkBUdS4wOjUT13IxpG8cM4d0i0DIsqSlUPnQCfyMqf\n"
	"jsVhO9IQsn7qMo0+2nNCI5JqblEXRvL39hHzJMOsq1NRqZO1Zjt9HCIB7m7Q42Jx\n"
	"e8zm7RzTiBFVKecxb5h4mmt3tUZQ0Kjd94yE6ARSE0rULmO+6H7hgI6sU8vqfSFe\n"
	"DimQ5mPReumBRDcErX+c7bRGPRul41kAB8XvPmAHG8xCepjH8xrgY/FeVBQT74xm\n"
	"MEYQaxdGpa8Azx6MZCrZOI0rzu+zI0CBQGE1h1Xk8HBozrn/G2OOAZcXyzHzq56R\n"
	"Z52zEQYFZmKH9tHTDI6fMfo8clr7esb/wmgEOt/lJYE9IMJrzUh+IwWuowdYaDVj\n"
	"nMrboUBVepmBKSUCAwEAAaMyMDAwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQU\n"
	"rhkYiczAkbCcVfNr67VGGaqilbgwDQYJKoZIhvcNAQELBQADggGBAGYiUTKdYBXk\n"
	"lZFIhZkCc33/lCgJw2mSrdAd+xJmJonRPy3qmYy3HniOmQdRVqResLALubz89VjJ\n"
	"dSeokujFrlNtb4CygojseqTsxWgeZlKjLU3tJ/Xn+DFIiP7k9+WPW7KFIIW0fq61\n"
	"MAI0lKjqpC8sJTlXoJemDw9MW/380nKr+K1YY3arRzsSHEIeA54xOggKEwvgz11A\n"
	"47xT83WoLwFQ4e9LZfCsL/M51lsLHAlJzDKyTTeSxCi/C6kUIzx8QyxHKYgBuNxz\n"
	"8vVLY/YzUv/l5ELYQ9gkAX0vZWdw7pqASUY8yvbzImrWqjFAHeN3zK687Ke9uppS\n"
	"dmjvPwvTK+SKm++NR8YCwb3xqHQHMYHV3lxjlOhaN6rxBW0l4gtvb2FMlhcljiZ+\n"
	"tF2ObVwEs6nqJSGrzubp0os+WmnbVSCaHz9jnRWb68C87mXCZkbA7FTSKJOVuqRM\n"
	"vVTcHQ7jwGQ2/SvikndFQ53zi2j9o/jTOiFv29rEOeHu67UAiFSi2A==\n"
	"-----END CERTIFICATE-----\n";

const gnutls_datum_t server = { server_pem, sizeof(server_pem) - 1 };
const gnutls_datum_t ca = { ca_pem, sizeof(ca_pem) - 1 };

void doit(void)
{
	int ret;
	gnutls_x509_crt_t server_crt, ca_crt;

	/* this must be called once in the program
	 */
	global_init();

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(6);

	gnutls_x509_crt_init(&server_crt);

	ret = gnutls_x509_crt_import(server_crt, &server, GNUTLS_X509_FMT_PEM);
	if (ret < 0)
		fail("gnutls_x509_crt_import");

	gnutls_x509_crt_init(&ca_crt);

	ret = gnutls_x509_crt_import(ca_crt, &ca, GNUTLS_X509_FMT_PEM);
	if (ret < 0)
		fail("gnutls_x509_crt_import");

	ret = gnutls_x509_crt_check_issuer(server_crt, ca_crt);
	if (!ret)
		fail("gnutls_x509_crt_check_issuer");

	gnutls_x509_crt_deinit(ca_crt);
	gnutls_x509_crt_deinit(server_crt);

	gnutls_global_deinit();

	if (debug)
		success("success");
}
