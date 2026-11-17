#!/usr/bin/env python
#
# Copyright © 2018, 2019, 2020, 2024 Christian Persch
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import argparse
import sys

bg = "\033[43m"
sgr0 = "\033[m"

def print_block(block):

    for row in range(0, 16):
        sys.stdout.write('\n')
        for column in range(0, 16):
            c = block * 256 + column * 16 + row
            sys.stdout.write(f'  {bg}{chr(c)}{sgr0}')
        sys.stdout.write('\n')
    sys.stdout.write('\n')

''' main '''
if __name__ == '__main__':

    if len(sys.argv) != 2:
        sys.stdout.write(f'Usage: {sys.argv[0]} BLOCK\n')
        sys.exit(2)

    try:
        block = int(sys.argv[1], 16)
    except Exception as e:
        sys.stdout.write(f'Failed to parse block "{sys.argv[1]}": {e}\n')
        sys.exit(3)

    print_block(block)
    sys.exit(0)
