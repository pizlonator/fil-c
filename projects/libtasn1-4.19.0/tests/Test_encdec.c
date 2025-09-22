/*
 * Copyright (C) 2002-2022 Free Software Foundation, Inc.
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
/* File: Test_tree.c                                 */
/* Description: Test sequences for these functions:  */
/*     asn1_visit_tree,                              */
/*     asn1_create_element,                          */
/*     asn1_delete_structure,                        */
/*     asn1_write_value,                             */
/*     asn1_read_value,                              */
/*****************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libtasn1.h"



int
main (int argc, char *argv[])
{
  int result;
  char buffer[5 * 1024];
  char buffer2[5 * 1024];
  asn1_node definitions = NULL;
  asn1_node asn1_element = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  FILE *out, *fd;
  ssize_t size;
  int start, end;
  int size2;
  const char *treefile = getenv ("ASN1PKIX");
  const char *derfile = getenv ("ASN1CRLDER");
  int verbose = 0;

  if (argc > 1)
    verbose = 1;

  if (!treefile)
    treefile = "pkix.asn";

  if (!derfile)
    derfile = "crl.der";

  if (verbose)
    {
      printf ("\n\n/****************************************/\n");
      printf ("/*     Test sequence : Test_indefinite  */\n");
      printf ("/****************************************/\n\n");
      printf ("ASN1TREE: %s\n", treefile);
    }

  /* Check version */
  if (asn1_check_version ("0.3.3") == NULL)
    printf ("\nLibrary version check ERROR:\n actual version: %s\n\n",
	    asn1_check_version (NULL));

  result = asn1_parser2tree (treefile, &definitions, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("ErrorDescription = %s\n\n", errorDescription);
      exit (1);
    }

  out = stdout;

  fd = fopen (derfile, "rb");
  if (fd == NULL)
    {
      printf ("Cannot read file %s\n", derfile);
      exit (1);
    }
  size = fread (buffer, 1, sizeof (buffer), fd);
  if (size <= 0)
    {
      printf ("Cannot read from file %s\n", derfile);
      exit (1);
    }

  fclose (fd);

  result =
    asn1_create_element (definitions, "PKIX1.CertificateList", &asn1_element);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot create CRL element\n");
      exit (1);
    }

  result = asn1_der_decoding (&asn1_element, buffer, size, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot decode DER data (size %ld)\n", (long) size);
      exit (1);
    }

  result =
    asn1_der_decoding_startEnd (asn1_element, buffer, size, "tbsCertList",
				&start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End\n");
      exit (1);
    }
  if (start != 4 && end != 358)
    {
      printf ("Error in start and end values. Have: %d..%d\n", start, end);
      exit (1);
    }

  result =
    asn1_der_decoding_startEnd (asn1_element, buffer, size, "signature",
				&start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End\n");
      exit (1);
    }
  if (start != 372 && end != 503)
    {
      printf ("Error in start and end values for signature. Have: %d..%d\n",
	      start, end);
      exit (1);
    }

  result =
    asn1_der_decoding_startEnd (asn1_element, buffer, size,
				"tbsCertList.revokedCertificates.?1.userCertificate",
				&start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End\n");
      exit (1);
    }
  if (start != 326 && end != 343)
    {
      printf
	("Error in start and end values for userCertificate. Have: %d..%d\n",
	 start, end);
      exit (1);
    }

  size2 = sizeof (buffer2);
  result = asn1_der_coding (asn1_element, "", buffer2, &size2, NULL);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot encode data (size %ld)\n", (long) size);
      exit (1);
    }

  if (size2 != size || memcmp (buffer, buffer2, size) != 0)
    {
      printf ("DER encoded data differ!\n");
      exit (1);
    }

  /* Clear the definition structures */
  asn1_delete_structure (&definitions);
  asn1_delete_structure (&asn1_element);

  if (out != stdout)
    fclose (out);

  exit (0);
}
