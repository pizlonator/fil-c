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
base=8400
get_ports
mkdir F

DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

echo 'int main(int argc, char * argv){ return 0; }' > ${PWD}/prog.c
gcc -Wl,--build-id -g -o prog ${PWD}/prog.c
testrun ${abs_top_builddir}/src/strip -g -f prog.debug ${PWD}/prog
tempfiles prog prog.debug prog.c
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a prog | grep 'Build ID' | cut -d ' ' -f 7`
export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d $DB -F -p $PORT1 -t0 -g0 -v F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1

# Server must become ready
wait_ready $PORT1 'ready' 1
# And the initial scan should have been done before moving
# files under the scan dirs.
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1

mv prog F
mv prog.debug F
tempfiles prog/F

kill -USR1 $PID1
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

# Add artifacts to the search paths and test whether debuginfod finds them while already running.
# Build another, non-stripped binary
echo "int main() { return 0; }" > ${PWD}/prog2.c
tempfiles prog2.c
gcc -Wl,--build-id -g -o prog2 ${PWD}/prog2.c
#testrun ${abs_top_builddir}/src/strip -g -f prog.debug ${PWD}/prog
BUILDID2=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a prog2 | grep 'Build ID' | cut -d ' ' -f 7`
mv prog2 F
#mv prog2.debug F
tempfiles F/prog2 F/prog2.debug

kill -USR1 $PID1
# Now there should be 3 files in the index
wait_ready $PORT1 'thread_work_total{role="traverse"}' 3
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

########################################################################

# Test whether debuginfod-find is able to fetch those files.
rm -rf $DEBUGINFOD_CACHE_PATH # clean it from previous tests
filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID`
cmp $filename F/prog.debug

filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find executable F/prog`
cmp $filename F/prog

# raw source filename
filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find source $BUILDID ${PWD}/foobar///./../prog.c`
cmp $filename  ${PWD}/prog.c

# and also the canonicalized one
filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find source $BUILDID ${PWD}/prog.c`
cmp $filename  ${PWD}/prog.c

# Rerun same tests for the prog2 binary
filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find -v debuginfo $BUILDID2 2>vlog`
cmp $filename F/prog2
grep -q Progress vlog
grep -q Downloaded.from vlog
tempfiles vlog
filename=`testrun env DEBUGINFOD_PROGRESS=1 ${abs_top_builddir}/debuginfod/debuginfod-find executable $BUILDID2 2>vlog2`
cmp $filename F/prog2
grep -q 'Downloading.*http' vlog2
tempfiles vlog2
filename=`testrun ${abs_top_builddir}/debuginfod/debuginfod-find source $BUILDID2 ${PWD}/prog2.c`
cmp $filename ${PWD}/prog2.c


kill $PID1
wait $PID1
PID1=0
exit 0
