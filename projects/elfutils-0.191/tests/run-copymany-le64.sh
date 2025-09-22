#! /bin/sh

. $srcdir/test-copymany-subr.sh

# 64bit, little endian, rel
testfiles testfile38
test_copy_and_add testfile38
test_copy_and_add testfile38.copy

# 64bit, little endian, non-rel
testfiles testfile10
test_copy_and_add testfile10
test_copy_and_add testfile10.copy

exit 0
