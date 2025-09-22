#! /bin/sh

. $srcdir/test-subr.sh

# echo "int main () {}" | gcc -xc -o testfile-arm-flags -
testfiles testfile-arm-flags

testrun_compare ${abs_top_builddir}/src/readelf -h testfile-arm-flags <<\EOF
ELF Header:
  Magic:   7f 45 4c 46 01 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF32
  Data:                              2's complement, little endian
  Ident Version:                     1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              DYN (Shared object file)
  Machine:                           ARM
  Version:                           1 (current)
  Entry point address:               0x3d1
  Start of program headers:          52 (bytes into file)
  Start of section headers:          6920 (bytes into file)
  Flags:                             Version5 EABI, hard-float ABI
  Size of this header:               52 (bytes)
  Size of program header entries:    32 (bytes)
  Number of program headers entries: 9
  Size of section header entries:    40 (bytes)
  Number of section headers entries: 29
  Section header string table index: 28

EOF
