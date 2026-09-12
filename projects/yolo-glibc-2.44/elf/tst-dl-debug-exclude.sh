#!/bin/sh
# Test for LD_DEBUG category exclusion.
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

# This script verifies the LD_DEBUG category exclusion functionality.
# It checks that:
# 1. Categories can be excluded using the '-' prefix.
# 2. Options are processed sequentially, meaning the last specified
#    option for a category (enable or exclude) takes precedence.

set -e

common_objpfx="$1"
test_wrapper_env="$2"
rtld_prefix="$3"
run_program_env="$4"
test_program="$5"

debug_output="${common_objpfx}elf/tst-dl-debug-exclude.debug"
rm -f "${debug_output}".*

# Run the test program with LD_DEBUG=all,-tls.
# We expect general logs but no TLS logs.
eval "${test_wrapper_env}" LD_DEBUG=all,-tls LD_DEBUG_OUTPUT="${debug_output}" \
    "${rtld_prefix}" "${test_program}"

fail=0

# 1. Check that general logs are present (e.g., file loading)
if ! grep -q 'file=' "${debug_output}".*; then
  echo "FAIL: 'file=' message not found (LD_DEBUG=all failed)"
  fail=1
fi

# 2. Check that TLS logs are NOT present
if grep -q 'tls: ' "${debug_output}".*; then
  echo "FAIL: TLS message found (exclusion of -tls failed)"
  fail=1
fi

rm -f "${debug_output}".*
# 3. Check for LD_DEBUG=all,-tls,tls (ordering verification)
# We expect TLS logs to BE present
eval "${test_wrapper_env}" LD_DEBUG=all,-tls,tls LD_DEBUG_OUTPUT="${debug_output}" \
    "${rtld_prefix}" "${test_program}"

if ! grep -q 'tls: ' "${debug_output}".*; then
  echo "FAIL: TLS message not found (ordering -tls,tls failed)"
  fail=1
fi

rm -f "${debug_output}".*
# 4. Check for LD_DEBUG=tls,-tls
# We expect TLS logs to NOT be present
eval "${test_wrapper_env}" LD_DEBUG=tls,-tls LD_DEBUG_OUTPUT="${debug_output}" \
    "${rtld_prefix}" "${test_program}"

if grep -q 'tls: ' "${debug_output}".*; then
  echo "FAIL: TLS message found (ordering tls,-tls failed)"
  fail=1
fi

if [ $fail -ne 0 ]; then
  echo "Test FAILED"
  cat "${debug_output}".*
  rm -f "${debug_output}".*
  exit 1
fi

echo "Test PASSED"
rm -f "${debug_output}".*
exit 0
