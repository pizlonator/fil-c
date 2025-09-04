#!/usr/bin/python3
# Test that glibc's sys/pidfd.h constants match the kernel's.
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
        description="Test that glibc's sys/pidfd.h constants "
        "match the kernel's.")
    parser.add_argument('--cc', metavar='CC',
                        help='C compiler (including options) to use')
    args = parser.parse_args()

    if glibcextract.compile_c_snippet(
            '#include <linux/pidfd.h>',
            args.cc).returncode != 0:
        sys.exit (77)

    linux_version_headers = glibcsyscalls.linux_kernel_version(args.cc)
    linux_version_glibc = (6, 9)
    sys.exit(glibcextract.compare_macro_consts(
                '#include <sys/pidfd.h>\n',
                '#include <asm/fcntl.h>\n'
                '#include <linux/pidfd.h>\n',
                args.cc,
                'PIDFD_.*',
                None,
                linux_version_glibc > linux_version_headers,
                linux_version_headers > linux_version_glibc))

if __name__ == '__main__':
    main()
