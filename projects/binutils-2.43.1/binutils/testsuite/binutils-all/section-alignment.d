#source: pr23633.s
#PROG: objcopy
#objcopy: --section-alignment=512
#objdump: -P sections
#target: [is_pecoff_format]

#...
.* Align: 512.*
#pass
