#source: relr-text.s
#ld: -pie -z pack-relative-relocs -T relr-relocs.ld
#readelf: -drW

#...
 0x0000000000000016 \(TEXTREL\)            0x0
#...
 0x0000000000000024 \(RELR\)               .*
 0x0000000000000023 \(RELRSZ\)             8 \(bytes\)
 0x0000000000000025 \(RELRENT\)            8 \(bytes\)
#...
Relocation section '\.relr\.dyn' .* contains 1 entry which relocates 1 location:
Index: Entry            Address           Symbolic Address
0000:  0000000000010000 0000000000010000  _start
