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

mkdir Z
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=11000
get_ports

DB=${PWD}/.debuginfod.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

cp -rvp ${abs_srcdir}/debuginfod-tars Z
tempfiles Z

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d $DB -Z .tar.xz -Z .tar.bz2=bzcat -p $PORT1 -t0 -g0 -v Z > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1

wait_ready $PORT1 'ready' 1

# Start second passive server with same database
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE --passive -d $DB -Z .tar.xz -Z .tar.bz2=bzcat -p $PORT2 -v > vlog$PORT2 2>&1 &
PID2=$!

tempfiles vlog$PORT2
errfiles vlog$PORT2

wait_ready $PORT2 'ready' 1

# Wait for first server to finish indexing
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

# No similar metrics for the passive server
! (curl http://localhost:$PORT2/metrics | grep -E 'role="scan"|role="groom"|role="traverse"')

# Confirm no active threads
! (ps -q $PID2 -e -L -o '%p %c %a' | grep -E 'scan|groom|traverse')

# Do a random lookup via passive server
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS=http://localhost:$PORT2 ${abs_builddir}/../debuginfod/debuginfod-find debuginfo cee13b2ea505a7f37bd20d271c6bc7e5f8d2dfcb

tempfiles $DB*

kill $PID1
wait $PID1
PID1=0

kill $PID2
wait $PID2
PID2=0

exit 0
