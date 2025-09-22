#!/usr/bin/env bash
#
# Copyright (C) 2021 Red Hat, Inc.
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
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=9900
get_ports

# Test when debuginfod hitting X-Forwarded-For hops limit.
# This test will start two servers (as a loop) with two different hop limits.

tempfiles vlog$PORT1
errfiles vlog$PORT1
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS=http://127.0.0.1:$PORT2 ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d :memory: --forwarded-ttl-limit 0 -p $PORT1 > vlog$PORT1 2>&1 &
PID1=$!

tempfiles vlog$PORT2
errfiles vlog$PORT2
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS=http://127.0.0.1:$PORT1 ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d :memory: --forwarded-ttl-limit 1 -p $PORT2 > vlog$PORT2 2>&1 &
PID2=$!

wait_ready $PORT1 'ready' 1
wait_ready $PORT2 'ready' 1

export DEBUGINFOD_URLS="http://127.0.0.1:$PORT1/"
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo 01234567 || true

# Use a different buildid to avoid using same cache.
export DEBUGINFOD_URLS="http://127.0.0.1:$PORT2/"
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo 11234567 || true

grep "forwared-ttl-limit reached and will not query the upstream servers" vlog$PORT1
grep -v "forwared-ttl-limit reached and will not query the upstream servers" vlog$PORT2 | grep "not found" vlog$PORT2

kill $PID1 $PID2
wait $PID1 $PID2

PID1=0
PID2=0

exit 0
