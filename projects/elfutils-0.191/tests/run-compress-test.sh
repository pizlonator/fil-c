#! /bin/sh
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

# uncompress -> gnucompress -> uncompress -> elfcompress -> uncompress
testrun_elfcompress_file()
{
    infile="$1"
    uncompressedfile="${infile}.uncompressed"
    tempfiles "$uncompressedfile"
    
    echo "uncompress $infile -> $uncompressedfile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t none -o ${uncompressedfile} ${infile}
    testrun ${abs_top_builddir}/src/elflint --gnu-ld ${uncompressedfile}

    SIZE_uncompressed=$(stat -c%s $uncompressedfile)

    gnucompressedfile="${infile}.gnu"
    tempfiles "$gnucompressedfile"
    echo "compress gnu $uncompressedfile -> $gnucompressedfile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t gnu -o ${gnucompressedfile} ${uncompressedfile}
    testrun ${abs_top_builddir}/src/elflint --gnu-ld ${gnucompressedfile}

    SIZE_gnucompressed=$(stat -c%s $gnucompressedfile)
    test $SIZE_gnucompressed -lt $SIZE_uncompressed ||
	{ echo "*** failure $gnucompressedfile not smaller"; exit -1; }
    
    gnuuncompressedfile="${infile}.gnu.uncompressed"
    tempfiles "$gnuuncompressedfile"
    echo "uncompress $gnucompressedfile -> $gnuuncompressedfile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t none -o ${gnuuncompressedfile} ${gnucompressedfile}
    testrun ${abs_top_builddir}/src/elfcmp ${uncompressedfile} ${gnuuncompressedfile}

    elfcompressedfile="${infile}.gabi"
    tempfiles "$elfcompressedfile"
    echo "compress gabi $uncompressedfile -> $elfcompressedfile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t zlib -o ${elfcompressedfile} ${uncompressedfile}
    testrun ${abs_top_builddir}/src/elflint --gnu-ld ${elfcompressedfile}

    SIZE_elfcompressed=$(stat -c%s $elfcompressedfile)
    test $SIZE_elfcompressed -lt $SIZE_uncompressed ||
	{ echo "*** failure $elfcompressedfile not smaller"; exit -1; }
    
    elfuncompressedfile="${infile}.gabi.uncompressed"
    tempfiles "$elfuncompressedfile"
    echo "uncompress $elfcompressedfile -> $elfuncompressedfile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t none -o ${elfuncompressedfile} ${elfcompressedfile}
    testrun ${abs_top_builddir}/src/elfcmp ${uncompressedfile} ${elfuncompressedfile}

    if test -z "$ELFUTILS_ZSTD"; then
      return;
    fi

    outputfile="${infile}.gabi.zstd"
    tempfiles "$outputfile"
    echo "zstd compress $elfcompressedfile -> $outputfile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t zstd -o ${outputfile} ${elfcompressedfile}
    testrun ${abs_top_builddir}/src/elfcmp ${uncompressedfile} ${outputfile}
    echo "checking compressed section header" $outputfile
    testrun ${abs_top_builddir}/src/readelf -Sz ${outputfile} | grep "ELF ZSTD" >/dev/null

    zstdfile="${infile}.zstd"
    tempfiles "$zstdfile"
    echo "zstd compress $uncompressedfile -> $zstdfile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t zstd -o ${zstdfile} ${elfuncompressedfile}
    testrun ${abs_top_builddir}/src/elfcmp ${uncompressedfile} ${zstdfile}
    echo "checking compressed section header" $zstdfile
    testrun ${abs_top_builddir}/src/readelf -Sz ${zstdfile} | grep "ELF ZSTD" >/dev/null

    zstdgnufile="${infile}.zstd.gnu"
    tempfiles "$zstdgnufile"
    echo "zstd re-compress to GNU ZLIB $zstdfile -> $zstdgnufile"
    testrun ${abs_top_builddir}/src/elfcompress -v -t zlib-gnu -o ${zstdgnufile} ${zstdfile}
    testrun ${abs_top_builddir}/src/elfcmp ${uncompressedfile} ${zstdgnufile}
    echo "checking .zdebug section name" $zstdgnufile
    testrun ${abs_top_builddir}/src/readelf -S ${zstdgnufile} | grep ".zdebug" >/dev/null
}

testrun_elfcompress()
{
    testfile="$1"
    testfiles ${testfile}
    testrun_elfcompress_file ${testfile}

    # Merge the string tables to make things a little more interesting.
    mergedfile="${testfile}.merged"
    tempfiles ${mergedfile}
    echo "merging string tables ${testfile} -> ${mergedfile}"
    testrun ${abs_top_builddir}/tests/elfstrmerge -o ${mergedfile} ${testfile}
    testrun_elfcompress_file ${mergedfile}
}

# Random ELF32 testfile
testrun_elfcompress testfile4

# Random ELF64 testfile
testrun_elfcompress testfile12

# Random ELF64BE testfile
testrun_elfcompress testfileppc64

# Random ELF32BE testfile
testrun_elfcompress testfileppc32

# Already compressed files
testrun_elfcompress testfile-zgnu64
testrun_elfcompress testfile-zgnu64be
testrun_elfcompress testfile-zgabi64
testrun_elfcompress testfile-zgabi64be
testrun_elfcompress testfile-zgnu32
testrun_elfcompress testfile-zgnu32be
testrun_elfcompress testfile-zgabi32
testrun_elfcompress testfile-zgabi32be

exit 0
