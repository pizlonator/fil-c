/*
 * Copyright (C) 2014-2022 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <libtasn1.h>

int
main (int argc, char **argv)
{
  int result = 0;
  asn1_node definitions = NULL, node1 = NULL, node2 = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  const char *choicefile = getenv ("ASN1CODINGDECODING2");
  int start, end;

  char data[1024];
  int data_size = sizeof (data);

  if (!choicefile)
    choicefile = "coding-decoding2.asn";

  /* Encode */
  result = asn1_parser2tree (choicefile, &definitions, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_create_element (definitions, "TEST.Main", &node1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "pad1", "\x01\x02\x03", 3);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "pad2", "\x00\x01", 2);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "choice0", "choice1", 1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "choice0.choice1", "choice2", 1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "choice0.choice1.choice2", "int1", 1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result =
    asn1_write_value (node1, "choice0.choice1.choice2.int1", "1234", 0);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_der_coding (node1, "", data, &data_size, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  /*asn1_print_structure(stdout, node1, "", ASN1_PRINT_ALL); */

  result =
    asn1_der_decoding_startEnd (node1, data, data_size,
				"choice0.choice1.choice2.int1", &start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End: %d\n", __LINE__);
      exit (1);
    }
  if (start != 7 && end != 10)
    {
      printf
	("Error in start and end values for choice1.choice2.int1. Have: %d..%d\n",
	 start, end);
      exit (1);
    }

  result =
    asn1_der_decoding_startEnd (node1, data, data_size, "choice0.choice1",
				&start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End: %d\n", __LINE__);
      exit (1);
    }
  if (start != 7 && end != 10)
    {
      printf ("Error in start and end values for choice1. Have: %d..%d\n",
	      start, end);
      exit (1);
    }

  result =
    asn1_der_decoding_startEnd (node1, data, data_size, "pad2", &start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End\n");
      exit (1);
    }
  if (start != 11 && end != 13)
    {
      printf ("Error in start and end values for pad2. Have: %d..%d\n", start,
	      end);
      exit (1);
    }

  asn1_delete_structure (&node1);

  /* Decode */
  result = asn1_create_element (definitions, "TEST.Main", &node2);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

#if 0
  printf ("der:");
  for (i = 0; i < data_size; i++)
    printf ("%.2x ", (unsigned char) (data[i]));
  printf ("\n");
#endif

  result = asn1_der_decoding (&node2, data, data_size, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d: %s\n", __LINE__, errorDescription);
      exit (1);
    }

  result =
    asn1_der_decoding_startEnd (node2, data, data_size,
				"choice0.choice1.choice2.int1", &start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End: %d\n", __LINE__);
      exit (1);
    }
  if (start != 7 && end != 10)
    {
      printf
	("Error in start and end values for choice0.choice1.choice2.int1. Have: %d..%d\n",
	 start, end);
      exit (1);
    }

  result =
    asn1_der_decoding_startEnd (node2, data, data_size, "pad2", &start, &end);
  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("Cannot find start End: %d\n", __LINE__);
      exit (1);
    }
  if (start != 11 && end != 13)
    {
      printf ("Error in start and end values for pad2. Have: %d..%d\n", start,
	      end);
      exit (1);
    }

  asn1_delete_structure (&node2);
  asn1_delete_structure (&definitions);

  return 0;
}
