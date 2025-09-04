#source: abi2_overflows.s
#as: --defsym underflow=1 --defsym pcrel20=1
#ld: -e 0 --section-start=.local_sym=0x1fe00000 -Ttext 0x20000000
#error: .*relocation truncated to fit: R_LARCH_PCREL20_S2 against `local_sym'
