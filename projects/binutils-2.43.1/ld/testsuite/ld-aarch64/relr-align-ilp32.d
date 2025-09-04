#source: relr-align-ilp32.s
#as: -mabi=ilp32
#ld: -m [aarch64_choose_ilp32_emul] -shared -z pack-relative-relocs -T relocs-ilp32.ld
#readelf: -rW

Relocation section '\.rela\.dyn' at offset 0x1.* contains 3 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name \+ Addend
12340009  000000b7 R_AARCH64_P32_RELATIVE            10000
1234000d  000000b7 R_AARCH64_P32_RELATIVE            10000
12340025  000000b7 R_AARCH64_P32_RELATIVE            10000

Relocation section '\.relr\.dyn' at offset 0x1.* contains 10 entries which relocate 10 locations:
Index: Entry    Address   Symbolic Address
0000:  12340000 12340000  double_0
0001:  00000003 12340004  double_0 \+ 0x4
0002:  12340012 12340012  double_2
0003:  00000003 12340016  double_2 \+ 0x4
0004:  12340020 12340020  single_0
0005:  1234002a 1234002a  single_2
0006:  12340034 12340034  big
0007:  123400b4 123400b4  big \+ 0x80
0008:  80000001 12340130  big \+ 0xfc
0009:  00000003 12340134  big \+ 0x100
