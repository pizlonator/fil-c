#source: relr-discard.s
#ld: -pie --no-apply-dynamic-relocs -z pack-relative-relocs -T relr-discard.ld
#readelf: -rW

# Note: There are unnecessary GOT entries and *_NONE relocations
# for those GOT entries and discarded locations, this is bug 31850.

Relocation section '\.rela\.dyn' at offset 0x1.* contains 4 entries:
    Offset             Info             Type               Symbol's Value  Symbol's Name \+ Addend
0000000000000000  0000000000000000 R_AARCH64_NONE                            0
0000000000000000  0000000000000000 R_AARCH64_NONE                            0
0000000000000000  0000000000000000 R_AARCH64_NONE                            0
0000000000000000  0000000000000000 R_AARCH64_NONE                            0

Relocation section '\.relr\.dyn' at offset 0x1.* contains 2 entries which relocate 2 locations:
Index: Entry            Address           Symbolic Address
0000:  0000000000020008 0000000000020008  _GLOBAL_OFFSET_TABLE_ \+ 0x8
0001:  0000000000000003 0000000000020010  _GLOBAL_OFFSET_TABLE_ \+ 0x10
