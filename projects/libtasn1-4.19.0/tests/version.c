/* version.c --- Version handling self tests.
 * Copyright (C) 2019-2022 Free Software Foundation, Inc.
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

#include <stdio.h>		/* printf */
#include <stdlib.h>		/* EXIT_SUCCESS */
#include <string.h>		/* strcmp */

/* Get ASN1 prototypes. */
#include <libtasn1.h>

int
main (int argc, char *argv[])
{
  int exit_code = EXIT_SUCCESS;
  char out[100];
  int j;
  unsigned gvn = ASN1_VERSION_NUMBER;
  unsigned gvmmp = (ASN1_VERSION_MAJOR << 16)
    + (ASN1_VERSION_MINOR << 8) + ASN1_VERSION_PATCH;

  printf ("ASN1_VERSION: %s\n", ASN1_VERSION);
  printf ("ASN1_VERSION_MAJOR: %d\n", ASN1_VERSION_MAJOR);
  printf ("ASN1_VERSION_MINOR: %d\n", ASN1_VERSION_MINOR);
  printf ("ASN1_VERSION_PATCH: %d\n", ASN1_VERSION_PATCH);
  printf ("ASN1_VERSION_NUMBER: %x\n", gvn);
  printf ("(ASN1_VERSION_MAJOR << 16) + (ASN1_VERSION_MINOR << 8)"
	  " + ASN1_VERSION_PATCH: %x\n", gvmmp);

  j = snprintf (out, sizeof (out) - 1, "%d.%d.%d", ASN1_VERSION_MAJOR,
		ASN1_VERSION_MINOR, ASN1_VERSION_PATCH);
  if (j < 0)
    {
      printf ("snprintf failure: %d", j);
      exit_code = EXIT_FAILURE;
      out[0] = '\0';
    }

  if (out[0])
    printf ("ASN1_VERSION_MAJOR.ASN1_VERSION_MINOR.ASN1_VERSION_PATCH: %s\n",
	    out);

  printf ("asn1_check_version (NULL): %s\n", asn1_check_version (NULL));

  if (!asn1_check_version (ASN1_VERSION))
    {
      printf ("FAIL: asn1_check_version (ASN1_VERSION)\n");
      exit_code = EXIT_FAILURE;
    }

  if (!asn1_check_version ("4.17.0"))
    {
      printf ("asn1_check_version(4.17.0) failure\n");
      exit_code = EXIT_FAILURE;
    }

  if (!asn1_check_version ("4.17"))
    {
      printf ("asn1_check_version(4.17) failure\n");
      exit_code = EXIT_FAILURE;
    }

  if (strcmp (ASN1_VERSION, asn1_check_version (NULL)) != 0)
    {
      printf ("FAIL: strcmp (ASN1_VERSION, asn1_check_version (NULL))\n");
      exit_code = EXIT_FAILURE;
    }

  if (ASN1_VERSION_NUMBER != gvn)
    {
      printf ("FAIL: ASN1_VERSION_NUMBER != gvn\n");
      exit_code = EXIT_FAILURE;
    }

  if (out[0])
    {
      if (!asn1_check_version (out))
	{
	  printf ("FAIL: asn1_check_version(%s)\n", out);
	  exit_code = EXIT_FAILURE;
	}

      /* ASN1_VERSION may look like "1.0.4.10-b872" but the derived string
         should be "1.0.4" anyway.  */
      if (strncmp (ASN1_VERSION, out, strlen (out)) != 0)
	{
	  printf ("FAIL: strncmp (ASN1_VERSION, %s, strlen (%s))\n", out,
		  out);
	  exit_code = EXIT_FAILURE;
	}
    }

  if (asn1_check_version ("4711.42.23"))
    {
      printf ("FAIL: asn1_check_version(4711.42.23)\n");
      exit_code = EXIT_FAILURE;
    }

  if (asn1_check_version ("UNKNOWN"))
    {
      printf ("FAIL: asn1_check_version (UNKNOWN)\n");
      exit_code = EXIT_FAILURE;
    }

  return exit_code;
}
