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

mkdir F
mkdir -p $DEBUGINFOD_CACHE_PATH
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=9700
get_ports

env LD_LIBRARY_PATH=$ldpath DEBUGINFOD_URLS= ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -F -p $PORT1 -d $DB \
    -t0 -g0 -v F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
# And initial scan should be done
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1

export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1/   # or without trailing /

# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

# We use -t0 and -g0 here to turn off time-based scanning & grooming.
# For testing purposes, we just sic SIGUSR1 / SIGUSR2 at the process.

########################################################################

# Compile a simple program, strip its debuginfo and save the build-id.
# Also move the debuginfo into another directory so that elfutils
# cannot find it without debuginfod.
echo "int main() { return 0; }" > ${PWD}/p+r%o\$g.c
tempfiles p+r%o\$g.c
# Create a subdirectory to confound source path names
mkdir foobar
gcc -Wl,--build-id -g -o p+r%o\$g ${PWD}/foobar///./../p+r%o\$g.c
testrun ${abs_top_builddir}/src/strip -g -f p+r%o\$g.debug ${PWD}/p+r%o\$g
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a p+r%o\\$g | grep 'Build ID' | cut -d ' ' -f 7`

mv p+r%o\$g F
mv p+r%o\$g.debug F
kill -USR1 $PID1
# Wait till both files are in the index.
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

########################################################################

# Test whether the cache default locations are correct
mkdir tmphome

# $HOME/.cache should be created.
testrun env HOME=$PWD/tmphome XDG_CACHE_HOME= DEBUGINFOD_CACHE_PATH= ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
if [ ! -f $PWD/tmphome/.cache/debuginfod_client/$BUILDID/debuginfo ]; then
  echo "could not find cache in $PWD/tmphome/.cache"
  err
fi

# $HOME/.cache should be found.
testrun env HOME=$PWD/tmphome XDG_CACHE_HOME= DEBUGINFOD_CACHE_PATH= ${abs_top_builddir}/debuginfod/debuginfod-find executable $BUILDID
if [ ! -f $PWD/tmphome/.cache/debuginfod_client/$BUILDID/executable ]; then
  echo "could not find cache in $PWD/tmphome/.cache"
  err
fi
# $XDG_CACHE_HOME should take priority over $HOME.cache.
testrun env HOME=$PWD/tmphome XDG_CACHE_HOME=$PWD/tmpxdg DEBUGINFOD_CACHE_PATH= ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
if [ ! -f $PWD/tmpxdg/debuginfod_client/$BUILDID/debuginfo ]; then
  echo "could not find cache in $PWD/tmpxdg/"
  err
fi

# A cache at the old default location ($HOME/.debuginfod_client_cache) should take
# priority over $HOME/.cache, $XDG_CACHE_HOME.
cp -vr $DEBUGINFOD_CACHE_PATH tmphome/.debuginfod_client_cache || true
# ||true is for tolerating errors, such a valgrind or something else
#        leaving negative-hit files in there

# Add a file that doesn't exist in $HOME/.cache, $XDG_CACHE_HOME.
mkdir tmphome/.debuginfod_client_cache/deadbeef
echo ELF... > tmphome/.debuginfod_client_cache/deadbeef/debuginfo
filename=`testrun env HOME=$PWD/tmphome XDG_CACHE_HOME=$PWD/tmpxdg DEBUGINFOD_CACHE_PATH= ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo deadbeef`
cmp $filename tmphome/.debuginfod_client_cache/deadbeef/debuginfo

# $DEBUGINFO_CACHE_PATH should take priority over all else.
testrun env HOME=$PWD/tmphome XDG_CACHE_HOME=$PWD/tmpxdg DEBUGINFOD_CACHE_PATH=$PWD/tmpcache ${abs_top_builddir}/debuginfod/debuginfod-find debuginfo $BUILDID
if [ ! -f $PWD/tmpcache/$BUILDID/debuginfo ]; then
  echo "could not find cache in $PWD/tmpcache/"
  err
fi
rm -rf ${PWD}/tmphome/ ${PWD}/tmpxdg ${PWD}/tmpcache

kill $PID1
wait $PID1
PID1=0
exit 0
