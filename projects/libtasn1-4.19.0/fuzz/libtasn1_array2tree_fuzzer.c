/*
 * Copyright(c) 2019 Free Software Foundation, Inc.
 *
 * This file is part of libtasn1.
 *
 * Libtasn1 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Libtasn1 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libtasn1.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This fuzzer is testing asn1_array2tree()'s robustness with arbitrary
 * input data.
 */

#include <config.h>

#include <assert.h>		/* assert */
#include <stdlib.h>		/* malloc, free */
#include <string.h>		/* memcpy */

#include "libtasn1.h"
#include "fuzzer.h"

const asn1_static_node pkix_asn1_tab[] = {
  {"PKIX1Implicit88", 536875024, NULL},
  {NULL, 0, NULL}
};

#define NAMESIZE  20
#define VALUESIZE 20
struct fuzz_elem
{
  unsigned int type;
  char name[NAMESIZE];
  char value[VALUESIZE];
};

#define MAXELEM 100
#define MAXDATASIZE (100 * sizeof(struct fuzz_elem))

int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
  if (size > MAXDATASIZE)	/* same as max_len = <MAXDATASIZE> in .options file */
    return 0;

  struct fuzz_elem *elem = (struct fuzz_elem *) malloc (size);
  assert (elem != NULL);
  memcpy (elem, data, size);

  int nelem = size / sizeof (struct fuzz_elem);
  asn1_static_node tab[MAXELEM + 1];	/* avoid VLA here */
  int it;

  for (it = 0; it < nelem; it++)
    {
      tab[it].type = elem[it].type;
      elem[it].name[NAMESIZE - 1] = 0;
      if (strcmp (elem[it].name, "NULL"))
	tab[it].name = elem[it].name;
      else
	tab[it].name = NULL;
      elem[it].value[VALUESIZE - 1] = 0;
      if (strcmp (elem[it].value, "NULL"))
	tab[it].value = elem[it].value;
      else
	tab[it].value = NULL;
    }

  /* end-of-array indicator */
  tab[nelem].type = 0;
  tab[nelem].name = NULL;
  tab[nelem].value = NULL;

  int result;
  asn1_node node = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];

  result = asn1_array2tree (tab, &node, errorDescription);

  if (result == ASN1_SUCCESS)
    asn1_delete_structure (&node);

  free (elem);

  return 0;
}
