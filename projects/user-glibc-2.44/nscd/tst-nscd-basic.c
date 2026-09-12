/* Fundamental tests for nscd functionality.
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

#include <array_length.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/check_nss.h>
#include <support/namespace.h>
#include <support/nscd_test.h>
#include <support/support.h>
#include <support/xmemstream.h>
#include <support/xstdio.h>

/* Large GECOS string for the large-gecos user.  */
static char *large_gecos;

/* Write bulk data to NSS files under /etc.  */
static void
setup_files (void)
{
  {
    large_gecos = xasprintf ("large_gecos%0999d", 1234);
    FILE *fp = xfopen ("/etc/passwd", "a");
    fprintf (fp, "large-gecos:x:998:998:%s:/var/empty:/bin/bash\n",
             large_gecos);
    xfclose (fp);
  }

  {
    FILE *fp = xfopen ("/etc/group", "a");

    /* large-group has many members.  Bumping the 1999 limit to 9999
       triggers an issue related to bug 33460 (short write in nscd).  */
    fputs ("large-group:x:20017:large-group-user-0", fp);
    for (int i = 1; i <= 1999; ++i)
      fprintf (fp, ",large-group-user-%d", i);
    fputc ('\n', fp);

    /* user3 has many groups.  */
    for (int i = 1002; i <= 9999; ++i)
      fprintf (fp, "user3-group%d:x:%d:user3\n", i, i);

    xfclose (fp);
  }
}

static void
invalidate_noop (const char *database)
{
}

/* Assign support_nscd_invalidate to enable invalidation.   */
static void (*invalidate) (const char *database) = invalidate_noop;

/* Standard error string for lookup failures.   */
static const char not_found[] = "error: (errno 0, Success)\n";

static void
test_passwd (void)
{
  {
    const char *expected =
      "name: root\n"
      "passwd: x\n"
      "uid: 0\n"
      "gid: 0\n"
      "gecos: Super User\n"
      "dir: /root\n"
      "shell: /bin/bash\n"
      ;
    invalidate ("passwd");
    check_passwd ("root by name", getpwnam ("root"), expected);
    invalidate ("passwd");
    check_passwd ("root by uid", getpwuid (0), expected);
  }

  {
    const char *expected =
      "name: bin\n"
      "passwd: x\n"
      "uid: 1\n"
      "gid: 1\n"
      "gecos: bin\n"
      "dir: /bin\n"
      "shell: /usr/sbin/nologin\n"
      ;
    invalidate ("passwd");
    check_passwd ("bin by name", getpwnam ("bin"), expected);
    invalidate ("passwd");
    check_passwd ("bin by uid", getpwuid (1), expected);
  }

  {
    const char *expected =
      "name: adm\n"
      "passwd: x\n"
      "uid: 3\n"
      "gid: 4\n"
      "gecos: adm\n"
      "dir: /var/adm\n"
      "shell: /usr/sbin/nologin\n"
      ;
    invalidate ("passwd");
    check_passwd ("adm by name", getpwnam ("adm"), expected);
    invalidate ("passwd");
    check_passwd ("adm by uid", getpwuid (3), expected);
  }

  {
    char *expected = xasprintf
      ("name: large-gecos\n"
       "passwd: x\n"
       "uid: 998\n"
       "gid: 998\n"
       "gecos: %s\n"
       "dir: /var/empty\n"
       "shell: /bin/bash\n",
       large_gecos);
    invalidate ("passwd");
    check_passwd ("large-gecos by name", getpwnam ("large-gecos"), expected);
    invalidate ("passwd");
    check_passwd ("large-gecos by uid", getpwuid (998), expected);
    free (expected);
  }

  invalidate ("passwd");
  errno = 0;
  check_passwd ("missing by name", getpwnam ("missing"), not_found);
  invalidate ("passwd");
  errno = 0;
  check_passwd ("missing by uid", getpwuid (999), not_found);
}

