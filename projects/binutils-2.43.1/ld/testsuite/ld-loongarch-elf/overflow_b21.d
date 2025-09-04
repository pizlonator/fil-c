#source: abi2_overflows.s
#as: --defsym overflow=1 --defsym b21=1
#ld: -e 0 --section-start=.local_sym=0x20400000 -Ttext 0x20000000
#error: .*relocation truncated to fit: R_LARCH_B21 against `local_sym'
