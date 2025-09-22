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

. $srcdir/debuginfod-subr.sh  # includes set -e
# for test case debugging, uncomment:
set -x
unset VALGRIND_CMD
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=10000
get_ports
DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
mkdir F
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE \
    -F -R -d $DB -p $PORT1 -t0 -g0 -v R ${PWD}/F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
# And initial scan should be done
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1

# Build a non-stripped binary
echo "int main() { return 0; }" > ${PWD}/F/p++r\$\#o^^g.c
gcc -Wl,--build-id -g -o ${PWD}/F/p++r\$\#o^^g ${PWD}/F/p++r\$\#o^^g.c
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a ${PWD}/F/p++r\\$\#o^^g | grep 'Build ID' | cut -d ' ' -f 7`
tempfiles ${PWD}/F/p++r\$\#o^^g.c ${PWD}/F/p++r\$\#o^^g
kill -USR1 $PID1
# Now there should be 1 files in the index
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0
rm -rf $DEBUGINFOD_CACHE_PATH # clean it from previous tests
ls F
env DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache DEBUGINFOD_URLS="http://127.0.0.1:$PORT1" \
    LD_LIBRARY_PATH=$ldpath ${abs_top_builddir}/debuginfod/debuginfod-find -vvv source F/p++r\$\#o^^g ${abs_builddir}/F/p++r\$\#o^^g.c > vlog1 2>&1 || true
tempfiles vlog1
grep 'F/p%2B%2Br%24%23o%5E%5Eg.c' vlog1

kill $PID1
wait $PID1
PID1=0
exit 0
