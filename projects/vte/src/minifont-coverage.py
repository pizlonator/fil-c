#!/usr/bin/python3
# Copyright © 2024 Christian Persch
#
# This programme is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This programme is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this programme.  If not, see <https://www.gnu.org/licenses/>.

import argparse
import os
import pathlib
import re
import sys

TYPE         = "uint64_t"
TYPE_C       = "UINT64_C"
TYPE_BITBITS = 6

coverage = [False] * 0x110000

def maybe_write_file(filename, output):
    try:
        with open(filename, "r") as of:
            content = of.read()
            if content == output:
                return
    except:
        pass

    of = open(filename, "w")
    of.write(output)
    of.close()

def process(filename):

    # We want to get the minifont coverage from the source. We could
    # try to build some test programme that runs MiniFont::draw_graphic()
    # for all unichars and tries to see if something was drawn, but just
    # greeping the source for "case" labels with unichar works
    # fine for now.

    rx = re.compile(r"case\s+(?P<first>0x[0-9a-fA-z]{4,6})(?:\s*\.\.\.\s*(?P<last>0x[0-9a-fA-F]{4,6}))?\s*:")
    with open(filename, "r") as f:
        for line in f:
            match = rx.search(line)
            if match is not None:
                first = match.group('first')
                last = match.group('last')
                if last is not None:
                    for c in range(int(first, 16), int(last, 16) + 1):
                        coverage[c] = True
                else:
                    coverage[int(first, 16)] = True

def write_output(output_filename):

    def n_covered(start, end):
        n = 0
        for c in range(start, end):
            if coverage[c]:
                n += 1
        return n

    # Coverage of each unicode block can be stored by 4 64-bit
    # integers. To further optimise, see if the coverage is
    # full.
    full = [] # start
    partial = [] # (start, coverage)
    blocks = []
    for block in range(0, 0x1100):
        block_start = block * 0x100
        any = False
        for line in range(0, 1 << (8 - TYPE_BITBITS)):
            start = block_start + line * (1 << TYPE_BITBITS)
            end = start + (1 << TYPE_BITBITS)
            n = n_covered(start, end)
            if n == (1 << TYPE_BITBITS):
                any = True
                full += [start]

            elif n > 0:
                any = True
                bits = 0
                for bit in range(0, 1 << TYPE_BITBITS):
                    if coverage[start + bit]:
                        bits |= 1 << bit

                partial += [(start, bits)]

        if any:
            blocks += [block_start]

    output = """// This is a generated file; do not edit!

static inline /* C++23 constexpr */ bool
unistr_is_local_graphic(char32_t c) noexcept
{
#if VTE_DEBUG && (VERSION_MINOR % 2) == 1 && !defined(MINIFONT_TEST)
  // Cover the whole blocks in debug mode on development versions
  switch (c >> 8) { // block
"""
    for block in blocks:
        output+= f"  case 0x{block >> 8:x}: /* U+{block >> 8:X}xx */\n"

    output += "    [[unlikely]] return true;\n"
    output += "  default: [[likely]] return false;\n"
    output += "  }\n\n"

    output += "#else // VTE_DEBUG\n\n"

    output += f"  switch (c >> {TYPE_BITBITS}) " + "{\n"
    for start in full:
        output += f"  case 0x{start >> TYPE_BITBITS:x}: /* {start:4X} */\n"

    output += "      [[unlikely]] return true;\n"

    for (start, bits) in partial:
        output += f"  case 0x{start >> TYPE_BITBITS:x}: /* {start:4X} */ [[unlikely]]" + " {\n"
        output += f"    static constexpr {TYPE} bits_{start:X} = {TYPE_C}(0b{bits:064b});\n"
        output += f"    return bits_{start:X} & ({TYPE_C}(1) << (c & 0x{(1 << TYPE_BITBITS) - 1:x}));\n"
        output +=  "  }\n"

    output += "  default: [[likely]] return false;\n"
    output += "  }\n"
    output += "#endif // VTE_DEBUG\n"
    output += "}\n"

    maybe_write_file(output_filename, output)

def write_tests(output_filename):
    output = """// This is a generated file; do not edit!

static constinit char32_t const minifont_coverage[] = {
"""

    for c in range(0, 0x110000):
        if coverage[c]:
            output += f'  0x{c:4X},\n'
            continue

    output += "};\n"

    maybe_write_file(output_filename, output)

parser = argparse.ArgumentParser(description='MiniFont coverage generator')
parser.add_argument('files', metavar='FILE', type=pathlib.Path, nargs='+',
                    help='Source file')
parser.add_argument('--output', type=pathlib.Path, required=True,
                    help='Output file')
parser.add_argument('--no-act', action='store_true', default=False,
                    help='Simulate only')
parser.add_argument('--tests', action='store_true', default=False,
                    help='Write testcases')

try:
    args = parser.parse_args()
except Exception as e:
    print(f'Failed to parse arguments: {e}')
    sys.exit(1)

for f in args.files:
    try:
        process(f)
    except Exception as e:
        print(f'Failed to process source file {f}: {e}')
        sys.exit(1)

if not args.no_act:
    try:
        if args.tests:
            write_tests(args.output)
        else:
            write_output(args.output)
    except Exception as e:
        print(f'Failed to write output file {args.output}: {e}')
        sys.exit(1)

sys.exit(0)
