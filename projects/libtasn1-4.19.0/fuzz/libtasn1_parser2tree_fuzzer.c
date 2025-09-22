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
 * This fuzzer is testing asn1_parser2tree()'s robustness with arbitrary ASN.1
 * input data.
 */

#include <config.h>

#include <stdlib.h>		/* malloc, free */
#include <string.h>		/* strcmp, memcpy */

#include "libtasn1.h"
#include "fuzzer.h"

static const uint8_t *g_data;
static size_t g_size;

#if defined HAVE_DLFCN_H && defined HAVE_FMEMOPEN
# include <dlfcn.h>
# ifdef RTLD_NEXT		/* Not defined e.g. on CygWin */

FILE *
fopen (const char *pathname, const char *mode)
{
  FILE *(*libc_fopen) (const char *, const char *) =
    (FILE * (*)(const char *, const char *)) dlsym (RTLD_NEXT, "fopen");

  if (!strcmp (pathname, "pkix.asn"))
    return fmemopen ((void *) g_data, g_size, mode);

  return libc_fopen (pathname, mode);
}
# endif
#endif

int
LLVMFuzzerTestOneInput (const uint8_t * data, size_t size)
{
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  asn1_node definitions = NULL;

  if (size > 10000)		/* same as max_len = 10000 in .options file */
    return 0;

  g_data = data;
  g_size = size;

  int rc = asn1_parser2tree ("pkix.asn", &definitions, errorDescription);
  if (rc == ASN1_SUCCESS)
    {
      asn1_delete_structure (&definitions);
    }

  return 0;
}
