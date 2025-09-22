#!/usr/bin/env bash
#
# Copyright (C) 2019-2021 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/debuginfod-subr.sh

# for test case debugging, uncomment:
set -x
unset VALGRIND_CMD

export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

# Test fetching a file using file:// . No debuginfod server needs to be run for
# this test.
local_dir=${PWD}/mocktree/buildid/aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd/source/my/path
mkdir -p ${local_dir}
echo "int main() { return 0; }" > ${local_dir}/main.c
# first test that is doesn't work, when no DEBUGINFOD_URLS is set
export DEBUGINFOD_URLS=""
testrun ${abs_top_builddir}/debuginfod/debuginfod-find source aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd /my/path/main.c && false || true

# Now test is with proper DEBUGINFOD_URLS
export DEBUGINFOD_URLS="file://${PWD}/mocktree/"
filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find source aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd /my/path/main.c`
cmp $filename ${local_dir}/main.c

exit 0
