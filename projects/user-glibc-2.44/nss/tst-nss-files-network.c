/* Test long entries and truncated numbers in /etc/networks (bug 32573/32575).
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <netdb.h>
#include <gnu/lib-names.h>
#include <nss.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/check_nss.h>
#include <support/namespace.h>
#include <support/test-driver.h>
#include <support/xdlfcn.h>
#include <support/xunistd.h>
#include <sys/resource.h>

#define STACK_LIM 1048576
#define STRING_SIZE (2 * STACK_LIM)

struct support_chroot *chroot_env;

static void
prepare (int argc, char **argv)
{
  int ret;
  char *content;
  char *entry = malloc (STRING_SIZE);
  struct rlimit lim;
  getrlimit (RLIMIT_STACK, &lim);
  lim.rlim_cur = STACK_LIM;
  setrlimit (RLIMIT_STACK, &lim);
  if (entry == NULL)
    {
      puts ("malloc failed, cannot test");
      exit (1);
    }
  memset (entry, 'A', STRING_SIZE);
  entry[STRING_SIZE - 1] = 0;
  ret = asprintf (&content, "%s\n%s\nnet3 %s\n",
    "net1 x0000000000Ff.077", /* legal 255.63.0.0 */
    "net2 xFF00000000.0.0.0", /* illegal */
    entry /* illegal */);
  if (ret == -1)
    {
      puts ("asprintf failed, cannot test");
      exit (1);
    }
  free (entry);
  chroot_env = support_chroot_create
    ((struct support_chroot_configuration)
     {
       .networks = content
     });

}

static int
do_test (void)
{
  support_become_root ();
  if (!support_can_chroot ())
    return EXIT_UNSUPPORTED;

  __nss_configure_lookup ("networks", "files");
  xdlopen (LIBNSS_FILES_SO, RTLD_NOW);

  xchroot (chroot_env->path_chroot);

  check_netent ("net1", getnetbyname ("net1"),
    "name: net1\n"
    "net: 0xff3f0000\n");
  check_netent ("net2", getnetbyname ("net2"), "error: HOST_NOT_FOUND\n");

  support_chroot_free (chroot_env);
  return 0;
}

#define PREPARE prepare
#include <support/test-driver.c>
