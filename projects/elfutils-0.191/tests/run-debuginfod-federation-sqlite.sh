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

DB=${PWD}/.debuginfod_tmp.sqlite
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache
tempfiles $DB

# Clean old dirictories
mkdir D L F
# not tempfiles F R L D Z - they are directories which we clean up manually
ln -s ${abs_builddir}/dwfllines L/foo   # any program not used elsewhere in this test

########################################################################
# Compile a simple program, strip its debuginfo and save the build-id.
# Also move the debuginfo into another directory so that elfutils
# cannot find it without debuginfod.
echo "int main() { return 0; }" > ${PWD}/prog.c
tempfiles prog.c
# Create a subdirectory to confound source path names
mkdir foobar
gcc -Wl,--build-id -g -o prog ${PWD}/foobar///./../prog.c
testrun ${abs_top_builddir}/src/strip -g -f prog.debug ${PWD}/prog
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a prog | grep 'Build ID' | cut -d ' ' -f 7`

mv prog F
mv prog.debug F
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=9100
get_ports
# Launch server which will be unable to follow symlinks
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d ${DB} -F -U -t0 -g0 -p $PORT1 L D F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1

wait_ready $PORT1 'ready' 1

# Wait till initial scan is done
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache2
mkdir -p $DEBUGINFOD_CACHE_PATH
# NB: run in -L symlink-following mode for the L subdir
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS=http://127.0.0.1:$PORT1 ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d ${DB}_2 -F -U -p $PORT2 -L L D F > vlog$PORT2 2>&1 &
PID2=$!
tempfiles vlog$PORT2
errfiles vlog$PORT2
tempfiles ${DB}_2
wait_ready $PORT2 'ready' 1
# Wait till initial scan is done
wait_ready $PORT2 'thread_work_total{role="traverse"}' 1
# And initial groom cycle
wait_ready $PORT1 'thread_work_total{role="groom"}' 1

export DEBUGINFOD_URLS='http://127.0.0.1:'$PORT2
# Use fresh cache for debuginfod-find client requests
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache3
mkdir -p $DEBUGINFOD_CACHE_PATH

if type bsdtar 2>/dev/null; then
    # copy in the deb files
    cp -rvp ${abs_srcdir}/debuginfod-debs/*deb D
    kill -USR1 $PID2
    wait_ready $PORT2 'thread_work_total{role="traverse"}' 2
    wait_ready $PORT2 'thread_work_pending{role="scan"}' 0
    wait_ready $PORT2 'thread_busy{role="scan"}' 0

    # All debs need to be in the index
    debs=$(find D -name \*.deb | wc -l)
    wait_ready $PORT2 'scanned_files_total{source=".deb archive"}' `expr $debs`
    ddebs=$(find D -name \*.ddeb | wc -l)
    wait_ready $PORT2 'scanned_files_total{source=".ddeb archive"}' `expr $ddebs`

    # ubuntu
    archive_test f17a29b5a25bd4960531d82aa6b07c8abe84fa66 "" ""
fi

testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID

# send a request to stress XFF and User-Agent federation relay;
# we'll grep for the two patterns in vlog$PORT1
curl -s -H 'User-Agent: TESTCURL' -H 'X-Forwarded-For: TESTXFF' $DEBUGINFOD_URLS/buildid/deaddeadbeef00000000/debuginfo -o /dev/null || true

grep UA:TESTCURL vlog$PORT1
grep XFF:TESTXFF vlog$PORT1

# confirm that first server can't resolve symlinked info in L/ but second can
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
         -a L/foo | grep 'Build ID' | cut -d ' ' -f 7`
file L/foo
file -L L/foo
export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1
rm -rf $DEBUGINFOD_CACHE_PATH
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID && false || true
rm -f $DEBUGINFOD_CACHE_PATH/$BUILDID/debuginfo # drop negative-hit file
export DEBUGINFOD_URLS=http://127.0.0.1:$PORT2
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID

# test again with scheme free url
export DEBUGINFOD_URLS=127.0.0.1:$PORT1
rm -rf $DEBUGINFOD_CACHE_PATH
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID && false || true
rm -f $DEBUGINFOD_CACHE_PATH/$BUILDID/debuginfo # drop negative-hit file
export DEBUGINFOD_URLS=127.0.0.1:$PORT2
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
# test parallel queries in client
rm -rf $DEBUGINFOD_CACHE_PATH
export DEBUGINFOD_URLS="BAD http://127.0.0.1:$PORT1 127.0.0.1:$PORT1 http://127.0.0.1:$PORT2 DNE"

testrun ${abs_builddir}/debuginfod_build_id_find -e F/prog 1

# And generate a few errors into the second debuginfod's logs, for analysis just below
curl -s http://127.0.0.1:$PORT2/badapi > /dev/null || true
curl -s http://127.0.0.1:$PORT2/buildid/deadbeef/debuginfo > /dev/null || true
# NB: this error is used to seed the 404 failure for the survive-404 tests

# Confirm bad artifact types are rejected without leaving trace
curl -s http://127.0.0.1:$PORT2/buildid/deadbeef/badtype > /dev/null || true
(curl -s http://127.0.0.1:$PORT2/metrics | grep 'badtype') && false

# Confirm that reused curl connections survive 404 errors.
# The rm's force an uncached fetch (in both servers and client cache)
rm -f .client_cache*/$BUILDID/debuginfo
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
rm -f .client_cache*/$BUILDID/debuginfo
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
rm -f .client_cache*/$BUILDID/debuginfo
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
# Trigger a flood of requests against the same archive content file.
# Use a file that hasn't been previously extracted in to make it
# likely that even this test debuginfod will experience concurrency
# and impose some "after-you" delays.
maxreq=$[$(nproc) * 4]
maxreq=$(( $maxreq > 64 ? 64 : $maxreq ))
(for i in `seq $maxreq`; do
    curl -s http://127.0.0.1:$PORT1/buildid/87c08d12c78174f1082b7c888b3238219b0eb265/executable >/dev/null &
 done;
 wait)
curl -s http://127.0.0.1:$PORT1/metrics | grep 'http_responses_after_you.*'
# If we could guarantee some minimum number of seconds of CPU time, we
# could assert that the after_you metrics show some nonzero amount of
# waiting.  A few hundred ms is typical on this developer's workstation.

########################################################################
# Trigger some some random activity, then trigger a clean shutdown.
# We used to try to corrupt the database while the debuginfod server
# was running and check it detected errors, but that was unreliably
# and slightly dangerous since part of the database was already mapped
# into memory.
kill -USR1 $PID1
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0
kill -USR2 $PID1
wait_ready $PORT1 'thread_work_total{role="groom"}' 2
curl -s http://127.0.0.1:$PORT1/buildid/beefbeefbeefd00dd00d/debuginfo > /dev/null || true

kill -INT $PID1 $PID2
wait $PID1 $PID2
PID1=0
PID2=0

# Run the tests again without the servers running. The target file should
# be found in the cache.

tempfiles .debuginfod_*

testrun ${abs_builddir}/debuginfod_build_id_find -e F/prog 1

# check out the debuginfod logs for the new style status lines
cat vlog$PORT2
grep -q 'UA:.*XFF:.*GET /buildid/.* 200 ' vlog$PORT2
grep -q 'UA:.*XFF:.*GET /metrics 200 ' vlog$PORT2
grep -q 'UA:.*XFF:.*GET /badapi 503 ' vlog$PORT2
grep -q 'UA:.*XFF:.*GET /buildid/deadbeef.* 404 ' vlog$PORT2

exit 0
