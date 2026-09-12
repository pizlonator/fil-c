#!/bin/sh
# Testing of multi-character transliterations
# Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

common_objpfx=$1
run_program_prefix_before_env=$2
run_program_env=$3
run_program_prefix_after_env=$4

# Generate data files.
# The locale only defines the LC_CTYPE category, so we expect a failure
# due to warnings.
ret=0
${run_program_prefix_before_env} \
${run_program_env} \
I18NPATH=../localedata \
${run_program_prefix_after_env} ${common_objpfx}locale/localedef \
--quiet -i tst-translit-locale -f UTF-8 ${common_objpfx}iconv/tst-translit || ret=$?
if [ $ret -gt 1 ]; then
  echo "FAIL: Locale compilation for tst-translit-locale failed (error $ret)."
  exit 1
fi

set -x

# Run the test.
${run_program_prefix_before_env} \
${run_program_env} \
LOCPATH=${common_objpfx}iconv \
${run_program_prefix_after_env} ${common_objpfx}iconv/tst-translit-mchar

# Local Variables:
#  mode:shell-script
# End:
