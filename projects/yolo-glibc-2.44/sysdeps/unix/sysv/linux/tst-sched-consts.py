#!/usr/bin/python3
# Test that glibc's sched.h constants match the kernel's.
# Copyright (C) 2018-2026 Free Software Foundation, Inc.
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
        description="Test that glibc's sched.h constants "
        "match the kernel's.")
    parser.add_argument('--cc', metavar='CC',
                        help='C compiler (including options) to use')
    args = parser.parse_args()
    linux_version_headers = glibcsyscalls.linux_kernel_version(args.cc)
    linux_version_glibc = (7, 1)
    sys.exit(glibcextract.compare_macro_consts(
        '#define _GNU_SOURCE 1\n'
        '#include <sched.h>\n',
        '#define _GNU_SOURCE 1\n'
        '#include <linux/sched.h>\n'
        '#include <linux/sched/types.h>\n',
        args.cc,
        'SCHED_.*',
        # SCHED_ISO is reserved, but not implemented in the kernel.
        # SCHED_OTHER is the standard name for SCHED_NORMAL.
        # SCHED_FLAG_ALL will receive more and more flags, so
        # exposing it to userspace does not seem useful.
        'SCHED_ISO'
        '|SCHED_OTHER'
        '|SCHED_FLAG_ALL',
        linux_version_glibc > linux_version_headers,
        linux_version_headers > linux_version_glibc))

if __name__ == '__main__':
    main()
