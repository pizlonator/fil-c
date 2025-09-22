/*
 * Copyright (C) 2000-2022 Free Software Foundation, Inc.
 *
 * This file is part of LIBTASN1.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*****************************************************/
/* File: CrlExample.c                                */
/* Description: An example on how to use the ASN1    */
/*              parser with the Certificate.txt file */
/*****************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libtasn1.h"



static char *
my_ltostr (long v, char *str)
{
  long d, r;
  char temp[20];
  int count, k, start;

  if (v < 0)
    {
      str[0] = '-';
      start = 1;
      v = -v;
    }
  else
    start = 0;

  count = 0;
  do
    {
      d = v / 10;
      r = v - d * 10;
      temp[start + count] = '0' + (char) r;
      count++;
      v = d;
    }
  while (v);

  for (k = 0; k < count; k++)
    str[k + start] = temp[start + count - k - 1];
  str[count + start] = 0;
  return str;
}


/******************************************************/
/* Function : get_name_type                           */
/* Description: analyze a structure of type Name      */
/* Parameters:                                        */
/*   char *root: the structure identifier             */
/*   char *answer: the string with elements like:     */
/*                 "C=US O=gov"                       */
/******************************************************/
static void
get_Name_type (asn1_node cert_def, asn1_node cert, const char *root,
	       unsigned char *ans)
{
  int k, k2, result, len;
  char name[128], str[1024], str2[1024], name2[128], counter[5], name3[128];
  asn1_node value = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  char *answer = (char *) ans;

  answer[0] = 0;
  k = 1;
  do
    {
      strcpy (name, root);
      strcat (name, ".rdnSequence.?");
      my_ltostr (k, counter);
      strcat (name, counter);

      len = sizeof (str) - 1;
      result = asn1_read_value (cert, name, str, &len);
      if (result == ASN1_ELEMENT_NOT_FOUND)
	break;
      k2 = 1;
      do
	{
	  strcpy (name2, name);
	  strcat (name2, ".?");
	  my_ltostr (k2, counter);
	  strcat (name2, counter);

	  len = sizeof (str) - 1;
	  result = asn1_read_value (cert, name2, str, &len);
	  if (result == ASN1_ELEMENT_NOT_FOUND)
	    break;
	  strcpy (name3, name2);
	  strcat (name3, ".type");

	  len = sizeof (str) - 1;
	  result = asn1_read_value (cert, name3, str, &len);
	  strcpy (name3, name2);
	  strcat (name3, ".value");
	  if (result == ASN1_SUCCESS)
	    {
	      len = sizeof (str2);
	      result =
		asn1_read_value (cert_def,
				 "PKIX1Implicit88.id-at-countryName", str2,
				 &len);
	      if (!strcmp (str, str2))
		{
		  asn1_create_element (cert_def,
				       "PKIX1Implicit88.X520OrganizationName",
				       &value);
		  len = sizeof (str) - 1;
		  asn1_read_value (cert, name3, str, &len);
		  result =
		    asn1_der_decoding (&value, str, len, errorDescription);

		  len = sizeof (str) - 1;
		  asn1_read_value (value, "", str, &len);	/* CHOICE */

		  strcpy (name3, str);

		  len = sizeof (str) - 1;
		  asn1_read_value (value, name3, str, &len);
		  str[len] = 0;
		  strcat (answer, " C=");
		  strcat (answer, str);

		  asn1_delete_structure (&value);
		}
	      else
		{
		  len = sizeof (str2);
		  result =
		    asn1_read_value (cert_def,
				     "PKIX1Implicit88.id-at-organizationName",
				     str2, &len);
		  if (!strcmp (str, str2))
		    {
		      asn1_create_element (cert_def,
					   "PKIX1Implicit88.X520OrganizationName",
					   &value);

		      len = sizeof (str) - 1;
		      asn1_read_value (cert, name3, str, &len);
		      asn1_der_decoding (&value, str, len, errorDescription);
		      len = sizeof (str) - 1;
		      asn1_read_value (value, "", str, &len);	/* CHOICE */
		      strcpy (name3, str);
		      len = sizeof (str) - 1;
		      asn1_read_value (value, name3, str, &len);
		      str[len] = 0;
		      strcat (answer, " O=");
		      strcat (answer, str);
		      asn1_delete_structure (&value);
		    }
		  else
		    {
		      len = sizeof (str2);
		      result =
			asn1_read_value (cert_def,
					 "PKIX1Implicit88.id-at-organizationalUnitName",
					 str2, &len);
		      if (!strcmp (str, str2))
			{
			  asn1_create_element (cert_def,
					       "PKIX1Implicit88.X520OrganizationalUnitName",
					       &value);
			  len = sizeof (str) - 1;
			  asn1_read_value (cert, name3, str, &len);
			  asn1_der_decoding (&value, str, len,
					     errorDescription);
			  len = sizeof (str) - 1;
			  asn1_read_value (value, "", str, &len);	/* CHOICE */
			  strcpy (name3, str);
			  len = sizeof (str) - 1;
			  asn1_read_value (value, name3, str, &len);
			  str[len] = 0;
			  strcat (answer, " OU=");
			  strcat (answer, str);
			  asn1_delete_structure (&value);
			}
		    }
		}
	    }
	  k2++;
	}
      while (1);
      k++;
    }
  while (1);
}


