#! /bin/sh

. $srcdir/test-copymany-subr.sh

# 64bit, big endian, rel
testfiles testfile23
test_copy_and_add testfile23
test_copy_and_add testfile23.copy

# 64bit, big endian, non-rel
testfiles testfile27
test_copy_and_add testfile27
test_copy_and_add testfile27.copy

exit 0
