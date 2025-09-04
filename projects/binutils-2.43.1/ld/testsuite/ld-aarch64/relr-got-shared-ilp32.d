#source: relr-got.s
#as: -mabi=ilp32
#ld: -m [aarch64_choose_ilp32_emul] -shared -z pack-relative-relocs -T relocs-ilp32.ld
#readelf: -rW

Relocation section '\.rela\.dyn' at offset 0x1.* contains 3 entries:
 Offset     Info    Type                Sym. Value  Symbol's Name \+ Addend
00020010  .* R_AARCH64_P32_GLOB_DAT 00010038   sym_global \+ 0
00020014  .* R_AARCH64_P32_GLOB_DAT 0000002a   sym_global_abs \+ 0
00020018  .* R_AARCH64_P32_GLOB_DAT 00000000   sym_weak_undef \+ 0

Relocation section '\.relr\.dyn' at offset 0x1.* contains 2 entries which relocate 3 locations:
Index: Entry    Address   Symbolic Address
0000:  00020004 00020004  _GLOBAL_OFFSET_TABLE_ \+ 0x4
0001:  00000007 00020008  _GLOBAL_OFFSET_TABLE_ \+ 0x8
                0002000c  _GLOBAL_OFFSET_TABLE_ \+ 0xc
