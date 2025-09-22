#! /usr/bin/env bash
# Copyright (C) 2023 Red Hat, Inc.
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
# set -x
set -e
base=14000
get_ports

# Test different command line combinations on the srcfiles binary itself.
ET_EXEC="${abs_top_builddir}/src/srcfiles"
ET_PID=$$

SRC_NAME="srcfiles.cxx"

# Ensure the output contains the expected source file srcfiles.cxx
testrun $ET_EXEC -e $ET_EXEC | grep $SRC_NAME > /dev/null

# Check if zip option is available (only available if libarchive is available.
#   Debuginfod optional to fetch source files from debuginfod federation.)
$ET_EXEC --help | grep -q zip && command -v unzip >/dev/null 2>&1 && zip=true || zip=false

for null_arg in --null ""; do
  for verbose_arg in --verbose ""; do
    echo "Test with options $null_arg $verbose_arg"
    testrun $ET_EXEC $null_arg $verbose_arg -p $ET_PID > /dev/null

    # Ensure that the output contains srcfiles.cxx
    cu_only=$(testrun $ET_EXEC $null_arg $verbose_arg -c -e $ET_EXEC)
    default=$(testrun $ET_EXEC $null_arg $verbose_arg -e $ET_EXEC)
    result1=$(echo "$cu_only" | grep "$SRC_NAME")
    result2=$(echo "$default" | grep "$SRC_NAME")

    if [ -z "$result1" ] || [ -z "$result2" ]; then
      exit 1
    fi

    # Ensure that the output with the cu-only option contains fewer source files
    if [ $(echo "$cu_only" | wc -m) -gt $(echo "$default" | wc -m) ]; then
      exit 1
    fi

    if $zip; then
      # Zip option tests
        testrun $ET_EXEC $verbose_arg -z -e $ET_EXEC > test.zip
        tempfiles test.zip

        unzip -v test.zip
        unzip -t test.zip

        # Ensure unzipped srcfiles.cxx and its contents are the same as the original source file
        unzip -j test.zip "*/$SRC_NAME"
        diff "$SRC_NAME" $abs_srcdir/../src/$SRC_NAME
        rm -f test.zip $SRC_NAME
    fi
  done
done

# Debuginfod source file downloading test.
# Start debuginfod server on the elfutils build directory.
if [ -x ${abs_builddir}/../debuginfod/debuginfod ] && $zip; then
  LD_LIBRARY_PATH=$ldpath ${abs_builddir}/../debuginfod/debuginfod -vvvv -d debuginfod.sqlite3 -F -p $PORT1 ${abs_top_builddir}/src > debuginfod.log 2>&1 &
  PID1=$!
  tempfiles debuginfod.sqlite3 debuginfod.log
  wait_ready $PORT1 'ready' 1
  wait_ready $PORT1 'thread_work_total{role="traverse"}' 1
  wait_ready $PORT1 'thread_work_pending{role="scan"}' 0
  wait_ready4 $PORT1 'thread_busy{role="scan"}' 0 300 # lots of source files may be slow to index with $db on NFS

  export DEBUGINFOD_URLS="http://localhost:${PORT1}/"
  export DEBUGINFOD_VERBOSE=1
  testrun $ET_EXEC -z -b -e $ET_EXEC > test.zip
  tempfiles test.zip

  unzip -v test.zip
  unzip -t test.zip

  # Extract the zip.
  mkdir extracted
  unzip test.zip -d extracted

  # Ensure that source files for this tool have been archived.
  source_files="srcfiles.cxx libdwfl.h gelf.h"
  extracted_files=$(find extracted -type f)
  for file in $source_files; do
      echo "$extracted_files" | grep -q "$file" > /dev/null
  done

  # Compare between the extracted file and the actual source file srcfiles.cxx.
  extracted_file=$(find extracted -name $SRC_NAME)
  diff "$extracted_file" $abs_srcdir/../src/$SRC_NAME

  rm -rf extracted

  kill $PID1
  wait
  PID1=0
fi
