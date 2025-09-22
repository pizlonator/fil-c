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
# itself through ptrace and will find bits and pieces of valgrind.
# On top of that valgrind also tries to read all the unwind info and
# will warn and complain about various opcodes it doesn't understand...
unset VALGRIND_CMD

tempfiles dwarf.{bt,err}
(set +ex; testrun ${abs_builddir}/backtrace-dwarf 1>dwarf.bt 2>dwarf.err; true)
cat dwarf.{bt,err}
check_native_unsupported dwarf.err dwarf
check_main dwarf.bt dwarf
