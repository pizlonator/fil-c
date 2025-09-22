#!/bin/sh
# Copyright (C) 2019 Red Hat, Inc.
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

testrun_on_self ${abs_top_builddir}/src/elfclassify --elf-file
testrun_on_self ${abs_top_builddir}/src/elfclassify --not-core
testrun_on_self ${abs_top_builddir}/src/elfclassify --unstripped
testrun_on_self ${abs_top_builddir}/src/elfclassify --not-linux-kernel-module

testrun_on_self_lib ${abs_top_builddir}/src/elfclassify --shared
testrun_on_self_lib ${abs_top_builddir}/src/elfclassify --loadable
testrun_on_self_lib ${abs_top_builddir}/src/elfclassify --not-executable
testrun_on_self_lib ${abs_top_builddir}/src/elfclassify --not-program

testrun_on_self_exe ${abs_top_builddir}/src/elfclassify --executable
testrun_on_self_exe ${abs_top_builddir}/src/elfclassify --program
testrun_on_self_exe ${abs_top_builddir}/src/elfclassify --loadable
testrun_on_self_exe ${abs_top_builddir}/src/elfclassify --not-shared

testrun ${abs_top_builddir}/src/elfclassify --not-shared $self_test_files_obj
testrun ${abs_top_builddir}/src/elfclassify --not-executable $self_test_files_obj
