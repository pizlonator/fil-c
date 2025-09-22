/*
 * Copyright (C) 2012-2022 Free Software Foundation, Inc.
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
 * Written by Simon Josefsson
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libtasn1.h"

struct tv
{
  unsigned int etype;
  unsigned int str_len;
  const void *str;
  unsigned int der_len;
  const void *der;
};

static const struct tv tv[] = {
  {ASN1_ETYPE_IA5_STRING, 20,
   "\x63\x73\x63\x61\x40\x70\x61\x73\x73\x70\x6f\x72\x74\x2e\x67\x6f\x76\x2e\x67\x72",
   22,
   "\x16\x14\x63\x73\x63\x61\x40\x70\x61\x73\x73\x70\x6f\x72\x74\x2e\x67\x6f\x76\x2e\x67\x72"},
  {ASN1_ETYPE_PRINTABLE_STRING, 5, "\x4e\x69\x6b\x6f\x73",
   7, "\x13\x05\x4e\x69\x6b\x6f\x73"},
  {ASN1_ETYPE_UTF8_STRING, 12, "Αττική",
   14, "\x0c\x0c\xce\x91\xcf\x84\xcf\x84\xce\xb9\xce\xba\xce\xae"},
  {ASN1_ETYPE_TELETEX_STRING, 15,
   "\x53\x69\x6d\x6f\x6e\x20\x4a\x6f\x73\x65\x66\x73\x73\x6f\x6e",
   17,
   "\x14\x0f\x53\x69\x6d\x6f\x6e\x20\x4a\x6f\x73\x65\x66\x73\x73\x6f\x6e"},
  {ASN1_ETYPE_OCTET_STRING, 36,
   "\x30\x22\x80\x0F\x32\x30\x31\x31\x30\x38\x32\x31\x30\x38\x30\x30\x30\x36\x5A\x81\x0F\x32\x30\x31\x31\x30\x38\x32\x33\x32\x30\x35\x39\x35\x39\x5A",
   38,
   "\x04\x24\x30\x22\x80\x0F\x32\x30\x31\x31\x30\x38\x32\x31\x30\x38\x30\x30\x30\x36\x5A\x81\x0F\x32\x30\x31\x31\x30\x38\x32\x33\x32\x30\x35\x39\x35\x39\x5A"}
};

#define SSTR(x) sizeof(x)-1,x
static const struct tv ber[] = {
  {ASN1_ETYPE_OCTET_STRING,
   SSTR ("\xa0\xa0"),
   SSTR ("\x24\x80\x04\x82\x00\x02\xa0\xa0\x00\x00")},
  {ASN1_ETYPE_OCTET_STRING,
   SSTR ("\xa0\xa0\xb0\xb0\xb0"),
   SSTR
   ("\x24\x80\x04\x82\x00\x02\xa0\xa0\x04\x82\x00\x03\xb0\xb0\xb0\x00\x00")},
  {ASN1_ETYPE_OCTET_STRING,
   SSTR ("\xa0\xa0\xb0\xb0\xb0\xa1\xa1"),
   SSTR
   ("\x24\x80\x04\x82\x00\x02\xa0\xa0\x04\x82\x00\x03\xb0\xb0\xb0\x24\x80\x04\x82\x00\x02\xa1\xa1\x00\x00\x00\x00")},
  {ASN1_ETYPE_OCTET_STRING,
   SSTR ("\xa0\xa0\xb0\xb0\xb0\xa1\xa1\xc1"),
   SSTR
   ("\x24\x80\x04\x82\x00\x02\xa0\xa0\x04\x82\x00\x03\xb0\xb0\xb0\x24\x80\x04\x82\x00\x02\xa1\xa1\x04\x82\x00\x01\xc1\x00\x00\x00\x00")},
};

int
main (int argc, char *argv[])
{
  int ret;
  unsigned char tl[ASN1_MAX_TL_SIZE];
  unsigned int tl_len, der_len, str_len;
  const unsigned char *str;
  unsigned char *b;
  unsigned int i;

  /* Dummy test */

  for (i = 0; i < sizeof (tv) / sizeof (tv[0]); i++)
    {
      /* Encode */
      tl_len = sizeof (tl);
      ret = asn1_encode_simple_der (tv[i].etype, tv[i].str, tv[i].str_len,
				    tl, &tl_len);
      if (ret != ASN1_SUCCESS)
	{
	  fprintf (stderr, "Encoding error in %u: %s\n", i,
		   asn1_strerror (ret));
	  return 1;
	}
      der_len = tl_len + tv[i].str_len;

      if (der_len != tv[i].der_len || memcmp (tl, tv[i].der, tl_len) != 0)
	{
	  fprintf (stderr,
		   "DER encoding differs in %u! (size: %u, expected: %u)\n",
		   i, der_len, tv[i].der_len);
	  return 1;
	}

      /* decoding */
      ret =
	asn1_decode_simple_der (tv[i].etype, tv[i].der, tv[i].der_len, &str,
				&str_len);
      if (ret != ASN1_SUCCESS)
	{
	  fprintf (stderr, "Decoding error in %u: %s\n", i,
		   asn1_strerror (ret));
	  return 1;
	}

      if (str_len != tv[i].str_len || memcmp (str, tv[i].str, str_len) != 0)
	{
	  fprintf (stderr,
		   "DER decoded data differ in %u! (size: %u, expected: %u)\n",
		   i, der_len, tv[i].str_len);
	  return 1;
	}
    }

  /* BER decoding */
  for (i = 0; i < sizeof (ber) / sizeof (ber[0]); i++)
    {
      /* decoding */
      ret =
	asn1_decode_simple_ber (ber[i].etype, ber[i].der, ber[i].der_len, &b,
				&str_len, NULL);
      if (ret != ASN1_SUCCESS)
	{
	  fprintf (stderr, "BER decoding error in %u: %s\n", i,
		   asn1_strerror (ret));
	  return 1;
	}

      if (str_len != ber[i].str_len || memcmp (b, ber[i].str, str_len) != 0)
	{
	  fprintf (stderr,
		   "BER decoded data differ in %u! (size: %u, expected: %u)\n",
		   i, str_len, ber[i].str_len);
	  return 1;
	}
      free (b);
    }


  return 0;
}
