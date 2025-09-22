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

# find an unused port number
mkdir R
# This variable is essential and ensures no time-race for claiming ports occurs
# set base to a unique multiple of 100 not used in any other 'run-debuginfod-*' test
base=8300
get_ports
DB=${PWD}/.debuginfod_tmp.sqlite
tempfiles $DB
export DEBUGINFOD_CACHE_PATH=${PWD}/.client_cache

env LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod $VERBOSE -R -p $PORT1 -d $DB -t0 -g0 -v R > vlog$PORT1 2>&1 &
PID1=$!
tempfiles vlog$PORT1
errfiles vlog$PORT1
# Server must become ready
wait_ready $PORT1 'ready' 1
# And make sure first scan is done
wait_ready $PORT1 'thread_work_total{role="traverse"}' 1

# Be patient when run on a busy machine things might take a bit.
export DEBUGINFOD_URLS='http://127.0.0.1:'$PORT1

# Check thread comm names
ps -q $PID1 -e -L -o '%p %c %a' | grep groom
ps -q $PID1 -e -L -o '%p %c %a' | grep scan
ps -q $PID1 -e -L -o '%p %c %a' | grep traverse

cp -rvp ${abs_srcdir}/debuginfod-rpms R
if [ "$zstd" = "false" ]; then  # nuke the zstd fedora 31 ones
    rm -vrf R/debuginfod-rpms/fedora31
fi

kill -USR1 $PID1
# Wait till both files are in the index and scan/index fully finished
wait_ready $PORT1 'thread_work_total{role="traverse"}' 2
wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
wait_ready $PORT1 'thread_busy{role="scan"}' 0

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
# arch
#archive_test cee13b2ea505a7f37bd20d271c6bc7e5f8d2dfcb /usr/src/debug/hello.c 7a1334e086b97e5f124003a6cfb3ed792d10cdf4

kill $PID1
wait $PID1
PID1=0
exit 0