/******************************************************/
/* Function : create_certificate                      */
/* Description: creates a certificate named           */
/*              "certificate1". Values are the same   */
/*              as in rfc2459 Appendix D.1            */
/* Parameters:                                        */
/*   unsigned char *der: contains the der encoding    */
/*   int *der_len: number of bytes of der string      */
/******************************************************/
static void
create_CRL (asn1_node cert_def, unsigned char *der, int *der_len)
{
  int result, k, len;
  unsigned char str[1024];
  const unsigned char *str2;
  asn1_node crl = NULL;
  asn1_node value = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  int max_len;

  max_len = *der_len;

  result =
    asn1_create_element (cert_def, "PKIX1Implicit88.CertificateList", &crl);

  /* Use the next 3 lines to visit the empty certificate */
  /*  printf("-----------------\n");
     asn1_visit_tree(crl,"");
     printf("-----------------\n"); */


  /* version: v2(1) */
  result = asn1_write_value (crl, "tbsCertList.version", "v2", 0);


  /* signature: dsa-with-sha */
  len = sizeof (str) - 1;
  result =
    asn1_read_value (cert_def, "PKIX1Implicit88.id-dsa-with-sha1", str, &len);
  result = asn1_write_value (crl, "tbsCertList.signature.algorithm", str, 1);
  result =
    asn1_write_value (crl, "tbsCertList.signature.parameters", NULL, 0);


  /* issuer: Country="US" Organization="gov" OrganizationUnit="nist" */
  result = asn1_write_value (crl, "tbsCertList.issuer", "rdnSequence", 1);

  result = asn1_write_value (crl, "tbsCertList.issuer.rdnSequence", "NEW", 1);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST", "NEW", 1);
  /* C */
  len = sizeof (str) - 1;
  result =
    asn1_read_value (cert_def, "PKIX1Implicit88.id-at-countryName", str,
		     &len);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST.?LAST.type",
		      str, 1);
  result =
    asn1_create_element (cert_def, "PKIX1Implicit88.X520countryName", &value);
  result = asn1_write_value (value, "", "US", 2);
  *der_len = max_len;
  result = asn1_der_coding (value, "", der, der_len, errorDescription);

  asn1_delete_structure (&value);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST.?LAST.value",
		      der, *der_len);


  result = asn1_write_value (crl, "tbsCertList.issuer.rdnSequence", "NEW", 4);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST", "NEW", 4);
  /* O */
  len = sizeof (str) - 1;
  result =
    asn1_read_value (cert_def, "PKIX1Implicit88.id-at-organizationName", str,
		     &len);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST.?LAST.type",
		      str, 8);
  result =
    asn1_create_element (cert_def, "PKIX1Implicit88.X520OrganizationName",
			 &value);
  result = asn1_write_value (value, "", "printableString", 1);
  result = asn1_write_value (value, "printableString", "gov", 3);
  *der_len = max_len;
  result = asn1_der_coding (value, "", der, der_len, errorDescription);
  asn1_delete_structure (&value);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST.?LAST.value",
		      der, *der_len);


  result = asn1_write_value (crl, "tbsCertList.issuer.rdnSequence", "NEW", 1);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST", "NEW", 1);
  /* OU */
  len = sizeof (str) - 1;
  result =
    asn1_read_value (cert_def, "PKIX1Implicit88.id-at-organizationalUnitName",
		     str, &len);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST.?LAST.type",
		      str, 1);
  result =
    asn1_create_element (cert_def,
			 "PKIX1Implicit88.X520OrganizationalUnitName",
			 &value);
  result = asn1_write_value (value, "", "printableString", 1);
  result = asn1_write_value (value, "printableString", "nist", 4);
  *der_len = max_len;
  result = asn1_der_coding (value, "", der, der_len, errorDescription);
  asn1_delete_structure (&value);
  result =
    asn1_write_value (crl, "tbsCertList.issuer.rdnSequence.?LAST.?LAST.value",
		      der, *der_len);


  /* validity */
  result = asn1_write_value (crl, "tbsCertList.thisUpdate", "utcTime", 1);
  result =
    asn1_write_value (crl, "tbsCertList.thisUpdate.utcTime", "970801000000Z",
		      1);

  result = asn1_write_value (crl, "tbsCertList.nextUpdate", "utcTime", 1);
  result =
    asn1_write_value (crl, "tbsCertList.nextUpdate.utcTime", "970808000000Z",
		      1);


  /* revokedCertificates */
  result =
    asn1_write_value (crl, "tbsCertList.revokedCertificates", "NEW", 1);
  str[0] = 18;
  result =
    asn1_write_value (crl,
		      "tbsCertList.revokedCertificates.?LAST.userCertificate",
		      str, 1);
  result =
    asn1_write_value (crl,
		      "tbsCertList.revokedCertificates.?LAST.revocationDate",
		      "utcTime", 1);
  result =
    asn1_write_value (crl,
		      "tbsCertList.revokedCertificates.?LAST.revocationDate.utcTime",
		      "970731000000Z", 1);

  result =
    asn1_write_value (crl,
		      "tbsCertList.revokedCertificates.?LAST.crlEntryExtensions",
		      "NEW", 1);
  len = sizeof (str) - 1;
  result = asn1_read_value (cert_def, "PKIX1Implicit88.id-ce-cRLReasons",
			    str, &len);
  result = asn1_write_value (crl, "tbsCertList.revokedCertificates.?LAST.crlEntryExtensions.?LAST.extnID", str, 1);	/* reasonCode */
  result =
    asn1_write_value (crl,
		      "tbsCertList.revokedCertificates.?LAST.crlEntryExtensions.?LAST.critical",
		      "FALSE", 1);
  str2 = (const unsigned char *) "\x0a\x01\x01";
  result =
    asn1_write_value (crl,
		      "tbsCertList.revokedCertificates.?LAST.crlEntryExtensions.?LAST.extnValue",
		      str2, 3);


  /* crlExtensions */
  result = asn1_write_value (crl, "tbsCertList.crlExtensions", NULL, 0);


  /* signatureAlgorithm: dsa-with-sha  */
  len = sizeof (str) - 1;
  result =
    asn1_read_value (cert_def, "PKIX1Implicit88.id-dsa-with-sha1", str, &len);
  result = asn1_write_value (crl, "signatureAlgorithm.algorithm", str, 1);
  result = asn1_write_value (crl, "signatureAlgorithm.parameters", NULL, 0);	/* NO OPTION */

  /* signature */
  *der_len = max_len;
  result =
    asn1_der_coding (crl, "tbsCertList", der, der_len, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      printf ("\n'tbsCertList' encoding creation: ERROR\n");
      return;
    }

  /* add the lines for the signature on der[0]..der[der_len-1]: result in str2 */
  result = asn1_write_value (crl, "signature", str2, 46 * 8);


  /* Use the next 3 lines to visit the certificate */
  /* printf("-----------------\n");
     asn1_visit_tree(crl,"");
     printf("-----------------\n"); */

  *der_len = max_len;
  result = asn1_der_coding (crl, "", der, der_len, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      printf ("\n'crl1' encoding creation: ERROR\n");
      return;
    }

  /* Print the 'Certificate1' DER encoding */
  printf ("-----------------\nCrl1 Encoding:\nNumber of bytes=%i\n",
	  *der_len);
  for (k = 0; k < *der_len; k++)
    printf ("%02x ", der[k]);
  printf ("\n-----------------\n");

  /* Clear the "certificate1" structure */
  asn1_delete_structure (&crl);
}



