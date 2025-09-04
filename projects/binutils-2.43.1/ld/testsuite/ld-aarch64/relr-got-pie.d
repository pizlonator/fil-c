#source: relr-got.s
#ld: -pie -z pack-relative-relocs -T relocs.ld
#readelf: -rW

Relocation section '\.rela\.dyn' at offset 0x1.* contains 2 entries:
    Offset             Info             Type               Symbol's Value  Symbol's Name \+ Addend
0000000000000000  0000000000000000 R_AARCH64_NONE                            0
0000000000020030  0000000200000401 R_AARCH64_GLOB_DAT     0000000000000000 sym_weak_undef \+ 0

Relocation section '\.relr\.dyn' at offset 0x1.* contains 2 entries which relocate 4 locations:
Index: Entry            Address           Symbolic Address
0000:  0000000000020008 0000000000020008  _GLOBAL_OFFSET_TABLE_ \+ 0x8
0001:  000000000000000f 0000000000020010  _GLOBAL_OFFSET_TABLE_ \+ 0x10
                        0000000000020018  _GLOBAL_OFFSET_TABLE_ \+ 0x18
                        0000000000020020  _GLOBAL_OFFSET_TABLE_ \+ 0x20
