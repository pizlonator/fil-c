#! /bin/sh
# Copyright (C) 2005-2012 Red Hat, Inc.
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


# We don't compile in an rpath because we want "make installcheck" to
# use the installed libraries.  So for local test runs we need to point
# the library path at this build.

# This wrapper script is called by the makefile, in one of two ways:
#	$(srcdir)/test-wrapper.sh ../libelf:... run-test.sh ...
# or:
#	$(srcdir)/test-wrapper.sh installed s,^,eu-, run-test.sh ...

if [ "$1" = installed ]; then
  shift
  elfutils_tests_rpath=$1
  shift
  program_transform_name="$1"
  shift
  elfutils_testrun=installed
else
  built_library_path="$1"
  shift
  elfutils_testrun=built
fi

old_path="${LD_LIBRARY_PATH:+:}$LD_LIBRARY_PATH"

case "$1" in
*.sh)
  export built_library_path program_transform_name elfutils_testrun
  export elfutils_tests_rpath
  is_shell_script="yes"
  ;;
*)
  if [ $elfutils_testrun = built ]; then
    LD_LIBRARY_PATH="$built_library_path$old_path"
  elif [ $elfutils_tests_rpath = yes ]; then
    echo >&2 installcheck not possible with --enable-tests-rpath
    exit 77
  elif [ "x$libdir" != x/usr/lib ] && [ "x$libdir" != x/usr/lib64 ]; then
    LD_LIBRARY_PATH="${libdir}:${libdir}/elfutils$old_path"
  fi
  export LD_LIBRARY_PATH
  is_shell_script="no"
  ;;
esac

if [ "x$VALGRIND_CMD" != "x" ]; then
  export VALGRIND_CMD
fi

# When it is a run-*.sh script the VALGRIND_CMD will be passed on
# otherwise we'll need to run the binary explicitly under valgrind.
if [ "x$is_shell_script" = xyes ]; then
  exec "$@"
else
  exec $VALGRIND_CMD "$@"
fi
