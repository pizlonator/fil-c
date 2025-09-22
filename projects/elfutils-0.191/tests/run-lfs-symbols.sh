#! /usr/bin/env bash
# Copyright (C) 2015 Red Hat, Inc.
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

. $srcdir/test-subr.sh

if ! grep -q -F '#define _FILE_OFFSET_BITS' ${abs_top_builddir}/config.h; then
  echo "LFS testing is irrelevant on this system"
  exit 77
fi

# #include <stdio.h>
# int main () {
#     FILE *f = fopen ("/dev/null", "r");
#     return f == NULL;
# }
#
# Built for Linux i686, without setting _FILE_OFFSET_BITS.
# $ gcc -m32 -O2 nolfs.c -o testfile-nolfs
testfiles testfile-nolfs

LFS_FORMAT='BEGIN {
  while ((getline < "%s") > 0)
    /^\w/ && bad[$0]
  FS="@"
}
/@@GLIBC_/ && $1 in bad { print $1 }'

LFS=$(printf "$LFS_FORMAT" "${abs_srcdir}/lfs-symbols")

makeprint() {
  make print-$1 -C $2 |& gawk -F= "/^$1=/{ print \$2 }"
}

testrun_lfs() {
  echo "checking $1"
  if [ -e "$1" ]; then
    bad=$(testrun ${abs_top_builddir}/src/nm -u "$1" | gawk "$LFS")
    if [ -n "$bad" ]; then
      echo "$1 contains non-lfs symbols:" $bad
      exit_status=1
    fi
  else
    echo "$1 doesn't exist"
    exit_status=1
  fi
}

echo First sanity-check that LFS detection works.
exit_status=0
testrun_lfs ./testfile-nolfs
if [ $exit_status -eq 0 ]; then
  echo "Didn't detect any problem with testfile-nolfs!"
  exit 99
fi
echo

exit_status=0

echo Check all normal build targets.
for dir in libelf libdw libasm libcpu src; do
  dir=${abs_top_builddir}/$dir
  for program in $(makeprint PROGRAMS $dir); do
    testrun_lfs $dir/$program
  done
done
echo

echo Check all test programs.
dir=${abs_builddir}
for program in $(makeprint check_PROGRAMS $dir); do
  testrun_lfs $dir/$program
done
echo

exit $exit_status
