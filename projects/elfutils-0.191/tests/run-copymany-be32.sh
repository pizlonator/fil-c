#! /bin/sh

. $srcdir/test-copymany-subr.sh

# 32bit, big endian, rel
testfiles testfile29
test_copy_and_add testfile29
test_copy_and_add testfile29.copy

# 32bit, big endian, non-rel
testfiles testfile26
test_copy_and_add testfile26
test_copy_and_add testfile26.copy

exit 0
