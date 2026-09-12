#!/bin/sh

# Copyright 2024-2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

# Check that the @deftypefun command is called with the expected
# arguments and includes checking for common mistakes including
# failure to include a space after the function name, or incorrect
# quoting.

success=:

# If no arguments are given, take all *.texi files in the current directory.
test $# != 0 || set *.texi

# We search for all @deftypefun and @deftypefunx command uses.
# Then we remove all of those that match our expectations.
# A @deftypefun or @deftypefunx command takes 3 arguments:
# - return type
# - name
# - arguments
# This is different from @deftypefn which includes an additional
# category which is implicit here.
grep -n -r '^@deftypefun' "$@" |
grep -v '^.*@deftypefunx\?'\
' \({\?[a-zA-Z0-9_ *]*}\?\) \([a-zA-Z0-9_]*\) (.*)$' &&
success=false

# We search for all @deftypefn and @deftypefnx command uses.
# We have 4 arguments in the command including the category.
grep -n -r '^@deftypefn' "$@" |
grep -v '^.*@deftypefnx\?'\
' {\?[a-zA-Z ]*}\? \({\?[a-zA-Z0-9@{}_ *]*}\?\) \([a-zA-Z0-9_]*\) (.*)$' &&
success=false

$success
