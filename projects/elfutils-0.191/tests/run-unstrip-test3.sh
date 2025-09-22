# Buggy binutils objdump might strip SHF_INFO_LINK from relocation sections.
# With gcc5 we might have a .rela.plt section with that flag set.
#
# int main() 
# {
#     return 0;
# }
#
# gcc -o testfile-info-link -g testprog.c
# objcopy --only-keep-debug testfile-info-link testfile-info-link.debuginfo
# eu-strip --strip-debug -o testfile-info-link.stripped testfile-info-link

original=testfile-info-link
stripped=testfile-info-link.stripped
debugfile=testfile-info-link.debuginfo

. $srcdir/run-unstrip-test.sh
