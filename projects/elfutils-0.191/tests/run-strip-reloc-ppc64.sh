#! /bin/sh

. $srcdir/strip-reloc-subr.sh

# See run-readelf-zdebug-rel.sh
testfiles testfile-debug-rel-ppc64.o
runtest testfile-debug-rel-ppc64.o 1

testfiles testfile-debug-rel-ppc64-z.o
runtest testfile-debug-rel-ppc64-z.o 1

testfiles testfile-debug-rel-ppc64-g.o
runtest testfile-debug-rel-ppc64-g.o 1

exit $runtest_status
