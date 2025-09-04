#source: abi2_max_imm.s
#as: --defsym pcrel20=1
#ld: -e 0 -Ttext 0x20000000 --section-start=.low=0x1fe00000 --section-start=.high=0x20200000
#objdump: -s

#...
Contents of section .text:
 20000000 04000019 e4ffff18.*
#pass
