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

/****************************************************************/
/* File: Test_setof.c                                           */
/* Description: Test writing of SET OF values and DER encoding. */
/****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "libtasn1.h"

static unsigned char expected_der[] = {
  0x31, 0x40, 0x30, 0x03, 0x04, 0x01, 0x01, 0x30,
  0x03, 0x04, 0x01, 0x05, 0x30, 0x04, 0x04, 0x02,
  0x00, 0x02, 0x30, 0x06, 0x04, 0x04, 0x00, 0x00,
  0x00, 0x00, 0x30, 0x06, 0x04, 0x04, 0x00, 0x00,
  0x00, 0x01, 0x30, 0x06, 0x04, 0x04, 0x00, 0x00,
  0x00, 0x02, 0x30, 0x06, 0x04, 0x04, 0x00, 0x01,
  0x00, 0x00, 0x30, 0x06, 0x04, 0x04, 0x01, 0x00,
  0x00, 0x00, 0x30, 0x06, 0x04, 0x04, 0x01, 0x01,
  0x00, 0x00
};

static unsigned char data[1024];
int data_size = sizeof (data);


int
main (int argc, char *argv[])
{
  int result, verbose = 0;
  asn1_node definitions = NULL;
  asn1_node asn1_element = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  const char *treefile = getenv ("ASN1SETOF");
  unsigned i;

  if (argc > 1)
    verbose = 1;

  if (!treefile)
    treefile = "setof.asn";

  if (verbose != 0)
    {
      printf ("\n\n/****************************************/\n");
      printf ("/*     Test sequence : coding-decoding  */\n");
      printf ("/****************************************/\n\n");
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

  result = asn1_create_element (definitions, "TEST.Set", &asn1_element);
  if (result != ASN1_SUCCESS)
    {
      fprintf (stderr, "asn1_create_element(): ");
      asn1_perror (result);
      exit (1);
    }

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "?LAST.val", "\x00\x02", 2);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result =
    asn1_write_value (asn1_element, "?LAST.val", "\x00\x01\x00\x00", 4);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result =
    asn1_write_value (asn1_element, "?LAST.val", "\x00\x00\x00\x00", 4);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result =
    asn1_write_value (asn1_element, "?LAST.val", "\x00\x00\x00\x02", 4);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result =
    asn1_write_value (asn1_element, "?LAST.val", "\x00\x00\x00\x01", 4);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result =
    asn1_write_value (asn1_element, "?LAST.val", "\x01\x00\x00\x00", 4);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result =
    asn1_write_value (asn1_element, "?LAST.val", "\x01\x01\x00\x00", 4);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "?LAST.val", "\x05", 1);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "", "NEW", 1);
  assert (result == ASN1_SUCCESS);

  result = asn1_write_value (asn1_element, "?LAST.val", "\x01", 1);
  assert (result == ASN1_SUCCESS);

  /* Clear the definition structures */

  result = asn1_der_coding (asn1_element, "", data, &data_size, NULL);
  if (result != ASN1_SUCCESS)
    {
      fprintf (stderr, "Encoding error.\n");
      asn1_perror (result);
      exit (1);
    }

  asn1_delete_structure (&asn1_element);
  asn1_delete_structure (&definitions);

  if (data_size != sizeof (expected_der)
      || memcmp (data, expected_der, data_size) != 0)
    {
      fprintf (stderr, "encoded data differ to expected [%d - %d]!\n",
	       data_size, (int) sizeof (expected_der));
      printf ("static unsigned char got[] = {\n");
      for (i = 0; i < (unsigned) data_size; i++)
	{
	  printf ("0x%.2x, ", (unsigned) data[i]);
	  if ((i + 1) % 8 == 0)
	    printf ("\n");
	}
      printf ("};\n");

      printf ("static unsigned char expected[] = {\n");
      for (i = 0; i < (unsigned) sizeof (expected_der); i++)
	{
	  printf ("0x%.2x, ", (unsigned) expected_der[i]);
	  if ((i + 1) % 8 == 0)
	    printf ("\n");
	}
      printf ("};\n");
      exit (1);
    }

  if (verbose)
    printf ("Success\n");
  exit (0);
}
