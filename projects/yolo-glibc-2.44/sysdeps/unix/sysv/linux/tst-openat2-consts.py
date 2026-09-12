#!/usr/bin/python3
# Test that glibc's sys/openat2.h constants match the kernel's.
# Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
        description="Test that glibc's sys/openat2.h constants "
        "match the kernel's.")
    parser.add_argument('--cc', metavar='CC',
                        help='C compiler (including options) to use')
    args = parser.parse_args()

    if glibcextract.compile_c_snippet(
            '#include <linux/openat2.h>',
            args.cc).returncode != 0:
        sys.exit (77)

    linux_version_headers = glibcsyscalls.linux_kernel_version(args.cc)
    # Constants in glibc were updated to match Linux v7.1.  When glibc
    # constants are updated this value should be updated to match the
    # released kernel version from which the constants were taken.
    linux_version_glibc = (7, 1)
    def check(cte, exclude=None):
        return glibcextract.compare_macro_consts(
                '#define _FCNTL_H\n'
                '#include <stdint.h>\n'
                '#include <bits/openat2.h>\n',
                '#include <asm/fcntl.h>\n'
                '#include <linux/openat2.h>\n',
                args.cc,
                cte,
                exclude,
                linux_version_glibc > linux_version_headers,
                linux_version_headers > linux_version_glibc)

    status = check('RESOLVE.*')
    sys.exit(status)

if __name__ == '__main__':
    main()
