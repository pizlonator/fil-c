#!/usr/bin/python3
# Check if glibc provided sys/mount.h can be used along related kernel
# headers.
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


def main():
    """The main entry point."""
    parser = argparse.ArgumentParser(
        description='Check if glibc provided sys/mount.h can be '
                    ' used along related kernel headers.')
    parser.add_argument('--cc', metavar='CC',
                        help='C compiler (including options) to use')
    args = parser.parse_args()

    if glibcextract.compile_c_snippet(
            '#include <linux/mount.h>',
            args.cc).returncode != 0:
        sys.exit (77)

    def check(testname, snippet):
        # Add -Werror to catch macro redefinitions and _ISOMAC to avoid
        # internal glibc definitions.
        r = glibcextract.compile_c_snippet(snippet, args.cc,
                '-Werror -D_ISOMAC')
        if r.returncode != 0:
            print('error: test {}:\n{}'.format(testname, r.output.decode()))
        return r.returncode

    status = max(
        check("sys/mount.h + linux/mount.h",
              "#include <sys/mount.h>\n"
              "#include <linux/mount.h>"),
        check("sys/mount.h + linux/fs.h",
              "#include <sys/mount.h>\n"
              "#include <linux/fs.h>"),
        check("linux/mount.h + sys/mount.h",
              "#include <linux/mount.h>\n"
              "#include <sys/mount.h>"),
        check("linux/fs.h + sys/mount.h",
              "#include <linux/fs.h>\n"
              "#include <sys/mount.h>"))
    sys.exit(status)

if __name__ == '__main__':
    main()
