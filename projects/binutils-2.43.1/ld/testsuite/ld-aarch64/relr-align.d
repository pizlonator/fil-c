#source: relr-align.s
#ld: -shared -z pack-relative-relocs -T relocs.ld
#readelf: -rW

Relocation section '\.rela\.dyn' .* contains 3 entries:
    Offset             Info             Type               Symbol's Value  Symbol's Name \+ Addend
0000000012340011 .* R_AARCH64_RELATIVE                        10000
0000000012340019 .* R_AARCH64_RELATIVE                        10000
0000000012340041 .* R_AARCH64_RELATIVE                        10000

Relocation section '\.relr\.dyn' .* contains 9 entries which relocate 10 locations:
Index: Entry            Address           Symbolic Address
0000:  0000000012340000 0000000012340000  double_0
0001:  0000000000000003 0000000012340008  double_0 \+ 0x8
0002:  0000000012340022 0000000012340022  double_2
0003:  0000000000000003 000000001234002a  double_2 \+ 0x8
0004:  0000000012340038 0000000012340038  single_0
0005:  000000001234004a 000000001234004a  single_2
0006:  0000000012340058 0000000012340058  big
0007:  8000000100000001 0000000012340158  big \+ 0x100
                        0000000012340250  big \+ 0x1f8
0008:  0000000000000003 0000000012340258  big \+ 0x200
