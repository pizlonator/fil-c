#! /bin/sh
# Copyright (C) 2005, 2007, 2008, 2015 Red Hat, Inc.
# This file is part of elfutils.
# Written by Ulrich Drepper <drepper@redhat.com>, 2005.
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

testfiles testfile18

testrun_compare ${abs_top_builddir}/src/elflint --gnu-ld testfile18 <<\EOF
section [ 8] '.rela.dyn': relocation 1: copy relocation against symbol of type FUNC
EOF

testfiles testfile32
testrun ${abs_top_builddir}/src/elflint -q testfile32

testfiles testfile33
testrun ${abs_top_builddir}/src/elflint -q testfile33

testfiles testfile42
testrun ${abs_top_builddir}/src/elflint -q --gnu-ld testfile42

# Contains debuginfo, compress it, recheck
tempfiles testfile42z
testrun ${abs_top_builddir}/src/elfcompress -f -q -o testfile42z testfile42
testrun ${abs_top_builddir}/src/elflint -q --gnu-ld testfile42z

testfiles testfile46
testrun ${abs_top_builddir}/src/elflint -q testfile46

# see also run-readelf-d.sh
testfiles testlib_dynseg.so
testrun ${abs_top_builddir}/src/elflint -q --gnu-ld testlib_dynseg.so

# s390x has SHT_HASH with sh_entsize 8 (really should be 4, but see common.h)
# This was wrongly checked when comparing .gnu.hash and .hash.
# Simple "int main (int argc, char **argv) { return 0; }"
# gcc -Xlinker --hash-style=both -o testfile-s390x-hash-both s390x-hash-both.c
testfiles testfile-s390x-hash-both
testrun ${abs_top_builddir}/src/elflint -q --gnu-ld testfile-s390x-hash-both

# Compress the symtab/strtab just because and recheck
tempfiles testfile-s390x-hash-bothz
testrun ${abs_top_builddir}/src/elfcompress -f -q --name='.s??tab' -o testfile-s390x-hash-bothz testfile-s390x-hash-both
testrun ${abs_top_builddir}/src/elflint -q --gnu-ld testfile-s390x-hash-bothz

exit 0
