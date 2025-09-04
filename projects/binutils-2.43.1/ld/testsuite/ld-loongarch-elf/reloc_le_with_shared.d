#source: reloc_le_with_shared.s
#as:
#ld: -shared
#error: .*relocation R_LARCH_TLS_LE_HI20 against `s` can not be used when making a shared object; recompile with -fPIC
#...
#pass
