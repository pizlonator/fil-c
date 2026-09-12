/* Test the getusershell series functions.
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

#include "support/temp_file.h"
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <support/support.h>
#include <support/namespace.h>
#include <support/test-driver.h>

static void
test_in_chroot (void *chroot_path)
{
  xchroot (chroot_path);

  TEST_COMPARE_STRING (getusershell (), "/bin/sh");
  TEST_COMPARE_STRING (getusershell (), "/bin/bash");
  TEST_COMPARE_STRING (getusershell (), "/usr/bin/zsh");
  TEST_COMPARE_STRING (getusershell (), NULL);
  TEST_COMPARE_STRING (getusershell (), NULL);

  setusershell ();
  TEST_COMPARE_STRING (getusershell (), "/bin/sh");
  endusershell ();

  xunlink ("/etc/shells");
  TEST_COMPARE_STRING (getusershell (), "/bin/sh");
  TEST_COMPARE_STRING (getusershell (), "/bin/csh");
  TEST_COMPARE_STRING (getusershell (), NULL);
  endusershell ();
}

static int
do_test (void)
{
  support_become_root ();
  if (!support_can_chroot ())
    return EXIT_UNSUPPORTED;

  char *chroot_dir = support_create_temp_directory ("tst-getusershell-");
  char *etc = xasprintf ("%s/etc", chroot_dir);
  add_temp_file (etc);
  xmkdir (etc, 0777);
  /* Don't add shells to file list as it will be deleted in test. */
  char *shells = xasprintf ("%s/shells", etc);
  support_copy_file ("tst-getusershell.shells", shells);

  support_isolate_in_subprocess (test_in_chroot, chroot_dir);

  free (etc);
  free (shells);
  free (chroot_dir);

  return 0;
}

#include <support/test-driver.c>
