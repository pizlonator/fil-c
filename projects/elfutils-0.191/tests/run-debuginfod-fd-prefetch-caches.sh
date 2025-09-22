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

# Set up archive folders for scanning
mkdir R Z
cp -rvp ${abs_srcdir}/debuginfod-tars Z
cp -rvp ${abs_srcdir}/debuginfod-rpms R
if [ "$zstd" = "false" ]; then  # nuke the zstd fedora 31 ones
    rm -vrf R/debuginfod-rpms/fedora31
fi

# Set up maximums. 1 is just a convenient cap to test on.
FDCACHE_MBS=1
FDCACHE_FDS=1
PREFETCH_FDS=1
PREFETCH=2
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=8800
get_ports

DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

######
# Test fd limits of fd and prefetch cache
######
rm -rf $DEBUGINFOD_CACHE_PATH
rm -rf $DB
# Testing prefetch fd maximum (Set mb maximums to be beyond consideration)
# Set --fdcache-mintmp=0 so we don't accidentally trigger an fdcache
# emergency flush for filling tmpdir
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS= ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -p $PORT1 -d $DB \
    --fdcache-fds=$FDCACHE_FDS --fdcache-prefetch-fds=$PREFETCH_FDS  -vvvvv -g 0 -t 0 \
    -Z .tar.bz2=bzcat Z  --fdcache-mbs=100  --fdcache-prefetch-mbs=100 \
    --fdcache-mintmp=0 --fdcache-prefetch=$PREFETCH > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
########################################################################
kill -USR1 $PID1
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0
export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1/
# load prefetch cache with debuginfod-tars/usr/src/debug/hello.c
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo cee13b2ea505a7f37bd20d271c6bc7e5f8d2dfcb
metrics=$(curl http://127.0.0.1:$PORT1/metrics)
regex="fdcache_prefetch_count ([0-9])+"
# Check to see if prefetch cache is maximally loaded. Note fdcache-prefetch (2) > prefetch-fds (1),
# so the debuginfod will try to load the prefetch cache with 2 files. We want prefetch-fds to cap that
# off
if [[ $metrics =~ $regex ]]; then
  if [[ ${BASH_REMATCH[1]} -ne $PREFETCH_FDS ]]; then
    err
  fi
else
  err
fi 

testrun ${abs_top_builddir}/debuginfod/debuginfod-find source cee13b2ea505a7f37bd20d271c6bc7e5f8d2dfcb /usr/src/debug/hello.c
metrics=$(curl http://127.0.0.1:$PORT1/metrics)
regex="fdcache_op_count\{op=\"prefetch_access\"\} ([0-9])+"
if [[ $metrics =~ $regex ]]; then
  # In the test above hello.c should've been loaded into the prefetch cache.
  # Ensure that searching for hello.c a second time accesses the prefetch cache once
  if [[  ${BASH_REMATCH[1]} -ne 1 ]]; then
    err
  fi
else
  err
fi

kill $PID1
wait $PID1
PID1=0

#########
# Test mb limit on fd cache
#########
rm -rf $DEBUGINFOD_CACHE_PATH
rm -rf $DB
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS= ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -p $PORT1 -d $DB \
    --fdcache-mbs=$FDCACHE_MBS -vvvvv -g 0 -t 0 -R R > vlog2$PORT1 2>&1 &
PID1=$!
tempfiles vlog2$PORT1
errfiles vlog2$PORT1
wait_ready $PORT1 'ready' 1
########################################################################
kill -USR1 $PID1
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
# All rpms need to be in the index, except the dummy permission-000 one
rpms=$(find R -name \*rpm | grep -v nothing | wc -l)
wait_ready $PORT1 'scanned_files_total{source=".rpm archive"}' $rpms
kill -USR1 $PID1
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

# This many archives cause the fd cache to be loaded with just over 1mb of
# files. So we expect the 1mb cap off
SHA=f4a1a8062be998ae93b8f1cd744a398c6de6dbb1
archive_test bc1febfd03ca05e030f0d205f7659db29f8a4b30 /usr/src/debug/hello-1.0/hello.c $SHA
archive_test c36708a78618d597dee15d0dc989f093ca5f9120 /usr/src/debug/hello2-1.0-2.x86_64/hello.c $SHA
archive_test 41a236eb667c362a1c4196018cc4581e09722b1b /usr/src/debug/hello2-1.0-2.x86_64/hello.c $SHA
archive_test bc1febfd03ca05e030f0d205f7659db29f8a4b30 /usr/src/debug/hello-1.0/hello.c $SHA
archive_test f0aa15b8aba4f3c28cac3c2a73801fefa644a9f2 /usr/src/debug/hello-1.0/hello.c $SHA
archive_test bbbf92ebee5228310e398609c23c2d7d53f6e2f9 /usr/src/debug/hello-1.0/hello.c $SHA
metrics=$(curl http://127.0.0.1:$PORT1/metrics)
regex="fdcache_bytes ([0-9]+)"
# Since the server metrics report in bytes, $mb is just the total number of bytes allocated
# to the fd cache. Ensure that this cap isn't crossed
mb=$(($FDCACHE_MBS*1024*1024))
if [[ $metrics =~ $regex ]]; then
  fdbytes=${BASH_REMATCH[1]}
  if [ $fdbytes -gt $mb ] ; then
    err
  fi
else
  err
fi

kill $PID1
wait $PID1
PID1=0
exit 0
