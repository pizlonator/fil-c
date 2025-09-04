#as: --gdwarf-5
#readelf: -r --wide
#skip: loongarch32-*-*

Relocation section '\.rela\.debug_line' at offset .* contains 5 entries:
#...
0+22.*R_LARCH_32[ \t]+[0-9]+.*
0+2c.*R_LARCH_32[ \t]+[0-9]+.*
0+36.*R_LARCH_64[ \t]+[0-9]+.*
0+42.*R_LARCH_ADD16[ \t]+[0-9]+.*
0+42.*R_LARCH_SUB16[ \t]+[0-9]+.*
#pass
