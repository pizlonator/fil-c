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

mkdir R Z
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=8700
get_ports

DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -d $DB -F -R -Z .tar.xz -Z .tar.bz2=bzcat -p $PORT1 --scan-checkpoint=1 -t0 -g0 -v R Z > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1

# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

# Make sure the initial scan has finished before copying the new files in
# We might remove some, which we don't want to be accidentally scanned.
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1

cp -rvp ${abs_srcdir}/debuginfod-rpms R
if [ "$zstd" = "false" ]; then  # nuke the zstd fedora 31 ones
    rm -vrf R/debuginfod-rpms/fedora31
fi

cp -rvp ${abs_srcdir}/debuginfod-tars Z

kill -USR1 $PID1
# Wait till both files are in the index and scan/index fully finished
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

# Take a dump if possible
type sqlite3 2>/dev/null && sqlite3 $DB '.d'

########################################################################
# All rpms need to be in the index, except the dummy permission-000 one
rpms=$(find R -name \*rpm | grep -v nothing | wc -l)
wait_ready $PORT1 'scanned_files_total{source=".rpm archive"}' $rpms
txz=$(find Z -name \*tar.xz | wc -l)
wait_ready $PORT1 'scanned_files_total{source=".tar.xz archive"}' $txz
tb2=$(find Z -name \*tar.bz2 | wc -l)
wait_ready $PORT1 'scanned_files_total{source=".tar.bz2 archive"}' $tb2

kill -USR1 $PID1  # two hits of SIGUSR1 may be needed to resolve .debug->dwz->srefs
# Wait till both files are in the index and scan/index fully finished
wait_ready $PORT1 'thread_work_total{role="traverse"}' 3
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

# Expect all source files found in the rpms (they are all called hello.c :)
# We will need to extract all rpms (in their own directory) and could all
# sources referenced in the .debug files.
mkdir extracted
cd extracted
subdir=0;
newrpms=$(find ../R -name \*\.rpm | grep -v nothing)
for i in $newrpms; do
    subdir=$[$subdir+1];
    mkdir $subdir;
    cd $subdir;
    ls -lah ../$i
    rpm2cpio ../$i | cpio -ivd;
    cd ..;
done
sourcefiles=$(find -name \*\\.debug \
              | env LD_LIBRARY_PATH=$ldpath xargs \
                ${abs_top_builddir}/src/readelf --debug-dump=decodedline \
              | grep mtime: | wc --lines)
cd ..
rm -rf extracted

wait_ready $PORT1 'found_sourcerefs_total{source=".rpm archive"}' $sourcefiles

kill $PID1
wait $PID1
PID1=0
exit 0