static void
test_group (void)
{
  {
    const char *expected =
      "name: root\n"
      "passwd: x\n"
      "gid: 0\n"
      ;
    invalidate ("group");
    check_group ("root by name", getgrnam ("root"), expected);
    invalidate ("group");
    check_group ("root by gid", getgrgid (0), expected);
  }

  {
    const char *expected =
      "name: wheel\n"
      "passwd: x\n"
      "gid: 10\n"
      "member: user1\n"
      "member: user2\n"
      ;
    invalidate ("group");
    check_group ("wheel by name", getgrnam ("wheel"), expected);
    invalidate ("group");
    check_group ("wheel by gid", getgrgid (10), expected);
  }

  {
    struct xmemstream mem;
    xopen_memstream (&mem);
    fputs ("name: large-group\n"
           "passwd: x\n"
           "gid: 20017\n",
           mem.out);
    for (int i = 0; i <= 1999; ++i)
      fprintf (mem.out, "member: large-group-user-%d\n", i);
    xfclose_memstream (&mem);

    invalidate ("group");
    check_group ("large-group by name", getgrnam ("large-group"), mem.buffer);
    invalidate ("group");
    check_group ("large-group by gid", getgrgid (20017), mem.buffer);
    free (mem.buffer);
  }

  invalidate ("group");
  errno = 0;
  check_group ("missing by name", getgrnam ("missing"), not_found);
  invalidate ("group");
  errno = 0;
  check_group ("missing by gid", getgrgid (999), not_found);
}

static void
test_grouplist (void)
{
  gid_t groups[10000];
  {
    int n = array_length (groups);
    TEST_COMPARE (getgrouplist ("user1", 1000, groups, &n), 3);
    TEST_COMPARE (n, 3);
    bool found[] = { [1000] = false };
    for (int i = 0; i < n; ++i)
      if (groups[i] < array_length (found))
        found[groups[i]] = true;
    TEST_VERIFY (found[10]);
    TEST_VERIFY (found[135]);
    TEST_VERIFY (found[1000]);
  }
  {
    int n = array_length (groups);
    int expected = 9999 - 1002 + 1;
    TEST_COMPARE (getgrouplist ("user3", 1500, groups, &n), expected);
    TEST_COMPARE (n, expected);
    bool found[] = { [10000] = false };
    for (int i = 0; i < n; ++i)
      if (groups[i] < array_length (found))
        found[groups[i]] = true;
    for (int i = 1002; i <= 9999; ++i)
      TEST_VERIFY (found[i]);
  }
}

static void
test_hosts (void)
{
  {
    /* See gethostbyname3_multi in nss_files/files-hosts.c regarding
       the duplication here.  There is also code earlier in the file
       that converts ::1 to 127.0.0.1.  */
    const char *expected =
      "name: localhost\n"
      "alias: localhost.localdomain\n"
      "alias: localhost.localdomain\n"
      "address: 127.0.0.1\n"
      "address: 127.0.0.1\n";
    invalidate ("hosts");
    check_hostent ("inet by name", gethostbyname ("localhost"), expected);
    in_addr_t addr = htonl (INADDR_LOOPBACK);
    invalidate ("hosts");
    check_hostent ("inet by address", gethostbyaddr (&addr, sizeof (addr),
                                                     AF_INET),
                   "name: localhost\n"
                   "alias: localhost.localdomain\n"
                   "address: 127.0.0.1\n");

    invalidate ("hosts");
    check_hostent ("missing.example by name",
                   gethostbyname ("missing.example"),
                   "error: TRY_AGAIN\n"); /* Network is not available.  */
  }

  {
    const char *expected =
      "name: localhost\n"
      "alias: localhost.localdomain\n"
      "address: ::1\n";
    invalidate ("hosts");
    check_hostent ("inet6 by name", gethostbyname2 ("localhost", AF_INET6),
                   expected);
    invalidate ("hosts");
    check_hostent ("inet6 by address",
                   gethostbyaddr (&in6addr_loopback,
                                  sizeof (in6addr_loopback), AF_INET6),
                   expected);
  }

  {
    struct addrinfo hints =
      {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
      };
    struct addrinfo *ai;
    int ret = getaddrinfo ("localhost", "80", &hints, &ai);
    invalidate ("hosts");
    check_addrinfo ("addrinfo", ai, ret,
                    "address: STREAM/TCP 127.0.0.1 80\n"
                    "address: STREAM/TCP ::1 80\n");
    if (ret == 0)
      freeaddrinfo (ai);

    ret = getaddrinfo ("missing", "80", &hints, &ai);
    invalidate ("hosts");
    check_addrinfo ("addrinfo", ai, ret,
                    "error: Temporary failure in name resolution\n");
    if (ret == 0)
      freeaddrinfo (ai);
  }

  {
    struct addrinfo hints =
      {
        .ai_flags = AI_CANONNAME,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
      };
    struct addrinfo *ai;
    int ret = getaddrinfo ("www", "80", &hints, &ai);
    invalidate ("hosts");
    check_addrinfo ("addrinfo", ai, ret,
                    "flags: AI_CANONNAME\n"
                    "canonname: www.example.com\n"
                    "address: STREAM/TCP 192.0.2.1 80\n");
    if (ret == 0)
      freeaddrinfo (ai);
  }
}

