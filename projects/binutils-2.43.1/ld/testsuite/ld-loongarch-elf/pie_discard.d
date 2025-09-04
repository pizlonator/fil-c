#source: pie_discard.s
#ld: -pie -e 0 -T pie_discard.t
#readelf: -rW

#...
Relocation section '\.rela\.dyn' .* 1 .*
#...
.*R_LARCH_RELATIVE.*
#pass

