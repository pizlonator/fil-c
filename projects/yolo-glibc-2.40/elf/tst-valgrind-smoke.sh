#!/bin/sh
# Valgrind smoke test.
# Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
system_rtld="$2"
test_wrapper_env="$3"
run_program_env="$4"
library_path="$5"
test_prog="$6"

# Test whether valgrind is available in the test
# environment.  If not, skip the test.
${test_wrapper_env} ${run_program_env} \
  /bin/sh -c "command -v valgrind" || exit 77

# Test valgrind works with the system ld.so in the test environment
/bin/sh -c \
  "${test_wrapper_env} ${run_program_env} \
   valgrind -q --error-exitcode=1 \
     ${system_rtld} /bin/echo ${system_rtld}" || exit 77

# Finally the actual test inside the test environment,
# using the just build ld.so and new libraries to run
# the smoke test under valgrind.
/bin/sh -c \
  "${test_wrapper_env} ${run_program_env} \
   valgrind -q --error-exitcode=1 \
     ${rtld} --library-path ${library_path} ${test_prog} ${rtld}"
