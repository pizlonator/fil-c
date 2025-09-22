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

set -x
unset VALGRIND_CMD
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=9600
get_ports

DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

echo "int main() { return 0; }" > ${PWD}/prog.c
# Create a subdirectory to confound source path names
mkdir foobar
gcc -Wl,--build-id -g -o prog ${PWD}/foobar///./../prog.c
testrun ${abs_top_builddir}/src/strip -g -f prog.debug ${PWD}/prog
tempfiles prog prog.debug prog.c
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a prog | grep 'Build ID' | cut -d ' ' -f 7`

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -F -p $PORT1 -d $DB -t0 -g0 ${PWD} > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles  vlog$PORT1
wait_ready $PORT1 'ready' 1
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

## PR27892
# Ensure DEBUGINFOD_MAXSIZE is functional and sends back the correct http
# code
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_RETRY_LIMIT=1 DEBUGINFOD_URLS="http://127.0.0.1:$PORT1/" DEBUGINFOD_MAXSIZE=1 \
    ${abs_top_builddir}/debuginfod/debuginfod-find -v -v executable ${PWD}/prog 2> find-vlog$PORT1 || true
tempfiles find-vlog$PORT1
errfiles  find-vlog$PORT1
echo "Checking maxsize"
grep "using max size 1B" find-vlog$PORT1
echo "Checking maxsize"
grep 'serving file '$(realpath ${PWD})'/prog' vlog$PORT1
echo "Checking maxsize"
grep 'File too large' vlog$PORT1
if [ -f ${DEBUGINFOD_CACHE_PATH}/${BUILDID} ]; then
  echo "File cached after maxsize check"
  err
fi
# Ensure no file is downloaded for longer than DEBUGINFOD_MAXTIME
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS="http://127.0.0.1:$PORT1/" DEBUGINFOD_MAXTIME=1 \
    ${abs_top_builddir}/debuginfod/debuginfod-find -v -v debuginfo ${PWD}/prog.debug 2> find-vlog$PORT1 || true
tempfiles find-vlog$PORT1
grep 'using max time' find-vlog$PORT1
# Ensure p+r%o\$g.debug is NOT cached
if [ -f ${DEBUGINFOD_CACHE_PATH}/${BUILDID} ]; then
  echo "File cached after maxtime check"
  err
fi

kill $PID1
wait $PID1
PID1=0

exit 0;
