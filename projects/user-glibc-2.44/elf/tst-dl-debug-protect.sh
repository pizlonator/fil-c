#!/bin/sh
# A script to run tests with LD_DEBUG=security and check
# that output contains expected pattern.
# Copyright (C) 2026 Free Software Foundation, Inc.
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

# Arguments are from Makefile:
# Path to the current build folder
objpfx="$1"
# Test wrapper command line
wrapper="$2"
# Dynamic loader command line
loader="$3"
# Test environment variables
runenv="$4"
# Grep pattern to look for in the test output
pattern="$5"
# Path to the test executable to run
program="$6"

output="${objpfx}`basename ${program}`.debug"
rm -f "${output}".*

eval "${wrapper}" \
  LD_DEBUG=security LD_DEBUG_OUTPUT="${output}" ${runenv} \
  "${loader}" "${program}"
rc=$?

if test $rc -eq 77; then
  echo "Test is not supported"
  rm -f "${output}".*
  exit 77
fi

output=$(ls "${output}".*)
cat "${output}"
if ! grep -q "${pattern}" "${output}"; then
  echo "Could not find expected '${pattern}' in LD_DEBUG_OUTPUT file"
  exit 1
fi
rm -f "${output}"
