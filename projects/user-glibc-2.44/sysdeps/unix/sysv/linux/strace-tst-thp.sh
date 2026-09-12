#!/bin/bash
# Run THP test under strace to verify control of the THP segment load.
# Copyright (C) 2026 Free Software Foundation, Inc.
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

set -e

rtld="$1"
test_wrapper_env="$2"
run_program_env="$3"
test_prog="$4"

cmd="${test_wrapper_env} ${run_program_env} strace -X raw ${rtld} ${test_prog}"

TIMEOUTFACTOR=${TIMEOUTFACTOR:-1}

case x"${run_program_env}" in
*glibc.elf.thp=1*)
  strace_expected=yes
  ;;
*)
  strace_expected=no
  ;;
esac

# Verify strace is not just present, but works in this environment.  If
# not, skip the test.
/bin/sh -c \
 "${test_wrapper_env} ${run_program_env} \
  strace -X raw -e trace=none -- /bin/true" > /dev/null 2>&1 || exit 77

# Finally the actual test inside the test environment, using the just
# build ld.so and new libraries to run the THP test under strace.
if /bin/sh -c \
  "timeout -k 4 $((3*$TIMEOUTFACTOR)) ${cmd} --direct 2>&1 \
   | grep -E \"madvise\(0x[0-9a-f]+, [0-9]+, 0xe)\""; then
  if test ${strace_expected} = yes; then
    exit 0
  else
    exit 1
  fi
else
  if test ${strace_expected} = no; then
    exit 0
  else
    exit 1
  fi
fi
