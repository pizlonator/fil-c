#! /bin/sh

. $srcdir/test-copymany-subr.sh

# 32bit, little endian, rel
testfiles testfile9
test_copy_and_add testfile9
test_copy_and_add testfile9.copy

# 32bit, little endian, non-rel
testfiles testfile
test_copy_and_add testfile
test_copy_and_add testfile.copy

exit 0
