#source: abi2_max_imm.s
#as: --defsym b16=1
#ld: -e 0 -Ttext 0x20000000 --section-start=.low=0x1ffe0000 --section-start=.high=0x20020000
#objdump: -s

#...
Contents of section .text:
 20000000 8500005a 85fcff59.*
#pass
