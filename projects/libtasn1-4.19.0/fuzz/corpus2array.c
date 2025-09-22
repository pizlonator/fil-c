/*
 * Copyright (C) 2019-2022 Free Software Foundation, Inc.
 *
 * This file is part of LIBTASN1.
 *
 * The LIBTASN1 library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

/*
 Helper tool to convert a corpus from libtasn1_array2tree_fuzzer into a asn1_static_node C code.

 Example usage:
   $ cat libtasn1_array2tree_fuzzer.in/e04b8bc97fcf0d6386dfa44e32b1700e42e0eb64 | ./corpus2array
   const asn1_static_node tab[] = {
     { "999=999", CONST_UNIVERSAL|CONST_IMPLICIT|CONST_SET|CONST_ASSIGN|CONST_DOWN, "993\xff~\xff$y\xd9\x80" },
     { NULL, 0, NULL }
   };   cat leak-d69d42ff01f6c3acca35dee27538a99e6821fced | ./corpus2array
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

struct fuzz_elem
{
  unsigned int type;
  char name[20];
  char value[20];
};

static char *
escape (unsigned char *s)
{
  static char out[19 * 6 + 1];
  char *p = out;

  while (*s)
    {
      if (*s > 127)
	{
	  if (isxdigit (s[1]))
	    {
	      sprintf (p, "\\u%04x", *s++);
	      p += 6;
	    }
	  else
	    {
	      sprintf (p, "\\x%02x", *s++);
	      p += 4;
	    }
	}
      else if (*s == '\n')
	{
	  sprintf (p, "\\n");
	  p += 2;
	  s++;
	}
      else if (*s == '\t')
	{
	  sprintf (p, "\\t");
	  p += 2;
	  s++;
	}
      else
	*p++ = *s++;
    }
  *p = 0;

  return out;
}

static const char *typename[24] = {
  "CONST_UNIVERSAL",
  "CONST_PRIVATE",
  "CONST_APPLICATION",
  "CONST_EXPLICIT",
  "CONST_IMPLICIT",
  "CONST_TAG",
  "CONST_OPTION",
  "CONST_DEFAULT",
  "CONST_TRUE",
  "CONST_FALSE",
  "CONST_LIST",
  "CONST_MIN_MAX",
  "CONST_1_PARAM",
  "CONST_SIZE",
  "CONST_DEFINED_BY",
  "CONST_GENERALIZED",
  "CONST_UTC",
  NULL,				/* #define CONST_IMPORTS     (1<<25) */
  "CONST_NOT_USED",
  "CONST_SET",
  "CONST_ASSIGN",
  "CONST_DOWN",
  "CONST_RIGHT",
  NULL
};

int
main (void)
{
  struct fuzz_elem e;

  printf ("const asn1_static_node tab[] = {\n");

  while (fread (&e, sizeof (e), 1, stdin) == 1)
    {
      e.name[sizeof (e.name) - 1] = 0;
      e.value[sizeof (e.value) - 1] = 0;
      if (strcmp (e.name, "NULL"))
	printf ("  { \"%s\"", escape ((unsigned char *) e.name));
      else
	printf ("  { NULL");

      if (e.type)
	{
	  int add = 0;
	  int i;

	  /* we leave the lowest 8 bit out */
	  if ((e.type & 0xFF) == 17)
	    {
	      printf (", ASN1_ETYPE_TIME");
	      add = 1;
	    }

	  for (i = 8; i < 32; i++)
	    {
	      if ((e.type & (1U << i)) && typename[i - 8])
		{
		  printf (add ? "|%s" : ", %s", typename[i - 8]);
		  add = 1;
		}
	    }

	  if (!add)
	    printf (", %u", e.type);
	}
      else
	printf (", 0");

      if (strcmp (e.value, "NULL"))
	printf (", \"%s\" },\n", escape ((unsigned char *) e.value));
      else
	printf (", NULL },");
    }

  printf ("  { NULL, 0, NULL }\n");
  printf ("};\n");

  return 0;
}
