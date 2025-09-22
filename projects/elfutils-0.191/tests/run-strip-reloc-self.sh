#! /bin/sh

. $srcdir/strip-reloc-subr.sh

# self test, shouldn't impact non-ET_REL files at all.
runtest ${abs_top_builddir}/src/strip 0
runtest ${abs_top_builddir}/src/strip.o 1

# Copy ET_REL file for self-test and make sure to run with/without
# elf section compression.
tempfiles strip-uncompressed.o strip-compressed.o
testrun ${abs_top_builddir}/src/elfcompress -o strip-uncompressed.o -t none \
  ${abs_top_builddir}/src/strip.o
testrun ${abs_top_builddir}/src/elfcompress -o strip-compressed.o -t zlib \
  --force ${abs_top_builddir}/src/strip.o

runtest strip-uncompressed.o 1
runtest strip-compressed.o 1

exit $runtest_status
