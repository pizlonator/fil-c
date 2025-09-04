#!/usr/bin/python3
# Sort Makefile lines as expected by project policy.
# Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

# The project consensus is to split Makefile variable assignment
# across multiple lines with one value per line.  The values are
# then sorted as described below, and terminated with a special
# list termination marker.  This splitting makes it much easier
# to add new tests to the list since they become just a single
# line insertion.  It also makes backports and merges easier
# since the new test may not conflict due to the ordering.
#
# Consensus discussion:
# https://inbox.sourceware.org/libc-alpha/f6406204-84f5-adb1-d00e-979ebeebbbde@redhat.com/
#
# To support cleaning up Makefiles we created this program to
# help sort existing lists converted to the new format.
#
# The program takes as input the Makefile to sort correctly,
# and the output file to write the correctly sorted output
# (it can be the same file).
#
# Sorting is only carried out between two special markers:
# (a) Marker start is '<variable> += \' (or '= \', or ':= \')
# (b) Marker end is '  # <variable>' (whitespace matters)
# With everything between (a) and (b) being sorted accordingly.
#
# You can use it like this:
# $ scripts/sort-makefile-lines.py < elf/Makefile > elf/Makefile.tmp
# $ mv elf/Makefile.tmp elf/Makefile
#
# The Makefile lines in the project are sorted using the
# following rules:
# - All lines are sorted as-if `LC_COLLATE=C sort`
# - Lines that have a numeric suffix and whose leading prefix
#   matches exactly are sorted according the numeric suffix
#   in increasing numerical order.
#
# For example:
# ~~~
# tests += \
#   test-a \
#   test-b \
#   test-b1 \
#   test-b2 \
#   test-b10 \
#   test-b20 \
#   test-b100 \
#   # tests
# ~~~
# This example shows tests sorted alphabetically, followed
# by a numeric suffix sort in increasing numeric order.
#
# Cleanups:
# - Tests that end in "a" or "b" variants should be renamed to
#   end in just the numerical value. For example 'tst-mutex7robust'
#   should be renamed to 'tst-mutex12' (the highest numbered test)
#   or 'tst-robust11' (the highest numbered test) in order to get
#   reasonable ordering.
# - Modules that end in "mod" or "mod1" should be renamed. For
#   example 'tst-atfork2mod' should be renamed to 'tst-mod-atfork2'
#   (test module for atfork2). If there are more than one module
#   then they should be named with a suffix that uses [0-9] first
#   then [A-Z] next for a total of 36 possible modules per test.
#   No manually listed test currently uses more than that (though
#   automatically generated tests may; they don't need sorting).
# - Avoid including another test and instead refactor into common
#   code with all tests including the common code, then give the
#   tests unique names.
#
# If you have a Makefile that needs converting, then you can
# quickly split the values into one-per-line, ensure the start
# and end markers are in place, and then run the script to
# sort the values.

import sys
import locale
import re
import functools

def glibc_makefile_numeric(string1, string2):
    # Check if string1 has a numeric suffix.
    var1 = re.search(r'([0-9]+) \\$', string1)
    var2 = re.search(r'([0-9]+) \\$', string2)
    if var1 and var2:
        if string1[0:var1.span()[0]] == string2[0:var2.span()[0]]:
            # string1 and string2 both share a prefix and
            # have a numeric suffix that can be compared.
            # Sort order is based on the numeric suffix.
            # If the suffix is the same return 0, otherwise
            # > 0 for greater-than, and < 0 for less-than.
            # This is equivalent to the numerical difference.
            return int(var1.group(1)) - int(var2.group(1))
    # Default to strcoll.
    return locale.strcoll(string1, string2)

def sort_lines(lines):

    # Use the C locale for language independent collation.
    locale.setlocale (locale.LC_ALL, "C")

    # Sort using a glibc-specific sorting function.
    lines = sorted(lines, key=functools.cmp_to_key(glibc_makefile_numeric))

    return lines

def sort_makefile_lines():

    # Read the whole Makefile.
    lines = sys.stdin.readlines()

    # Build a list of all start markers (tuple includes name).
    startmarks = []
    for i in range(len(lines)):
        # Look for things like "var = \", "var := \" or "var += \"
        # to start the sorted list.
        var = re.search(r'^([-_a-zA-Z0-9]*) [\+:]?\= \\$', lines[i])
        if var:
            # Remember the index and the name.
            startmarks.append((i, var.group(1)))

    # For each start marker try to find a matching end mark
    # and build a block that needs sorting.  The end marker
    # must have the matching comment name for it to be valid.
    rangemarks = []
    for sm in startmarks:
        # Look for things like "  # var" to end the sorted list.
        reg = r'^ *# ' + sm[1] + r'$'
        for j in range(sm[0] + 1, len(lines)):
            if re.search(reg, lines[j]):
                # Remember the block to sort (inclusive).
                rangemarks.append((sm[0] + 1, j))
                break

    # We now have a list of all ranges that need sorting.
    # Sort those ranges (inclusive).
    for r in rangemarks:
        lines[r[0]:r[1]] = sort_lines(lines[r[0]:r[1]])

    # Output the whole list with sorted lines to stdout.
    [sys.stdout.write(line) for line in lines]


def main(argv):
    sort_makefile_lines ()

if __name__ == '__main__':
    main(sys.argv[1:])
