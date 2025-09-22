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
base=8100
get_ports
DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

# Clean old dirictories
mkdir R ${PWD}/F
env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE \
    -F -R -d $DB -p $PORT1 -t0 -g0 -v R ${PWD}/F > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
# Be patient when run on a busy machine things might take a bit.

# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

# wait till the initial scan is done before triggering a new one
# and before dropping new file into the scan dirs
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
# Same for the initial groom cycle, we don't want it to be done
# half way initializing the file setup
wait_ready $PORT1 'thread_work_total{role="groom"}' 1

# Build a non-stripped binary
echo "int main() { return 0; }" > ${PWD}/F/prog.c
gcc -Wl,--build-id -g -o ${PWD}/F/prog ${PWD}/F/prog.c
BUILDID=`env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../src/readelf \
          -a ${PWD}/F/prog | grep 'Build ID' | cut -d ' ' -f 7`
tempfiles ${PWD}/F/prog ${PWD}/F/prog.c

cp -rvp ${abs_srcdir}/debuginfod-rpms R
if [ "$zstd" = "false" ]; then  # nuke the zstd fedora 31 ones
    rm -vrf R/debuginfod-rpms/fedora31
fi

kill -USR1 $PID1
# Now there should be 1 files in the index
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

cp -rvp ${abs_srcdir}/debuginfod-rpms R
if [ "$zstd" = "false" ]; then  # nuke the zstd fedora 31 ones
    rm -vrf R/debuginfod-rpms/fedora31
fi

tempfiles vlog3
cp -rvp ${abs_srcdir}/debuginfod-tars Z
kill -USR1 $PID1
# Wait till both files are in the index and scan/index fully finished
wait_ready $PORT1 'thread_work_total{role="traverse"}' 3
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0
########################################################################
# All rpms need to be in the index, except the dummy permission-000 one
rpms=$(find R -name \*rpm | grep -v nothing | wc -l)
wait_ready $PORT1 'scanned_files_total{source=".rpm archive"}' $rpms
txz=$(find Z -name \*tar.xz | wc -l)

kill -USR1 $PID1  # two hits of SIGUSR1 may be needed to resolve .debug->dwz->srefs
# Wait till both files are in the index and scan/index fully finished
wait_ready $PORT1 'thread_work_total{role="traverse"}' 4
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

export DEBUGINFOD_URLS=http://127.0.0.1:$PORT1

# common source file sha1
SHA=f4a1a8062be998ae93b8f1cd744a398c6de6dbb1
# fedora31
if [ $zstd = true ]; then
    # fedora31 uses zstd compression on rpms, older rpm2cpio/libarchive can't handle it
    # and we're not using the fancy -Z '.rpm=(rpm2cpio|zstdcat)<' workaround in this testsuite
    archive_test 420e9e3308971f4b817cc5bf83928b41a6909d88 /usr/src/debug/hello3-1.0-2.x86_64/foobar////./../hello.c $SHA
    archive_test 87c08d12c78174f1082b7c888b3238219b0eb265 /usr/src/debug/hello3-1.0-2.x86_64///foobar/./..//hello.c $SHA
fi
# fedora30
archive_test c36708a78618d597dee15d0dc989f093ca5f9120 /usr/src/debug/hello2-1.0-2.x86_64/hello.c $SHA
archive_test 41a236eb667c362a1c4196018cc4581e09722b1b /usr/src/debug/hello2-1.0-2.x86_64/hello.c $SHA
# rhel7
archive_test bc1febfd03ca05e030f0d205f7659db29f8a4b30 /usr/src/debug/hello-1.0/hello.c $SHA
archive_test f0aa15b8aba4f3c28cac3c2a73801fefa644a9f2 /usr/src/debug/hello-1.0/hello.c $SHA
# rhel6
archive_test bbbf92ebee5228310e398609c23c2d7d53f6e2f9 /usr/src/debug/hello-1.0/hello.c $SHA
archive_test d44d42cbd7d915bc938c81333a21e355a6022fb7 /usr/src/debug/hello-1.0/hello.c $SHA
RPM_BUILDID=d44d42cbd7d915bc938c81333a21e355a6022fb7 # in rhel6/ subdir

# Drop some of the artifacts, run a groom cycle; confirm that
# debuginfod has forgotten them, but remembers others
rm -r R/debuginfod-rpms/rhel6/*

kill -USR2 $PID1  # groom cycle
## 1 groom cycle already took place at/soon-after startup, so -USR2 makes 2
wait_ready $PORT1 'thread_work_total{role="groom"}' 2
# Expect 4 rpms containing 2 buildids to be deleted by the groom
wait_ready $PORT1 'groomed_total{decision="stale"}' 4
# Expect no more groom actions pending
wait_ready $PORT1 'thread_work_pending{role="groom"}' 0

rm -rf $DEBUGINFOD_CACHE_PATH # clean it from previous tests

# this is one of the buildids from the groom-deleted rpms
testrun ${abs_top_builddir}/debuginfod/debuginfod-find executable $RPM_BUILDID && false || true

# but this one was not deleted so should be still around
testrun ${abs_top_builddir}/debuginfod/debuginfod-find executable $BUILDID || true

kill $PID1
wait $PID1
PID1=0
exit 0
