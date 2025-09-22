/*
 * Copyright (C) 2016 Red Hat, Inc.
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
#include <string.h>
#include <libtasn1.h>

int
main (int argc, char **argv)
{
  int result = 0, len;
  asn1_node definitions = NULL, node1 = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  const char *choicefile = getenv ("ASN1_MSCAT");
  const char *datafile = getenv ("ASN1_SPC_PE_IMAGE_DATA");
  FILE *fp;
  char data[1024];
  int data_size = sizeof (data);
  unsigned etype;
  int cmp;

  if (choicefile == NULL)
    {
      choicefile = "mscat.asn";
    }

  if (datafile == NULL)
    {
      datafile = "spc_pe_image_data.der";
    }

  result = asn1_parser2tree (choicefile, &definitions, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      fprintf (stderr, "error in %d: %s\n", __LINE__, errorDescription);
      exit (1);
    }

  result = asn1_create_element (definitions,
				"CATALOG.SpcPEImageData", &node1);
  if (result != ASN1_SUCCESS)
    {
      fprintf (stderr, "error in %d\n", __LINE__);
      exit (1);
    }

  fp = fopen (datafile, "rb");
  if (fp == NULL)
    {
      fprintf (stderr, "error in %d\n", __LINE__);
      exit (1);
    }
  data_size = fread (data, 1, sizeof (data), fp);
  fclose (fp);

  result = asn1_der_decoding2 (&node1,
			       data,
			       &data_size,
			       ASN1_DECODE_FLAG_STRICT_DER, errorDescription);
  if (result != ASN1_SUCCESS)
    {
      fprintf (stderr, "error in %d: decoding: %s\n", __LINE__,
	       errorDescription);
      exit (1);
    }

  len = 0;
  result = asn1_read_value_type (node1, "flags", NULL, &len, &etype);
  if (result != ASN1_MEM_ERROR)
    {
      fprintf (stderr, "error in %d: %s\n", __LINE__, errorDescription);
      exit (1);
    }

  if ((size_t) len >= sizeof (data))
    {
      fprintf (stderr,
	       "error len=%d, sizeof(data)=%zu\n", len, sizeof (data));
      exit (1);
    }

  result = asn1_read_value (node1, "flags", data, &len);
  if (result != ASN1_SUCCESS)
    {
      fprintf (stderr, "error in %d: %s\n", __LINE__, errorDescription);
      exit (1);
    }

  etype = 0;
  len = 0;
  result = asn1_read_value_type (node1, "link", NULL, &len, &etype);
  if (result != ASN1_MEM_ERROR)
    {
      fprintf (stderr,
	       "error in %d: result is: %s\n",
	       __LINE__, asn1_strerror (result));
      exit (1);
    }

  if (etype != ASN1_ETYPE_CHOICE)
    {
      fprintf (stderr,
	       "error in %d: The type (%u) doesn't match choice.\n",
	       __LINE__, etype);
      exit (1);
    }

  if (len != 5)
    {
      fprintf (stderr,
	       "length doesn't match the expected (got: %d, should be: 20)\n",
	       len);
      exit (1);
    }

  len = sizeof (data);
  result = asn1_read_value (node1, "link", data, &len);
  if (result != ASN1_SUCCESS)
    {
      fprintf (stderr, "error in %d: %s\n", __LINE__, errorDescription);
      exit (1);
    }

  cmp = memcmp (data, "file", len);
  if (cmp != 0)
    {
      fprintf (stderr, "contents don't match\n");
      exit (1);
    }

  printf ("SUCCESS\n");

  asn1_delete_structure (&node1);
  asn1_delete_structure (&definitions);

  return 0;
}
