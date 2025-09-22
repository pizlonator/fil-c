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

########################################################################
# set up tests for retrying failed queries.
retry_attempts=`(testrun env DEBUGINFOD_URLS=http://255.255.255.255/JUNKJUNK DEBUGINFOD_RETRY_LIMIT=10 DEBUGINFOD_VERBOSE=1 DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache \
    ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo \
    ${abs_top_builddir}/debuginfod/debuginfod-find || true) \
    2>&1 >/dev/null | grep -c 'Retry failed query'`
if [ $retry_attempts -ne 10 ]; then
    echo "retry mechanism failed."
    exit 1;
fi

exit 0;