/******************************************************/
/* Function : get_certificate                         */
/* Description: creates a certificate named           */
/*              "certificate2" from a der encoding    */
/*              string                                */
/* Parameters:                                        */
/*   unsigned char *der: the encoding string          */
/*   int der_len: number of bytes of der string      */
/******************************************************/
static void
get_CRL (asn1_node cert_def, unsigned char *der, int der_len)
{
  int result, len, start, end;
  unsigned char str[1024], str2[1024];
  asn1_node crl2 = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];


  asn1_create_element (cert_def, "PKIX1Implicit88.CertificateList", &crl2);

  result = asn1_der_decoding (&crl2, der, der_len, errorDescription);

  if (result != ASN1_SUCCESS)
    {
      printf ("Problems with DER encoding\n");
      return;
    }


  /* issuer */
  get_Name_type (cert_def, crl2, "tbsCertList.issuer", str);
  printf ("crl2:\nissuer: %s\n", str);


  /* Verify sign */
  len = sizeof (str) - 1;
  result = asn1_read_value (crl2, "signatureAlgorithm.algorithm", str, &len);

  result =
    asn1_read_value (cert_def, "PKIX1Implicit88.id-dsa-with-sha1", str2,
		     &len);
  if (!strcmp ((char *) str, (char *) str2))
    {				/* dsa-with-sha */

      result = asn1_der_decoding_startEnd (crl2, der, der_len,
					   "tbsCertList", &start, &end);

      /* add the lines to calculate the sha on der[start]..der[end] */

      result = asn1_read_value (crl2, "signature", str, &len);

      /* compare the previous value to signature ( with issuer public key) */
    }

  /* Use the next 3 lines to visit the certificate */
  /* printf("-----------------\n");
     asn1_visit_tree(crl2,"");
     printf("-----------------\n"); */


  /* Clear the "crl2" structure */
  asn1_delete_structure (&crl2);
}

extern const asn1_static_node pkix_asn1_tab[];

/********************************************************/
/* Function : main                                      */
/* Description: reads the certificate description.      */
/*              Creates a certificate and calculate     */
/*              the der encoding. After that creates    */
/*              another certificate from der string     */
/********************************************************/
int
main (int argc, char *argv[])
{
  int result, der_len;
  unsigned char der[1024];
  asn1_node PKIX1Implicit88 = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];

  if (1)
    result =
      asn1_array2tree (pkix_asn1_tab, &PKIX1Implicit88, errorDescription);
  else
    result =
      asn1_parser2tree ("pkix.asn", &PKIX1Implicit88, errorDescription);

  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("%s\n", errorDescription);
      exit (1);
    }

  /* Use the following 3 lines to visit the PKIX1Implicit structures */
  /* printf("-----------------\n");
     asn1_visit_tree(cert_def,"PKIX1Implicit88");
     printf("-----------------\n"); */

  der_len = 1024;
  create_CRL (PKIX1Implicit88, der, &der_len);


  get_CRL (PKIX1Implicit88, der, der_len);

  /* Clear the "PKIX1Implicit88" structures */
  asn1_delete_structure (&PKIX1Implicit88);

  return 0;
}
