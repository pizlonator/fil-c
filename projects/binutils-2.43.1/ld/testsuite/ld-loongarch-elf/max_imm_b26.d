#source: abi2_max_imm.s
#as: --defsym b26=1
#ld: -e 0 -Ttext 0x20000000 --section-start=.low=0x18000000 --section-start=.high=0x28000000
#objdump: -s

#...
Contents of section .text:
 20000000 00020050 fffdff53.*
#pass
