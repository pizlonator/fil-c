/* Basic test of __nss_get_default_domain
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <unistd.h>
#include <string.h>
#include <dlfcn.h>

#include "nss_compat/nisdomain.h"

#include <support/test-driver.h>
#include <support/support.h>
#include <support/namespace.h>
#include <support/check.h>

char unset_domain[] = "unset_domain";
char new_domain[] = "new_domain";

/* This function checks the __nss_get_default_domain() function in
   nss_compat/nssdomain.c.  Because this is an internal function to
   libnss_compat.so, the Makefile will link that object to this test
   case directly.  */

static int
do_test (void)
{
  char *domain_name;
  char buf[1024];

  /* We need to be in a network namespace so we can change the domain
     name without interfering with the host system.  */
  support_become_root ();
  support_enter_network_namespace ();
  if (!support_in_uts_namespace ())
    return EXIT_UNSUPPORTED;

  /* First pass: set an empty domain and make sure it's returned
     correctly.  This should not be cached.  */

  /* Set the domain name to a known value.  */
  TEST_VERIFY (setdomainname ("", 0) == 0);

  /* Make sure it got set.  */
  TEST_VERIFY (getdomainname (buf, sizeof(buf)) == 0);
  TEST_COMPARE_STRING (buf, "");

  /* Set this to a known "unknown" value so we can detect if it's not
     changed.  */
  domain_name = unset_domain;

  /* This is the function we're testing.  */
  TEST_VERIFY (__nss_get_default_domain (& domain_name) == 0);

  /* Make sure the correct domain name is returned.  */
  TEST_VERIFY (domain_name != NULL);
  TEST_COMPARE_STRING (domain_name, "");

  /* Second pass: set a non-empty domain and make sure it's returned
     correctly.  This works because the empty domain is not
     cached.  */

  /* Set the domain name to a known value.  */
  TEST_VERIFY (setdomainname (new_domain, strlen (new_domain)) == 0);

  /* Make sure it got set.  */
  TEST_VERIFY (getdomainname (buf, sizeof(buf)) == 0);
  TEST_COMPARE_STRING (buf, new_domain);

  /* Set this to a known "unknown" value so we can detect if it's not
     changed.  */
  domain_name = unset_domain;

  /* This is the function we're testing.  */
  TEST_VERIFY (__nss_get_default_domain (& domain_name) == 0);

  /* Make sure the correct domain name is returned.  */
  TEST_VERIFY (domain_name != NULL);
  TEST_COMPARE_STRING (domain_name, new_domain);

  /* The function caches the name, so check it twice.  */
  TEST_VERIFY (__nss_get_default_domain (& domain_name) == 0);

  TEST_VERIFY (domain_name != NULL);
  TEST_COMPARE_STRING (domain_name, new_domain);

  /* Third pass: set an empty domain again but expect the cached
     value.  */

  /* Set the domain name to a known value.  */
  TEST_VERIFY (setdomainname ("", 0) == 0);

  /* Make sure it got set.  */
  TEST_VERIFY (getdomainname (buf, sizeof(buf)) == 0);
  TEST_COMPARE_STRING (buf, "");

  /* Set this to a known "unknown" value so we can detect if it's not
     changed.  */
  domain_name = unset_domain;

  /* This is the function we're testing.  */
  TEST_VERIFY (__nss_get_default_domain (& domain_name) == 0);

  TEST_VERIFY (domain_name != NULL);
  TEST_COMPARE_STRING (domain_name, new_domain);

  return 0;
}

#include <support/test-driver.c>
