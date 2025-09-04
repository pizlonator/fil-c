#source: relr-data.s
#ld: -pie -z pack-relative-relocs -T relr-relocs.ld
#readelf: -rW

Relocation section '\.rela\.dyn' at offset 0x[0-9a-f]+ contains 5 entries:
    Offset             Info             Type               Symbol's Value  Symbol's Name \+ Addend
0000000012340000  0000000000000003 R_LARCH_RELATIVE                          10004
0000000012340008  0000000000000003 R_LARCH_RELATIVE                          10008
0000000012340010  0000000000000003 R_LARCH_RELATIVE                          1000c
0000000012340018  0000000000000003 R_LARCH_RELATIVE                          12340050
0000000012340040  0000000c00000002 R_LARCH_64             0000000000000000 sym_weak_undef \+ 0

Relocation section '\.relr\.dyn' at offset 0x[0-9a-f]+ contains 2 entries which relocate 4 locations:
Index: Entry            Address           Symbolic Address
0000:  0000000012340020 0000000012340020  aligned_local
0001:  0000000000000027 0000000012340028  aligned_hidden
                        0000000012340030  aligned_global
                        0000000012340048  aligned_DYNAMIC
