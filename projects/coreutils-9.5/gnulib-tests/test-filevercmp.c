/* Test of filevercmp() function.
   Copyright (C) 2008-2024 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include "filevercmp.h"

#include <stddef.h>
#include <string.h>

#include "macros.h"

/* set of well sorted examples */
static const char *const examples[] =
{
  "",
  ".",
  "..",
  ".0",
  ".9",
  ".A",
  ".Z",
  ".a~",
  ".a",
  ".b~",
  ".b",
  ".z",
  ".zz~",
  ".zz",
  ".zz.~1~",
  ".zz.0",
  ".\1",
  ".\1.txt",
  ".\1x",
  ".\1x\1",
  ".\1.0",
  "0",
  "9",
  "A",
  "Z",
  "a~",
  "a",
  "a.b~",
  "a.b",
  "a.bc~",
  "a.bc",
  "a+",
  "a.",
  "a..a",
  "a.+",
  "b~",
  "b",
  "gcc-c++-10.fc9.tar.gz",
  "gcc-c++-10.fc9.tar.gz.~1~",
  "gcc-c++-10.fc9.tar.gz.~2~",
  "gcc-c++-10.8.12-0.7rc2.fc9.tar.bz2",
  "gcc-c++-10.8.12-0.7rc2.fc9.tar.bz2.~1~",
  "glibc-2-0.1.beta1.fc10.rpm",
  "glibc-common-5-0.2.beta2.fc9.ebuild",
  "glibc-common-5-0.2b.deb",
  "glibc-common-11b.ebuild",
  "glibc-common-11-0.6rc2.ebuild",
  "libstdc++-0.5.8.11-0.7rc2.fc10.tar.gz",
  "libstdc++-4a.fc8.tar.gz",
  "libstdc++-4.10.4.20040204svn.rpm",
  "libstdc++-devel-3.fc8.ebuild",
  "libstdc++-devel-3a.fc9.tar.gz",
  "libstdc++-devel-8.fc8.deb",
  "libstdc++-devel-8.6.2-0.4b.fc8",
  "nss_ldap-1-0.2b.fc9.tar.bz2",
  "nss_ldap-1-0.6rc2.fc8.tar.gz",
  "nss_ldap-1.0-0.1a.tar.gz",
  "nss_ldap-10beta1.fc8.tar.gz",
  "nss_ldap-10.11.8.6.20040204cvs.fc10.ebuild",
  "z",
  "zz~",
  "zz",
  "zz.~1~",
  "zz.0",
  "zz.0.txt",
  "\1",
  "\1.txt",
  "\1x",
  "\1x\1",
  "\1.0",
  "#\1.b#",
  "#.b#",
  NULL
};

/* Sets of examples that should all sort equally.  Each set is
   terminated by NULL.  */
static char const *const equals[] =
  {
    "a",
    "a0",
    "a0000",
    NULL,
    "a\1c-27.txt",
    "a\1c-027.txt",
    "a\1c-00000000000000000000000000000000000000000000000000000027.txt",
    NULL,
    ".a\1c-27.txt",
    ".a\1c-027.txt",
    ".a\1c-00000000000000000000000000000000000000000000000000000027.txt",
    NULL,
    "a\1c-",
    "a\1c-0",
    "a\1c-00",
    NULL,
    ".a\1c-",
    ".a\1c-0",
    ".a\1c-00",
    NULL,
    "a\1c-0.txt",
    "a\1c-00.txt",
    NULL,
    ".a\1c-1\1.txt",
    ".a\1c-001\1.txt",
    NULL,
  };

static int
sign (int i)
{
  return i < 0 ? -1 : 0 < i;
}

/* Return filevercmp (A, B), checking that a similar result is gotten
   after replacing all '\1's with '\0's and calling filenvercmp with
   the embedded '\0's.  */
static int
test_filevercmp (char const *a, char const *b)
{
  int result = filevercmp (a, b);

  char buffer[1000];

  ptrdiff_t alen = strlen (a), blen = strlen (b);
  ASSERT (alen + blen <= sizeof buffer);
  memcpy (buffer, a, alen);
  memcpy (buffer + alen, b, blen);
  for (ptrdiff_t i = 0; i < alen + blen; i++)
    if (buffer[i] == '\1')
      buffer[i] = '\0';

  int nresult = filenvercmp (buffer, alen, buffer + alen, blen);
  ASSERT (sign (nresult) == sign (result));

  return result;
}

int
main (void)
{
  const char *const *i;

  /* Following tests taken from test-strverscmp.c */
  ASSERT (filevercmp ("", "") == 0);
  ASSERT (filevercmp ("a", "a") == 0);
  ASSERT (filevercmp ("a", "b") < 0);
  ASSERT (filevercmp ("b", "a") > 0);
  ASSERT (filevercmp ("00", "01") < 0);
  ASSERT (filevercmp ("01", "010") < 0);
  ASSERT (filevercmp ("9", "10") < 0);
  ASSERT (filevercmp ("0a", "0") > 0);

  /* compare each version string with each other - O(n^2) */
  for (i = examples; *i; i++)
    {
      const char *const *j;
      for (j = examples; *j; j++)
        {
          int result = test_filevercmp (*i, *j);
          if (result < 0)
            ASSERT (i < j);
          else if (0 < result)
            ASSERT (j < i);
          else
            ASSERT (i == j);
        }
    }

  for (i = equals; i < equals + sizeof equals / sizeof *equals; i++)
    for (; *i; i++)
      for (char const *const *j = i; *j; j++)
        {
          ASSERT (test_filevercmp (*i, *j) == 0);
          ASSERT (test_filevercmp (*j, *i) == 0);
        }

  return 0;
}
