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
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=8200
get_ports
DB=${PWD}/.debuginfod_tmp.sqlite
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache
tempfiles $DEBUGINFOD_CACHE_PATH $DB
# Clean old dirictories
mkdir R ${PWD}/F
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE \
    -F -R -d $DB -p $PORT1 -t0 -g0 -v R ${PWD}/F > vlog$PORT1 2>&1 &

PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

# wait till the initial scan is done before triggering a new one
# and before dropping new file into the scan dirs
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
# Same for the initial groom cycle, we don't want it to be done
# half way initializing the file setup
wait_ready $PORT1 'thread_work_total{role="groom"}' 1

cp -rvp ${abs_srcdir}/debuginfod-rpms R
if [ "$zstd" = "false" ]; then  # nuke the zstd fedora 31 ones
    rm -vrf R/debuginfod-rpms/fedora31
fi

kill -USR1 $PID1
# Now there should be 1 files in the index
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

# common source file sha1
SHA=f4a1a8062be998ae93b8f1cd744a398c6de6dbb1

########################################################################
## PR26810: Now rename some files in the R directory, then rescan, so
# there are two copies of the same buildid in the index, one for the
# no-longer-existing file name, and one under the new name.

# run a groom cycle to force server to drop its fdcache
kill -USR2 $PID1  # groom cycle
wait_ready $PORT1 'thread_work_total{role="groom"}' 2
# move it around a couple of times to make it likely to hit a nonexistent entry during iteration
mv R/debuginfod-rpms/rhel7 R/debuginfod-rpms/rhel7renamed
kill -USR1 $PID1  # scan cycle
wait_ready $PORT1 'thread_work_total{role="traverse"}' 3
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0
mv R/debuginfod-rpms/rhel7renamed R/debuginfod-rpms/rhel7renamed2
kill -USR1 $PID1  # scan cycle
wait_ready $PORT1 'thread_work_total{role="traverse"}' 4
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0
mv R/debuginfod-rpms/rhel7renamed2 R/debuginfod-rpms/rhel7renamed3
kill -USR1 $PID1  # scan cycle
wait_ready $PORT1 'thread_work_total{role="traverse"}' 5
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1

# retest rhel7
archive_test bc1febfd03ca05e030f0d205f7659db29f8a4b30 /usr/src/debug/hello-1.0/hello.c $SHA
archive_test f0aa15b8aba4f3c28cac3c2a73801fefa644a9f2 /usr/src/debug/hello-1.0/hello.c $SHA

grep -E '(libc.error.*rhel7)|(bc1febfd03ca)|(f0aa15b8aba)' vlog$PORT1

kill $PID1
wait $PID1
PID1=0
exit 0;