static void
test_netgroup (void)
{
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "", "batman", ""));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "public", "batman", ""));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "", "batman", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "public", "batman", "earth"));

  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "", "superman", ""));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "public", "superman", ""));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "", "superman", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "public", "superman", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "private", "superman", ""));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "private", "superman", "earth"));

  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "", "clark", ""));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "private", "clark", ""));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "", "clark", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "private", "clark", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "public", "clark", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "", "clark", "krypton"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "public", "clark", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "private", "clark", "krypton"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "public", "clark", "krypton"));

  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "", "kalel", ""));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "private", "kalel", ""));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "", "kalel", "krypton"));
  invalidate ("netgroup");
  TEST_VERIFY (innetgr ("hero", "private", "kalel", "krypton"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "", "kalel", "earth"));
  invalidate ("netgroup");
  TEST_VERIFY (!innetgr ("hero", "public", "kalel", "earth"));

  {
    TEST_COMPARE (setnetgrent ("hero"), 1);
    char *host = NULL;
    char *user = NULL;
    char *domain = NULL;
    TEST_COMPARE (getnetgrent (&host, &user, &domain), 1);
    TEST_COMPARE_STRING (host, NULL);
    TEST_COMPARE_STRING (user, "batman");
    TEST_COMPARE_STRING (domain, NULL);
    TEST_COMPARE (getnetgrent (&host, &user, &domain), 1);
    TEST_COMPARE_STRING (host, "private");
    TEST_COMPARE_STRING (user, "clark");
    TEST_COMPARE_STRING (domain, "earth");
    TEST_COMPARE (getnetgrent (&host, &user, &domain), 1);
    TEST_COMPARE_STRING (host, "public");
    TEST_COMPARE_STRING (user, "superman");
    TEST_COMPARE_STRING (domain, NULL);
    TEST_COMPARE (getnetgrent (&host, &user, &domain), 1);
    TEST_COMPARE_STRING (host, NULL);
    TEST_COMPARE_STRING (user, "kalel");
    TEST_COMPARE_STRING (domain, "krypton");
    TEST_COMPARE (getnetgrent (&host, &user, &domain), 0);
    endnetgrent ();
  }
}

static void
test_services (void)
{
  invalidate ("services");
  check_servent ("exec/tcp", getservbyname ("exec", "tcp"),
                 "name: exec\n"
                 "port: 512\n"
                 "proto: tcp\n");
  invalidate ("services");
  errno = 0;
  check_servent ("exec/udp", getservbyname ("exec", "udp"), not_found);
  invalidate ("services");
  check_servent ("biff/udp", getservbyname ("biff", "udp"),
                 "name: biff\n"
                 "alias: comsat\n"
                 "port: 512\n"
                 "proto: udp\n");
  invalidate ("services");
  check_servent ("comsat/udp", getservbyname ("comsat", "udp"),
                 "name: biff\n"
                 "alias: comsat\n"
                 "port: 512\n"
                 "proto: udp\n");
  invalidate ("services");
  errno = 0;
  check_servent ("biff/tcp", getservbyname ("biff", "tcp"), not_found);
}

static void
all_tests (void *ignored)
{
  test_passwd ();
  test_group ();
  test_grouplist ();
  test_hosts ();
  test_netgroup ();
  test_services ();
}

static int
do_test (void)
{
  setup_files ();

  /* First run the tests without nscd running, to check that the test
     expectations are correct.  Use a separate process to avoid
     caching nscd availability.  */
  support_isolate_in_subprocess (all_tests, NULL);

  support_nscd_copy_configuration ();

  support_nscd_start ();

  all_tests (NULL);

  /* Retry from cache.  */
  all_tests (NULL);

  invalidate = support_nscd_invalidate;
  all_tests (NULL);

  support_nscd_stop ();

  free (large_gecos);

  return 0;
}

#include <support/test-driver.c>
