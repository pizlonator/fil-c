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

# Set up directories for scanning
mkdir F
mkdir R
cp -rvp ${abs_srcdir}/debuginfod-rpms R

# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=13000
get_ports

# We use -t0 and -g0 here to turn off time-based scanning & grooming.
# For testing purposes, we just sic SIGUSR1 at the process.

env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS= ${abs_builddir}/../debuginfod/debuginfod $VERBOSE \
    -R R -F F -p $PORT1 -d $DB -t0 -g0 -v F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
# And initial scan should be done
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1

export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1

# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

########################################################################

# Compile a simple program, strip its debuginfo and save the build-id.
# Also move the debuginfo into another directory so that elfutils
# cannot find it without debuginfod.
tempfiles prog.c
echo "int main() { return 0; }" > ${PWD}/prog.c

gcc -Wl,--build-id -g -o F/prog ${PWD}/prog.c

testrun ${abs_top_builddir}/src/strip -g -f F/prog.debug ${PWD}/F/prog
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a F/prog | grep 'Build ID' | cut -d ' ' -f 7`

kill -USR1 $PID1
# Wait till both files are in the index.
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

########################################################################

# Build-id for a file in the one of the testsuite's F31 rpms
RPM_BUILDID=d44d42cbd7d915bc938c81333a21e355a6022fb7

# Download sections from files indexed with -F
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $BUILDID .debug_info
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $BUILDID .text

# Download sections from files indexed with -R
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $RPM_BUILDID .debug_info
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $RPM_BUILDID .text

# Verify that the downloaded files match the contents of the original sections
tempfiles ${BUILDID}.debug_info
objcopy F/prog.debug -O binary --only-section=.debug_info --set-section-flags .debug_info=alloc $BUILDID.debug_info
cmp ${BUILDID}.debug_info ${DEBUGINFOD_CACHE_PATH}/${BUILDID}/section-.debug_info

tempfiles ${BUILDID}.text
objcopy F/prog -O binary --only-section=.text ${BUILDID}.text
cmp ${BUILDID}.text ${DEBUGINFOD_CACHE_PATH}/${BUILDID}/section-.text

# Download the original debuginfo/executable files.
DEBUGFILE=`env LD_LIBRARY_PATH=$ldpath ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $RPM_BUILDID`
EXECFILE=`env LD_LIBRARY_PATH=$ldpath ${abs_top_builddir}/debuginfod/debuginfod-find executable $RPM_BUILDID`
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv debuginfo $BUILDID
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv executable $BUILDID

if test "$(arch)" == "x86_64"; then
  tempfiles DEBUGFILE.debug_info
  objcopy $DEBUGFILE -O binary --only-section=.debug_info --set-section-flags .debug_info=alloc DEBUGFILE.debug_info
  testrun diff -u DEBUGFILE.debug_info ${DEBUGINFOD_CACHE_PATH}/${RPM_BUILDID}/section-.debug_info

  tempfiles EXECFILE.text
  objcopy $EXECFILE -O binary --only-section=.text EXECFILE.text
  testrun diff -u EXECFILE.text ${DEBUGINFOD_CACHE_PATH}/${RPM_BUILDID}/section-.text
fi

# Kill the server.
kill $PID1
wait $PID1
PID1=0

# Delete the section files from the cache.
rm -f ${DEBUGINFOD_CACHE_PATH}/${RPM_BUILDID}/section-.text
rm -f ${DEBUGINFOD_CACHE_PATH}/${RPM_BUILDID}/section-.debug_info
rm -f ${DEBUGINFOD_CACHE_PATH}/${BUILDID}/section-.text
rm -f ${DEBUGINFOD_CACHE_PATH}/${BUILDID}/section-.debug_info

# Verify that the client can extract sections from the debuginfo or executable
# if they're already in the cache.
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $BUILDID .debug_info
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $BUILDID .text
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $RPM_BUILDID .debug_info
testrun ${abs_top_builddir}/debuginfod/debuginfod-find -vvv section $RPM_BUILDID .text

exit 0
