/* NSS service provider with errno clobber.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <errno.h>
#include <nss.h>
#include <stdlib.h>

/* Catch misnamed and functions.  */
#pragma GCC diagnostic error "-Wmissing-prototypes"
NSS_DECLARE_MODULE_FUNCTIONS (test_errno)

static void __attribute__ ((constructor))
init (void)
{
  /* An arbitrary error code which is otherwise not used.  */
  errno = -1009;
}

/* Lookup functions for pwd follow that do not return any data.  */

/* Catch misnamed function definitions.  */

enum nss_status
_nss_test_errno_setpwent (int stayopen)
{
  setenv ("_nss_test_errno_setpwent", "yes", 1);
  return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_test_errno_getpwent_r (struct passwd *result,
                            char *buffer, size_t size, int *errnop)
{
  setenv ("_nss_test_errno_getpwent_r", "yes", 1);
  return NSS_STATUS_NOTFOUND;
}

enum nss_status
_nss_test_errno_endpwent (void)
{
  setenv ("_nss_test_errno_endpwent", "yes", 1);
  return NSS_STATUS_SUCCESS;
}
