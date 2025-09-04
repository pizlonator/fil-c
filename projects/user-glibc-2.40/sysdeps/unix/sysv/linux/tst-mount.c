/* Basic tests for the new Linux API added on Linux 5.2.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <support/namespace.h>
#include <sys/mount.h>

_Static_assert (sizeof (struct mount_attr) == MOUNT_ATTR_SIZE_VER0,
		"sizeof (struct mount_attr) != MOUNT_ATTR_SIZE_VER0");

static int
do_test (void)
{
  support_become_root ();
  if (!support_enter_mount_namespace ())
    FAIL_UNSUPPORTED ("cannot enter mount namespace, skipping test");

  int r = fsopen ("it_should_be_not_a_valid_mount", 0);
  TEST_VERIFY_EXIT (r == -1);
  if (errno == ENOSYS)
    FAIL_UNSUPPORTED ("kernel does not support new mount API, skipping test");
  TEST_COMPARE (errno, ENODEV);

  int fd = fsopen ("tmpfs", FSOPEN_CLOEXEC);
  TEST_VERIFY (fd != -1);

  TEST_COMPARE (fsconfig (-1, FSCONFIG_SET_STRING, "size", "2048", 0), -1);
  TEST_COMPARE (errno, EINVAL);

  {
    int r = fsconfig (fd, FSCONFIG_SET_STRING, "size", "2048", 0);
    TEST_VERIFY (r != -1);
    r = fsconfig (fd, FSCONFIG_CMD_CREATE, NULL, NULL, 0);
    TEST_VERIFY (r != -1);
  }

  TEST_COMPARE (fsmount (-1, FSMOUNT_CLOEXEC, MOUNT_ATTR_NODEV), -1);
  TEST_COMPARE (errno, EBADF);

  int mfd = fsmount (fd, FSMOUNT_CLOEXEC, MOUNT_ATTR_NODEV);
  TEST_VERIFY (mfd != -1);

  TEST_COMPARE (move_mount (-1, "", AT_FDCWD, "/tmp", 0), -1);
  TEST_COMPARE (errno, ENOENT);
  {
    int r = move_mount (mfd, "", AT_FDCWD, "/tmp", MOVE_MOUNT_F_EMPTY_PATH);
    TEST_VERIFY (r != -1);
  }

  TEST_COMPARE (fspick (AT_FDCWD, "", 0), -1);
  TEST_COMPARE (errno, ENOENT);
  {
    int pfd = fspick (AT_FDCWD, "/tmp", FSPICK_CLOEXEC);
    TEST_VERIFY (pfd != -1);
  }

  TEST_COMPARE (open_tree (AT_FDCWD, "", 0), -1);
  TEST_COMPARE (errno, ENOENT);
  int fd_tree = open_tree (AT_FDCWD, "/tmp",
			   OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC);
  TEST_VERIFY (fd_tree != -1);

  {
    struct mount_attr attr =
    {
      .attr_set = MOUNT_ATTR_RDONLY,
    };
     mount_setattr (fd_tree, "", AT_EMPTY_PATH, &attr,
			   sizeof (attr));
    int r = mount_setattr (fd_tree, "", AT_EMPTY_PATH, &attr,
			   sizeof (attr));
    /* New mount API was added on 5.1, but mount_setattr on 5.12.  */
    if (r == -1)
      TEST_COMPARE (errno, ENOSYS);
    else
      {
	TEST_COMPARE (mount_setattr (-1, "", AT_EMPTY_PATH, &attr,
				     sizeof (attr)), -1);
	TEST_COMPARE (errno, EBADF);
	TEST_COMPARE (mount_setattr (fd_tree, "", AT_EMPTY_PATH, &attr,
				     sizeof (attr) - 8), -1);
	TEST_COMPARE (errno, EINVAL);
      }
  }

  _exit (0);
}

#include <support/test-driver.c>
