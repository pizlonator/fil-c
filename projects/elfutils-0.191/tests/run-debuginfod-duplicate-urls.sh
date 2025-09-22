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
base=8600
get_ports

DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod \
    $VERBOSE -F -p $PORT1 -t0 -g0 -d ${DB} F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
wait_ready $PORT1 'ready' 1

########################################################################
## PR27983
# Ensure no duplicate urls are used in when querying servers for files
rm -rf $DEBUGINFOD_CACHE_PATH # clean it from previous tests
env DEBUGINFOD_URLS="http://127.0.0.1:$PORT1 http://127.0.0.1:$PORT1 http://127.0.0.1:$PORT1 http://127.0.0.1:7999" \
 LD_LIBRARY_PATH=$ldpath ${abs_top_builddir}/debuginfod/debuginfod-find -vvv executable 0 > vlog1 2>&1 || true
tempfiles vlog1
cat vlog1
if [ $( grep -c 'duplicate url: http://127.0.0.1:'$PORT1'.*' vlog1 ) -ne 2 ]; then
  echo "Duplicate servers remain";
  err
fi

kill $PID1
wait $PID1
PID1=0
exit 0
