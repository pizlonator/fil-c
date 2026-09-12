#!/bin/sh
# Test for thread ID logging in dynamic linker.
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

# This script runs the tst-dl-debug-tid test case and verifies its
# LD_DEBUG output. It checks for thread creation/destruction messages
# to ensure the dynamic linker's thread-aware logging is working.

set -e

# Arguments are from Makefile.
common_objpfx="$1"
test_wrapper_env="$2"
rtld_prefix="$3"
run_program_env="$4"
test_program="$5"

debug_output="${common_objpfx}elf/tst-dl-debug-tid.debug"
rm -f "${debug_output}".*

# Run the test program with LD_DEBUG=tls.
eval "${test_wrapper_env}" LD_DEBUG=tls LD_DEBUG_OUTPUT="${debug_output}" \
    "${rtld_prefix}" "${test_program}"

debug_output=$(ls "${debug_output}".*)
# Check for the "Thread starting" message.
if ! grep -q 'tls: thread starting; TID=.*, TCB=0x' "${debug_output}"; then
    echo "error: 'Thread starting' message not found"
    cat "${debug_output}"
    exit 1
fi

# Check that we have a message where the PID (from prefix) is different
# from the TID (in the message). This indicates a worker thread log.
if ! grep 'tls: thread starting; TID=.*, TCB=0x' "${debug_output}" | awk -F '[ \t:]+' '{
  sub(/TID=/, "", $5);
  sub(/,/, "", $5);
  if ($1 != $5)
    exit 0;
  exit 1
}'; then
    echo "error: No 'Thread starting' message from a worker thread found"
    cat "${debug_output}"
    exit 1
fi

# We expect messages from thread creation and destruction.
if ! grep -q 'tls: allocate TCB 0x\|tls: deallocate TCB 0x\|tls: TCB reused from cache\|tls: TCB deallocated' \
     "${debug_output}"; then
    echo "error: Expected TCB allocation/deallocation message not found"
    cat "${debug_output}"
    exit 1
fi

# Check for TLS module ID assignment.
if ! grep -q 'tls: assign modid .* to' "${debug_output}"; then
    echo "error: Expected 'modid ... assigned to' message not found"
    cat "${debug_output}"
    exit 1
fi

# Check for TLS block allocation.
if ! grep -q 'tls: allocate block .* for modid .* size=.*, TCB=0x' "${debug_output}"; then
    echo "error: Expected 'modid ... allocated' message not found"
    cat "${debug_output}"
    exit 1
fi

# TLS block deallocation might be skipped due to DTV surplus.
if grep -q 'tls: deallocate block .* for modid .* TCB=0x' "${debug_output}"; then
    echo "INFO: module deallocated message found"
else
    echo "INFO: module deallocated message not found (may be due to DTV surplus)"
fi

cat "${debug_output}"
rm -f "${debug_output}"
