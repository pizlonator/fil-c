#as: --no-warn
#readelf: -rsW
#skip: loongarch32-*-*

Relocation section '\.rela\.text' at offset .* contains 2 entries:
.*
0+04[ 	]+0000000000000066[ 	]+R_LARCH_ALIGN[ 	]+c
0+14[ 	]+0000000500000066[ 	]+R_LARCH_ALIGN[ 	]+0+[ 	]+\.Lla-relax-align \+ 404

Relocation section '\.rela\.text2' at offset .* contains 2 entries:
.*
0+04[ 	]+0000000000000066[ 	]+R_LARCH_ALIGN[ 	]+c
0+14[ 	]+0000000600000066[ 	]+R_LARCH_ALIGN[ 	]+0+[ 	]+\.Lla-relax-align \+ 404

Symbol table '\.symtab' contains .* entries:
#...
[ 	]+.*:[ 	]+0+[ 	]+0[ 	]+SECTION[ 	]+LOCAL[ 	]+DEFAULT[ 	]+1[ 	]+\.text
#...
[ 	]+.*:[ 	]+0+[ 	]+0[ 	]+SECTION[ 	]+LOCAL[ 	]+DEFAULT[ 	]+5[ 	]+\.text2
#...
[ 	]+.*:[ 	]+0+[ 	]+0[ 	]+NOTYPE[ 	]+LOCAL[ 	]+DEFAULT[ 	]+1[ 	]+\.Lla-relax-align
#...
[ 	]+.*:[ 	]+0+[ 	]+0[ 	]+NOTYPE[ 	]+LOCAL[ 	]+DEFAULT[ 	]+5[ 	]+\.Lla-relax-align
#pass
