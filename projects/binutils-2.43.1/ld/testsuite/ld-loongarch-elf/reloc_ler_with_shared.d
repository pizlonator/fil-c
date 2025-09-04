#source: reloc_ler_with_shared.s
#as:
#ld: -shared
#error: .*relocation R_LARCH_TLS_LE_HI20_R against `s` can not be used when making a shared object; recompile with -fPIC
