#! /bin/sh
# Copyright (C) 2005, 2006, 2013 Red Hat, Inc.
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

# This tests all the miscellaneous components of backend support
# against whatever this build is running on.  A platform will fail
# this test if it is missing parts of the backend implementation.
#
# As new backend code is added to satisfy the test, be sure to update
# the fixed test cases (run-allregs.sh et al) to test that backend
# in all builds.

tempfiles native.c native
printf '#include <unistd.h>\nint main (void) { while (1) pause (); }\n' \
  > native.c

native=0
kill_native()
{
  test $native -eq 0 || {
    kill -9 $native 2> /dev/null || :
    wait $native 2> /dev/null || :
  }
  native=0
}

native_cleanup()
{
  kill_native
  test_cleanup
}

native_exit()
{
  native_cleanup
  exit_cleanup
}

trap native_cleanup 1 2 15
trap native_exit 0

for cc in "$HOSTCC" "$HOST_CC" cc gcc "$CC"; do
  test "x$cc" != x || continue
  $cc -o native -g native.c &&
  # Some shell versions don't do this right without the braces.
  { ./native & native=$! ; } &&
  sleep 1 && kill -0 $native &&
  break ||
  native=0
done

native_test()
{
  # Try the build against itself, i.e. $config_host.
  echo "Try the build against itself: $@ -e $1"
  testrun "$@" -e $1

  # Try the build against a presumed native process, running this sh.
  # For tests requiring debug information, this may not test anything.
  echo "Try the build against a presumed native process: $@ -p $$"
  testrun "$@" -p $$

  # Try the build against the trivial native program we just built with -g.
  echo "Try the build against the trivial native program: $@ -p $native"
  test $native -eq 0 || testrun "$@" -p $native
}

native_test ${abs_builddir}/allregs
native_test ${abs_builddir}/funcretval

# We do this explicitly rather than letting the trap 0 cover it,
# because as of version 3.1 bash prints the "Killed" report for
# $native when we do the kill inside the exit handler.
native_cleanup

exit 0
