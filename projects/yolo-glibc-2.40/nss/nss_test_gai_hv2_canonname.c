/* NSS service provider that only provides gethostbyname2_r.
   Copyright The GNU Toolchain Authors.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <nss.h>
#include <stdlib.h>
#include <string.h>
#include "nss/tst-nss-gai-hv2-canonname.h"

/* Catch misnamed and functions.  */
#pragma GCC diagnostic error "-Wmissing-prototypes"
NSS_DECLARE_MODULE_FUNCTIONS (test_gai_hv2_canonname)

extern enum nss_status _nss_files_gethostbyname2_r (const char *, int,
						    struct hostent *, char *,
						    size_t, int *, int *);

enum nss_status
_nss_test_gai_hv2_canonname_gethostbyname2_r (const char *name, int af,
					      struct hostent *result,
					      char *buffer, size_t buflen,
					      int *errnop, int *herrnop)
{
  return _nss_files_gethostbyname2_r (name, af, result, buffer, buflen, errnop,
				      herrnop);
}

enum nss_status
_nss_test_gai_hv2_canonname_getcanonname_r (const char *name, char *buffer,
					    size_t buflen, char **result,
					    int *errnop, int *h_errnop)
{
  /* We expect QUERYNAME, which is a small enough string that it shouldn't fail
     the test.  */
  if (memcmp (QUERYNAME, name, sizeof (QUERYNAME))
      || buflen < sizeof (QUERYNAME))
    abort ();

  strncpy (buffer, name, buflen);
  *result = buffer;
  return NSS_STATUS_SUCCESS;
}
