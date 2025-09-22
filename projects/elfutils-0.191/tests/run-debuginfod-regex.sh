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

mkdir F

DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB

export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=9400
get_ports
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod \
    $VERBOSE -F -p $PORT1 -t0 -g0 -d ${DB} F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1

# Wait till the server is ready and an initial scan has been done
wait_ready $PORT1 'ready' 1
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1

# Compile a simple program, strip its debuginfo and save the build-id.
# Also move the debuginfo into another directory so that elfutils
# cannot find it without debuginfod.
echo "int main() { return 0; }" > ${PWD}/prog.c
tempfiles prog.c
# Create a subdirectory to confound source path names
mkdir foobar
gcc -Wl,--build-id -g -o prog ${PWD}/foobar///./../prog.c
testrun ${abs_top_builddir}/src/strip -g -f prog.debug ${PWD}/prog

mv prog F
mv prog.debug F
tempfiles F/prog.debug F/prog

kill -USR1 $PID1
# Wait till both files are in the index.
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0
cp ${DB} ${DB}.backup
tempfiles ${DB}.backup

kill $PID1
wait $PID1
PID1=0

#######################################################################
## PR27711
# Test to ensure that the --include="^$" --exclude=".*" options remove all files from a database backup
#
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod \
    $VERBOSE -p $PORT2 -t0 -g0 --regex-groom --include="^$" --exclude=".*" -d ${DB}.backup > vlog$PORT2 2>&1 &

#reuse PID1
PID1=$!
tempfiles vlog$PORT2
errfiles vlog$PORT2
# Server must become ready
wait_ready $PORT2 'ready' 1

# Wait till the initial groom cycle has been done
wait_ready $PORT2 'thread_work_total{role="groom"}' 1
wait_ready $PORT2 'groom{statistic="archive d/e"}'  0
wait_ready $PORT2 'groom{statistic="archive sdef"}' 0
wait_ready $PORT2 'groom{statistic="archive sref"}' 0
wait_ready $PORT2 'groom{statistic="buildids"}' 0
wait_ready $PORT2 'groom{statistic="file d/e"}' 0
wait_ready $PORT2 'groom{statistic="file s"}' 0
wait_ready $PORT2 'groom{statistic="files scanned (#)"}' 0
wait_ready $PORT2 'groom{statistic="files scanned (mb)"}' 0

kill $PID1
wait $PID1
PID1=0

exit 0;

