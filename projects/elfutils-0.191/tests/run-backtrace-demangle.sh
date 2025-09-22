#! /usr/bin/env bash
# Copyright (C) 2014, 2015 Red Hat, Inc.
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

if test -n "$ELFUTILS_DISABLE_DEMANGLE"; then
  echo "demangler unsupported"
  exit 77
fi

. $srcdir/backtrace-subr.sh

child=testfile-backtrace-demangle
testfiles $child{,.core}
tempfiles $child.{bt,err}

# Disable valgrind while dumping because of a bug unmapping libc.so.
# https://bugs.kde.org/show_bug.cgi?id=327427
SAVED_VALGRIND_CMD="$VALGRIND_CMD"
unset VALGRIND_CMD

# There can be more than 3 frames, but depending on the system/installed
# glibc we might not be able to unwind fully till the end.
# cxxfunc -> f -> main
# Expect to see the top two and a warning that there are more frames
# (exit code 1)
testrun ${abs_top_builddir}/src/stack -n 2 -e $child --core $child.core >$child.bt 2>$child.err || exitcode=$?
cat $child.{bt,err}

if [ "x$SAVED_VALGRIND_CMD" != "x" ]; then
  VALGRIND_CMD="$SAVED_VALGRIND_CMD"
  export VALGRIND_CMD
fi

if test $exitcode != 1 || ! grep "shown max number of frames" $child.err; then
  echo >&2 $2: expected more than 2 frames
  false
fi
if ! grep -w f $child.bt; then
  echo >&2 $2: no f
  false
fi
if ! grep ' cxxfunc(int)' $child.bt; then
  echo >&2 $2: no cxxfunc
  false
fi
