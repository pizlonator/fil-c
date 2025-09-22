#!/usr/bin/env bash
#
# Copyright (C) 2019-2023 Red Hat, Inc.
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

mkdir R Z
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=10100
get_ports

DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

cp -rvp ${abs_srcdir}/debuginfod-rpms R

if [ "$zstd" = "false" ]; then  # nuke the zstd fedora 31 ones
    rm -vrf R/debuginfod-rpms/fedora31
    rpms=0
    tarxz=1
    groomed=2
else
    rpms=6
    tarxz=1
    groomed=8
fi

cp -rvp ${abs_srcdir}/debuginfod-tars Z

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d $DB -X 'hello2' -I 'hello' -R -Z .tar.xz -Z .tar.bz2=bzcat -p $PORT1 -t0 -g0 -v R Z > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1

# Wait till both files are in the index and scan/index fully finished
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

# Confirm that the hello3 files are excluded

wait_ready $PORT1 'scanned_files_total{source=".rpm archive"}' $rpms
wait_ready $PORT1 'scanned_files_total{source=".tar.xz archive"}' $tarxz


kill $PID1
wait $PID1
PID1=0

# Start second debuginfod, with exclusion of everything

echo 'RERUN WITH -r and -X everything' >> vlog$PORT1

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d $DB -r -X '.*' -F -R -Z .tar.xz -Z .tar.bz2=bzcat -p $PORT1 -t0 -g0 -v R Z >> vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1

wait_ready $PORT1 'thread_work_total{role="groom"}' 1
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

wait_ready $PORT1 'groomed_total{decision="stale"}' $groomed
wait_ready $PORT1 'groom{statistic="files scanned (#)"}' 0

kill $PID1
wait $PID1
PID1=0

exit 0
