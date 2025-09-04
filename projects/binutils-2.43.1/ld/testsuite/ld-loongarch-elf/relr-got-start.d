#source: relr-got-start.s
#ld: -pie -z pack-relative-relocs -T relr-relocs.ld
#readelf: -rW

Relocation section '\.relr\.dyn' at offset 0x[a-z0-f]+ contains 1 entry which relocates 1 location:
Index: Entry            Address           Symbolic Address
0000:  0000000000020008 0000000000020008  _GLOBAL_OFFSET_TABLE_ \+ 0x8
