#source: relr-got.s
#as: -mabi=ilp32
#ld: -m [aarch64_choose_ilp32_emul] -pie -z pack-relative-relocs -T relocs-ilp32.ld
#readelf: -rW

Relocation section '\.rela\.dyn' at offset 0x1.* contains 2 entries:
 Offset     Info    Type                Sym\. Value  Symbol's Name \+ Addend
00000000  .* R_AARCH64_NONE                    0
00020018  .* R_AARCH64_P32_GLOB_DAT 00000000   sym_weak_undef \+ 0

Relocation section '\.relr\.dyn' at offset 0x1.* contains 2 entries which relocate 4 locations:
Index: Entry    Address   Symbolic Address
0000:  00020004 00020004  _GLOBAL_OFFSET_TABLE_ \+ 0x4
0001:  0000000f 00020008  _GLOBAL_OFFSET_TABLE_ \+ 0x8
                0002000c  _GLOBAL_OFFSET_TABLE_ \+ 0xc
                00020010  _GLOBAL_OFFSET_TABLE_ \+ 0x10
