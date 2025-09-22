/*
 * Copyright(c) 2019 Free Software Foundation, Inc.
 *
 * This file is part of libtasn1.
 *
 * Libtasn1 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Libtasn1 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libtasn1.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This fuzzer is testing arbitrary DER input data with GnuTLS's ASN.1 definition (lib/gnutls.asn).
 * So, any issues found here likely have a real world impact on every software using libgnutls.
 */

#include <config.h>

#include <assert.h>		/* assert */
#include <stdlib.h>		/* malloc, free */
#include <string.h>		/* strcmp, memcpy */

#include "libtasn1.h"
#include "fuzzer.h"

/*
 * This is a ASN.1 definition array used by GnuTLS.
 * It is created from lib/gnutls.asn over at the GnuTLS project.
 */
const asn1_static_node gnutls_asn1_tab[] = {
  {"GNUTLS", 536872976, NULL},
  {NULL, 1073741836, NULL},
  {"RSAPublicKey", 1610612741, NULL},
  {"modulus", 1073741827, NULL},
  {"publicExponent", 3, NULL},
  {"RSAPrivateKey", 1610612741, NULL},
  {"version", 1073741827, NULL},
  {"modulus", 1073741827, NULL},
  {"publicExponent", 1073741827, NULL},
  {"privateExponent", 1073741827, NULL},
  {"prime1", 1073741827, NULL},
  {"prime2", 1073741827, NULL},
  {"exponent1", 1073741827, NULL},
  {"exponent2", 1073741827, NULL},
  {"coefficient", 1073741827, NULL},
  {"otherPrimeInfos", 16386, "OtherPrimeInfos"},
  {"ProvableSeed", 1610612741, NULL},
  {"algorithm", 1073741836, NULL},
  {"seed", 7, NULL},
  {"OtherPrimeInfos", 1612709899, NULL},
  {"MAX", 1074266122, "1"},
  {NULL, 2, "OtherPrimeInfo"},
  {"OtherPrimeInfo", 1610612741, NULL},
  {"prime", 1073741827, NULL},
  {"exponent", 1073741827, NULL},
  {"coefficient", 3, NULL},
  {"AlgorithmIdentifier", 1610612741, NULL},
  {"algorithm", 1073741836, NULL},
  {"parameters", 541081613, NULL},
  {"algorithm", 1, NULL},
  {"DigestInfo", 1610612741, NULL},
  {"digestAlgorithm", 1073741826, "DigestAlgorithmIdentifier"},
  {"digest", 7, NULL},
  {"DigestAlgorithmIdentifier", 1073741826, "AlgorithmIdentifier"},
  {"DSAPublicKey", 1073741827, NULL},
  {"DSAParameters", 1610612741, NULL},
  {"p", 1073741827, NULL},
  {"q", 1073741827, NULL},
  {"g", 3, NULL},
  {"DSASignatureValue", 1610612741, NULL},
  {"r", 1073741827, NULL},
  {"s", 3, NULL},
  {"DSAPrivateKey", 1610612741, NULL},
  {"version", 1073741827, NULL},
  {"p", 1073741827, NULL},
  {"q", 1073741827, NULL},
  {"g", 1073741827, NULL},
  {"Y", 1073741827, NULL},
  {"priv", 3, NULL},
  {"DHParameter", 1610612741, NULL},
  {"prime", 1073741827, NULL},
  {"base", 1073741827, NULL},
  {"privateValueLength", 16387, NULL},
  {"ECParameters", 1610612754, NULL},
  {"namedCurve", 12, NULL},
  {"ECPrivateKey", 1610612741, NULL},
  {"Version", 1073741827, NULL},
  {"privateKey", 1073741831, NULL},
  {"parameters", 1610637314, "ECParameters"},
  {NULL, 2056, "0"},
  {"publicKey", 536895494, NULL},
  {NULL, 2056, "1"},
  {"PrincipalName", 1610612741, NULL},
  {"name-type", 1610620931, NULL},
  {NULL, 2056, "0"},
  {"name-string", 536879115, NULL},
  {NULL, 1073743880, "1"},
  {NULL, 27, NULL},
  {"KRB5PrincipalName", 1610612741, NULL},
  {"realm", 1610620955, NULL},
  {NULL, 2056, "0"},
  {"principalName", 536879106, "PrincipalName"},
  {NULL, 2056, "1"},
  {"RSAPSSParameters", 1610612741, NULL},
  {"hashAlgorithm", 1610637314, "AlgorithmIdentifier"},
  {NULL, 2056, "0"},
  {"maskGenAlgorithm", 1610637314, "AlgorithmIdentifier"},
  {NULL, 2056, "1"},
  {"saltLength", 1610653699, NULL},
  {NULL, 1073741833, "20"},
  {NULL, 2056, "2"},
  {"trailerField", 536911875, NULL},
  {NULL, 1073741833, "1"},
  {NULL, 2056, "3"},
  {"GOSTParameters", 1610612741, NULL},
  {"publicKeyParamSet", 1073741836, NULL},
  {"digestParamSet", 1073741836, NULL},
  {"encryptionParamSet", 16396, NULL},
  {"GOSTPrivateKey", 1073741831, NULL},
  {"GOSTPrivateKeyOld", 3, NULL},
  {NULL, 0, NULL}
};

int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
  static asn1_node _gnutls_gnutls_asn = NULL;
  static int first = 1;
  asn1_node dn;
  int res;

  if (size > 10000)		/* same as max_len = 10000 in .options file */
    return 0;

  if (first)
    {
      first = 0;

      /* from _gnutls_global_init() */
      res = asn1_array2tree (gnutls_asn1_tab, &_gnutls_gnutls_asn, NULL);
      assert (res == ASN1_SUCCESS);
    }

  /* from gnutls_dh_params_import_pkcs3() */
  if ((res =
       asn1_create_element (_gnutls_gnutls_asn, "GNUTLS.DHParameter",
			    &dn)) == ASN1_SUCCESS)
    {
      /* from cert_get_issuer_dn() */
      res = asn1_der_decoding (&dn, data, size, NULL);
      asn1_delete_structure (&dn);
    }

  /* from _gnutls_x509_write_gost_params() */
  if ((res =
       asn1_create_element (_gnutls_gnutls_asn, "GNUTLS.GOSTParameters",
			    &dn)) == ASN1_SUCCESS)
    {
      if ((res =
	   asn1_write_value (dn, "digestParamSet", "1.2.643.7.1.1.2.2",
			     1)) == ASN1_SUCCESS)
	{
	  /* from cert_get_issuer_dn() */
	  res = asn1_der_decoding (&dn, data, size, NULL);

	  /* from _gnutls_x509_der_encode() */
	  int dersize = 0;
	  if ((res =
	       asn1_der_coding (dn, "", NULL, &dersize,
				NULL)) == ASN1_MEM_ERROR)
	    {
	      void *der = malloc (dersize);
	      assert (der);
	      res = asn1_der_coding (dn, "", der, &dersize, NULL);
	      free (der);
	    }
	}

      asn1_delete_structure (&dn);
    }

  return 0;
}
