#!/bin/sh
# Test for TLS logging in dynamic linker.
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

# This script runs the tst-tls-debug-recursive test case and verifies its
# LD_DEBUG=tls output. It checks for various TLS-related messages
# to ensure the dynamic linker's TLS logging is working correctly.

set -e
common_objpfx="$1"
test_wrapper_env="$2"
rtld_prefix="$3"
run_program_env="$4"
test_program="$5"

debug_output="${common_objpfx}elf/tst-tls-debug-recursive.debug"
rm -f "${debug_output}".*

# Run the test program with LD_DEBUG=tls.
eval "${test_wrapper_env}" LD_DEBUG=tls LD_DEBUG_OUTPUT="${debug_output}" \
    "${rtld_prefix}" "${test_program}"

debug_output=$(ls "${debug_output}".*)

fail=0

# Check for expected messages
if ! grep -q 'tls: DTV resized for TCB 0x.*: oldsize' "${debug_output}"; then
  echo "FAIL: DTV resized message not found"
  fail=1
fi

if ! grep -q 'tls: DTV update for TCB 0x.*: modid .* deallocated block' "${debug_output}"; then
  echo "FAIL: module deallocated during DTV update message not found"
  fail=1
fi

if ! grep -q 'tls: assign modid .* to' "${debug_output}"; then
  echo "FAIL: module assigned message not found"
  fail=1
fi

if ! grep -q 'tls: allocate block .* for modid .* size=.*, TCB=0x' "${debug_output}"; then
  echo "FAIL: module allocated message not found"
  fail=1
fi

if ! grep -q 'tls: modid .* update DTV to generation .* TCB=0x' "${debug_output}"; then
  echo "FAIL: update DTV message not found"
  fail=1
fi

if ! grep -q 'tls: initial modid limit set to' "${debug_output}"; then
  echo "FAIL: initial modid limit message not found"
  fail=1
fi

if [ $fail -ne 0 ]; then
  echo "Test FAILED"
  cat "${debug_output}"
  rm -f "${debug_output}"
  exit 1
fi

echo "Test PASSED"
cat "${debug_output}"
rm -f "${debug_output}"
exit 0
