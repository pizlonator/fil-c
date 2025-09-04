#!/usr/bin/python3
# Test that glibc's sys/mount.h constants match the kernel's.
# Copyright (C) 2022-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

import argparse
import sys

import glibcextract
import glibcsyscalls


def main():
    """The main entry point."""
    parser = argparse.ArgumentParser(
        description="Test that glibc's sys/mount.h constants "
        "match the kernel's.")
    parser.add_argument('--cc', metavar='CC',
                        help='C compiler (including options) to use')
    args = parser.parse_args()

    if glibcextract.compile_c_snippet(
            '#include <linux/mount.h>',
            args.cc).returncode != 0:
        sys.exit (77)

    linux_version_headers = glibcsyscalls.linux_kernel_version(args.cc)
    # Constants in glibc were updated to match Linux v6.9.  When glibc
    # constants are updated this value should be updated to match the
    # released kernel version from which the constants were taken.
    linux_version_glibc = (6, 9)
    def check(cte, exclude=None):
        return glibcextract.compare_macro_consts(
                '#include <sys/mount.h>\n',
                '#include <asm/fcntl.h>\n'
                '#include <linux/mount.h>\n',
                args.cc,
                cte,
                exclude,
                linux_version_glibc > linux_version_headers,
                linux_version_headers > linux_version_glibc)

    # Skip testing FS_CONFIG commands since they are only enums in the kernel
    # header.
    status = max(
        check('FSOPEN_.*'),
        check('FSMOUNT_.*'),
        # MOVE_MOUNT__MASK may vary depending of the kernel version.
        check('MOVE_MOUNT_.*', 'MOVE_MOUNT__MASK'),
        check('OPEN_TREE_*'),
        # MOUNT_ATTR_SIZE_VER0 is used for mount_setattr.
        check('MOUNT_ATTR_.*', 'MOUNT_ATTR_SIZE_VER0'))
    sys.exit(status)

if __name__ == '__main__':
    main()
