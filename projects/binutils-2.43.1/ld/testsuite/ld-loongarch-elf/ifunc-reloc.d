#ld: -shared
#readelf: -Wr

#...
.*'\.rela\.dyn'.*
#...
.* R_LARCH_RELATIVE .*
.* R_LARCH_IRELATIVE .*
.* R_LARCH_IRELATIVE .*
.* R_LARCH_IRELATIVE .*
.* R_LARCH_IRELATIVE .*
#...
.*'\.rela\.plt'.*
#...
.* R_LARCH_JUMP_SLOT .*
.* R_LARCH_JUMP_SLOT .*
.* R_LARCH_JUMP_SLOT .*
.* R_LARCH_JUMP_SLOT .*
.* R_LARCH_JUMP_SLOT .*
