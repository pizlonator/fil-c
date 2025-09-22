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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libtasn1.h"



int
main (int argc, char *argv[])
{
  int result;
  char buffer[10 * 1024];
  asn1_node definitions = NULL;
  asn1_node asn1_element = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  FILE *out, *fd;
  int size;
  const char *treefile = getenv ("ASN1PKIX");
  const char *indeffile = getenv ("ASN1INDEF");
  int verbose = 0;

  if (argc > 1)
    verbose = 1;

  if (!treefile)
    treefile = "pkix.asn";

  if (!indeffile)
    indeffile = "TestIndef.p12";

  if (verbose)
    {
      printf ("\n\n/****************************************/\n");
      printf ("/*     Test sequence : strict DER decoding  */\n");
      printf ("/****************************************/\n\n");
      printf ("ASN1TREE: %s\n", treefile);
    }

  /* Check version */
  if (asn1_check_version ("0.2.11") == NULL)
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

  fd = fopen (indeffile, "rb");
  if (fd == NULL)
    {
      printf ("Cannot read file %s\n", indeffile);
      exit (1);
    }
  size = fread (buffer, 1, sizeof (buffer), fd);
  if (size <= 0)
    {
      printf ("Cannot read from file %s\n", indeffile);
      exit (1);
    }

  fclose (fd);

  result =
    asn1_create_element (definitions, "PKIX1.pkcs-12-PFX", &asn1_element);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot create PKCS12 element\n");
      exit (1);
    }

  result =
    asn1_der_decoding2 (&asn1_element, buffer, &size, 0, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot decode BER data (size %ld) in %s: %s\n", (long) size,
	      indeffile, errorDescription);
      exit (1);
    }

  asn1_delete_structure (&asn1_element);

  /* Now try with strict DER */
  result =
    asn1_create_element (definitions, "PKIX1.pkcs-12-PFX", &asn1_element);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot create PKCS12 element\n");
      exit (1);
    }

  result =
    asn1_der_decoding2 (&asn1_element, buffer, &size,
			ASN1_DECODE_FLAG_STRICT_DER, errorDescription);
  if (result == ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Should not have decoded DER data (size %ld) in %s: %s\n",
	      (long) size, indeffile, errorDescription);
      exit (1);
    }

  asn1_delete_structure (&asn1_element);

  /* Clear the definition structures */
  asn1_delete_structure (&definitions);

  if (out != stdout)
    fclose (out);

  exit (0);
}
