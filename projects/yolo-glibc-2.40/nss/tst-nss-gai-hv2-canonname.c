/* Test NSS query path for plugins that only implement gethostbyname2
   (#30843).
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
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <mcheck.h>
#include <support/check.h>
#include <support/xstdio.h>
#include "nss/tst-nss-gai-hv2-canonname.h"

#define PREPARE do_prepare

static void do_prepare (int a, char **av)
{
  FILE *hosts = xfopen ("/etc/hosts", "w");
  for (unsigned i = 2; i < 255; i++)
    {
      fprintf (hosts, "ff01::ff02:ff03:%u:2\ttest.example.com\n", i);
      fprintf (hosts, "192.168.0.%u\ttest.example.com\n", i);
    }
  xfclose (hosts);
}

static int
do_test (void)
{
  mtrace ();

  __nss_configure_lookup ("hosts", "test_gai_hv2_canonname");

  struct addrinfo hints = {};
  struct addrinfo *result = NULL;

  hints.ai_family = AF_INET6;
  hints.ai_flags = AI_ALL | AI_V4MAPPED | AI_CANONNAME;

  int ret = getaddrinfo (QUERYNAME, NULL, &hints, &result);

  if (ret != 0)
    FAIL_EXIT1 ("getaddrinfo failed: %s\n", gai_strerror (ret));

  TEST_COMPARE_STRING (result->ai_canonname, QUERYNAME);

  freeaddrinfo(result);
  return 0;
}

#include <support/test-driver.c>
