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
  const char *choicefile = getenv ("ASN1CHOICE");

  char data[1024];
  int data_size = sizeof (data);

  if (!choicefile)
    choicefile = "pkix.asn";

  /* Encode */
  result = asn1_parser2tree (choicefile, &definitions, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_create_element (definitions, "TEST.Choice0", &node1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "", "choice1", 1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "choice1", "choice2", 1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "choice1.choice2", "int1", 1);
  if (result != ASN1_SUCCESS)
    {
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  result = asn1_write_value (node1, "choice1.choice2.int1", "1234", 0);
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

  asn1_delete_structure (&node1);

  /* Decode */
  result = asn1_create_element (definitions, "TEST.Choice0", &node2);
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
      printf ("error in %d\n", __LINE__);
      exit (1);
    }

  asn1_delete_structure (&node2);
  asn1_delete_structure (&definitions);

  return 0;
}
