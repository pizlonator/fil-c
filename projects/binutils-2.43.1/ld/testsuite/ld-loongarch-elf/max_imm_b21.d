#source: abi2_max_imm.s
#as: --defsym b21=1
#ld: -e 0 -Ttext 0x20000000 --section-start=.low=0x1fc00000 --section-start=.high=0x20400000
#objdump: -s

#...
Contents of section .text:
 20000000 90000040 8ffcff43.*
#pass
