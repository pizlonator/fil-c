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

mkdir Z
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=12000
get_ports

export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

cp -rvp ${abs_srcdir}/debuginfod-tars Z
tempfiles Z


for Cnum in "" "-C" "-C10" "-C100"
do
    env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE $Cnum -d :memory: -Z .tar.xz -Z .tar.bz2=bzcat -p $PORT1 -t0 -g0 -v --fdcache-fds=0 --fdcache-prefetch-fds=0 Z >> vlog$PORT1 2>&1 &
    PID1=$!
    tempfiles vlog$PORT1
    errfiles vlog$PORT1
    
    wait_ready $PORT1 'ready' 1
    # Wait for server to finish indexing
    wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
    wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
    wait_ready $PORT1 'thread_busy{role="scan"}' 0

    # Do a bunch of lookups in parallel
    lookup_nr=64
    for jobs in `seq $lookup_nr`; do
        curl -s http://localhost:$PORT1/buildid/cee13b2ea505a7f37bd20d271c6bc7e5f8d2dfcb/debuginfo > /dev/null &
    done

    # all curls should succeed
    wait_ready $PORT1 'http_responses_transfer_bytes_count{code="200",type="debuginfo"}' $lookup_nr
    
    (sleep 5;
     curl -s http://localhost:$PORT1/metrics | grep -E 'error|responses';
     kill $PID1) &
    wait # for all curls, the ()& from just above, and for debuginfod
    PID1=0
done

# Note this xfail comes too late, the above wait_ready for
# http_responses_transfer_bytes_count will have failed.
xfail "grep Server.reached.connection vlog$PORT1" # PR28661

exit 0
