#source: reloc_abs_with_shared.s
#as:
#ld: -shared
#error: .*relocation R_LARCH_ABS_HI20 against `s` can not be used when making a shared object; recompile with -fPIC
#...
#pass
