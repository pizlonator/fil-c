#source: relr-discard.s
#ld: -shared -z pack-relative-relocs -T relr-discard.ld
#readelf: -rW

Relocation section '\.rela\.dyn' at offset 0x[0-9a-f]+ contains 1 entry:
    Offset             Info             Type               Symbol's Value  Symbol's Name \+ Addend
0000000000020010  0000000300000002 R_LARCH_64             000000000001000c sym_global \+ 0

Relocation section '\.relr\.dyn' at offset 0x[0-9a-f]+ contains 1 entry which relocates 1 location:
Index: Entry            Address           Symbolic Address
0000:  0000000000020008 0000000000020008  _GLOBAL_OFFSET_TABLE_ \+ 0x8
