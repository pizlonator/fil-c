/*
 * Copyright (C) 2019 Nikos Mavrogiannopoulos
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

struct tv
{
  int der_len;
  const unsigned char *der;
  const char *oid;
  int expected_error;
};

static const struct tv tv[] = {
  {.der_len = 0,
   .der = (void *) "",
   .oid = "5.999.3",
   .expected_error = ASN1_VALUE_NOT_VALID	/* cannot start with 5 */
   },
  {.der_len = 0,
   .der = (void *) "",
   .oid = "0.48.9",
   .expected_error = ASN1_VALUE_NOT_VALID	/* second field cannot be 48 */
   },
  {.der_len = 0,
   .der = (void *) "",
   .oid = "1.40.9",
   .expected_error = ASN1_VALUE_NOT_VALID	/* second field cannot be 40 */
   },
  {.der_len = 4,
   .der = (void *) "\x06\x02\x4f\x63",
   .oid = "1.39.99",
   .expected_error = ASN1_SUCCESS,
   },
  {.der_len = 6,
   .der = (void *) "\x06\x04\x01\x02\x03\x04",
   .oid = "0.1.2.3.4",
   .expected_error = ASN1_SUCCESS},
  {.der_len = 5,
   .der = (void *) "\x06\x03\x51\x02\x03",
   .oid = "2.1.2.3",
   .expected_error = ASN1_SUCCESS},
  {.der_len = 5,
   .der = (void *) "\x06\x03\x88\x37\x03",
   .oid = "2.999.3",
   .expected_error = ASN1_SUCCESS},
  {.der_len = 12,
   .der = (void *) "\x06\x0a\x2b\x06\x01\x04\x01\x92\x08\x09\x05\x01",
   .oid = "1.3.6.1.4.1.2312.9.5.1",
   .expected_error = ASN1_SUCCESS},
  {.der_len = 19,
   .der =
   (void *)
   "\x06\x11\xfa\x80\x00\x00\x00\x0e\x01\x0e\xfa\x80\x00\x00\x00\x0e\x63\x6f\x6d",
   .oid = "2.1998768.0.0.14.1.14.1998848.0.0.14.99.111.109",
   .expected_error = ASN1_SUCCESS},
  {.der_len = 19,
   .der =
   (void *)
   "\x06\x11\x2b\x06\x01\x04\x01\x92\x08\x09\x02\xaa\xda\xbe\xbe\xfa\x72\x01\x07",
   .oid = "1.3.6.1.4.1.2312.9.2.1467399257458.1.7",
   .expected_error = ASN1_SUCCESS},
};

int
main (int argc, char *argv[])
{
  unsigned char der[128];
  int ret, der_len, i, j;

  for (i = 0; i < (int) (sizeof (tv) / sizeof (tv[0])); i++)
    {
      der_len = sizeof (der);
      ret = asn1_object_id_der (tv[i].oid, der, &der_len, 0);
      if (ret != ASN1_SUCCESS)
	{
	  if (ret == tv[i].expected_error)
	    continue;
	  fprintf (stderr,
		   "%d: iter %lu, encoding of OID failed: %s\n",
		   __LINE__, (unsigned long) i, asn1_strerror (ret));
	  return 1;
	}
      else if (ret != tv[i].expected_error)
	{
	  fprintf (stderr,
		   "%d: iter %lu, encoding of OID %s succeeded when expecting failure\n",
		   __LINE__, (unsigned long) i, tv[i].oid);
	  return 1;
	}

      if (der_len != tv[i].der_len || memcmp (der, tv[i].der, der_len) != 0)
	{
	  fprintf (stderr,
		   "%d: iter %lu, re-encoding of OID %s resulted to different string (%d vs %d bytes)\n",
		   __LINE__, (unsigned long) i, tv[i].oid, der_len,
		   tv[i].der_len);
	  fprintf (stderr, "\nGot:\t\t");
	  for (j = 0; j < der_len; j++)
	    fprintf (stderr, "%.2x", der[j]);

	  fprintf (stderr, "\nExpected:\t");
	  for (j = 0; j < tv[i].der_len; j++)
	    fprintf (stderr, "%.2x", tv[i].der[j]);
	  fprintf (stderr, "\n");

	  return 1;
	}
    }

  return 0;
}
