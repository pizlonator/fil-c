#!/usr/bin/python3
#
# Test if getconf returns the symbolic constants defined in limits.h.
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
import glibcextract
import subprocess
import sys


def parse_getconf(cmd):
    getconf_out = subprocess.run(cmd + ['-a'], stdout=subprocess.PIPE,
                                 check=True, universal_newlines=True).stdout
    getconf_set = {}
    for line in getconf_out.splitlines():
        fields = line.split()
        if len(fields) == 2:
            getconf_set[fields[0]] = fields[1]
    return getconf_set


def main():
    """The main entry point."""
    parser = argparse.ArgumentParser(
        description='Test if getconf returns the symbolic constants defined '
        'in limits.h.')
    parser.add_argument('--cc', metavar='CC',
                        help='C compiler (including options) to use')
    parser.add_argument('cmd',
                        help='How to run getconf')
    args = parser.parse_args()

    ctes = glibcextract.compute_macro_consts(
            source_text='#include <limits.h>\n',
            cc=args.cc,
            macro_re='_POSIX.*',
            exclude_re='_POSIX_C_SOURCE|_POSIX_SOURCE')

    getconf_ctes = parse_getconf (args.cmd.split())

    ret = 0
    for cte,value in ctes.items():
        if cte in getconf_ctes:
            if getconf_ctes[cte] != value:
                print('Symbol {} is defined as {} on limits.h but getconf shows {}'
                      .format(cte, value, getconf_ctes[cte]))
                ret = 1
        else:
            print('Symbol {} not presented in getconf'.format(cte))
            ret = 1
    sys.exit(ret)


if __name__ == '__main__':
    main()
