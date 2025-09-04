#!/usr/bin/python
# Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

def get_glibc_shared_code(path):
    """ Get glibc shared code information from a file

    The input file must have project names in their own line ending with a colon
    and all shared files in the project on their own lines following the project
    name.  Whitespaces are ignored.  Lines with # as the first non-whitespace
    character are ignored.

    Args:
        path: The path to file containing shared code information.

    Returns:
        A dictionary with project names as key and lists of files as values.
    """

    projects = {}
    with open(path, 'r') as f:
        for line in f.readlines():
            line = line.strip()
            if len(line) == 0 or line[0] == '#':
                continue
            if line[-1] == ':':
                cur = line[:-1]
                projects[cur] = []
            else:
                projects[cur].append(line)

    return projects

# Function testing.
import sys
from os import EX_NOINPUT
from os.path import exists
from pprint import *

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: %s <file name>' % sys.argv[0])
        print('Run this script from the base glibc source directory')
        sys.exit(EX_NOINPUT)

    print('Testing get_glibc_shared_code with %s:\n' % sys.argv[1])
    r = get_glibc_shared_code(sys.argv[1])
    errors = False
    for k in r.keys():
        for f in r[k]:
            if not exists(f):
                print('%s does not exist' % f)
                errors = True

    if not errors:
        pprint(r)
