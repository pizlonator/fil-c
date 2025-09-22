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
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=8000
get_ports
env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS= ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -F -p $PORT1 -d $DB \
    -t0 -g0 -v ${PWD} > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1/   # or without trailing /

# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

########################################################################
# PR25628
rm -rf $DEBUGINFOD_CACHE_PATH # clean it from previous tests

# The query is designed to fail, while the empty file should be created.
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo 01234567 || true
if [ ! -f $DEBUGINFOD_CACHE_PATH/01234567/debuginfo ]; then
  echo "could not find cache in $DEBUGINFOD_CACHE_PATH"
  err
fi

if [ `stat -c "%s" $DEBUGINFOD_CACHE_PATH/01234567/debuginfo` != 0 ]; then
  echo "The cache $DEBUGINFOD_CACHE_PATH/01234567/debuginfo is not empty"
  err
fi

bytecount_before=`curl -s http://127.0.0.1:$PORT1/metrics | grep 'http_responses_transfer_bytes_count{code="404",type="debuginfo"}'`
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo 01234567 || true
bytecount_after=`curl -s http://127.0.0.1:$PORT1/metrics | grep 'http_responses_transfer_bytes_count{code="404",type="debuginfo"}'`
if [ "$bytecount_before" != "$bytecount_after" ]; then
  echo "http_responses_transfer_bytes_count{code="404",type="debuginfo"} has changed."
  err
fi

# set cache_miss_s to 0 and sleep 1 to make the mtime expire.
echo 0 > $DEBUGINFOD_CACHE_PATH/cache_miss_s
sleep 1
bytecount_before=`curl -s http://127.0.0.1:$PORT1/metrics | grep 'http_responses_transfer_bytes_count{code="404",type="debuginfo"}'`
testrun ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo 01234567 || true
bytecount_after=`curl -s http://127.0.0.1:$PORT1/metrics | grep 'http_responses_transfer_bytes_count{code="404",type="debuginfo"}'`
if [ "$bytecount_before" == "$bytecount_after" ]; then
  echo "http_responses_transfer_bytes_count{code="404",type="debuginfo"} should be incremented."
  err
fi

kill $PID1
wait $PID1
PID1=0
exit 0
