#source: abi2_overflows.s
#as: --defsym underflow=1 --defsym b16=1
#ld: -e 0 --section-start=.local_sym=0x1ffe0000 -Ttext 0x20000000
#error: .*relocation truncated to fit: R_LARCH_B16 against `local_sym'
