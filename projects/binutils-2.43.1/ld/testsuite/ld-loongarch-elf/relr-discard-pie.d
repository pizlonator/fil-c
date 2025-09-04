#source: relr-discard.s
#ld: -pie -z pack-relative-relocs -T relr-discard.ld
#readelf: -rW

Relocation section '\.relr\.dyn' at offset 0x[0-9a-f]+ contains 2 entries which relocate 2 locations:
Index: Entry            Address           Symbolic Address
0000:  0000000000020008 0000000000020008  _GLOBAL_OFFSET_TABLE_ \+ 0x8
0001:  0000000000000003 0000000000020010  _GLOBAL_OFFSET_TABLE_ \+ 0x10
