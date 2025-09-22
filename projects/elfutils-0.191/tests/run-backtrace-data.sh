#! /usr/bin/env bash
# Copyright (C) 2013 Red Hat, Inc.
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

. $srcdir/backtrace-subr.sh

# This test really cannot be run under valgrind, it tries to introspect
# its own maps and registers and will find valgrinds instead.
unset VALGRIND_CMD

tempfiles data.{bt,err}
(set +ex;
 testrun ${abs_builddir}/backtrace-data 1>data.bt 2>data.err;
 if test $? == 77; then cat data.{bt,err}; exit 77; fi
 true)

cat data.{bt,err}
check_unsupported data.err data
check_all data.{bt,err} data
