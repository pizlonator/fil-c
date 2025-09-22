#! /usr/bin/env bash
# Copyright (C) 2017 Red Hat, Inc.
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

tempfiles objects.list test.ar

echo Make a sorted list of the just build src .o files.
(cd ${abs_top_builddir}/src; ls *.o | sort) > objects.list
cat objects.list

echo Create a new ar file with the .o files.
testrun ${abs_top_builddir}/src/ar -r test.ar \
	$(echo ${abs_top_builddir}/src/*.o | sort)

echo List the ar file contents.
testrun_compare ${abs_top_builddir}/src/ar -t test.ar < objects.list

echo Delete all objects again.
testrun ${abs_top_builddir}/src/ar -d test.ar $(cat objects.list)

echo Check new ar file is now empty
testrun_compare ${abs_top_builddir}/src/ar -t test.ar << EOF
EOF

exit 0
