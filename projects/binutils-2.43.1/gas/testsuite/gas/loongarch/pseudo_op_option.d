#as: -mrelax
#objdump: -dr
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0.* <.text>:
   0:	1a000004 	pcalau12i   	\$a0, 0
			0: R_LARCH_PCALA_HI20	x
			0: R_LARCH_RELAX	\*ABS\*
   4:	02c00084 	addi.d      	\$a0, \$a0, 0
			4: R_LARCH_PCALA_LO12	x
			4: R_LARCH_RELAX	\*ABS\*
   8:	1a000004 	pcalau12i   	\$a0, 0
			8: R_LARCH_PCALA_HI20	x
   c:	02c00084 	addi.d      	\$a0, \$a0, 0
			c: R_LARCH_PCALA_LO12	x
  10:	1a000004 	pcalau12i   	\$a0, 0
			10: R_LARCH_PCALA_HI20	x
			10: R_LARCH_RELAX	\*ABS\*
  14:	02c00084 	addi.d      	\$a0, \$a0, 0
			14: R_LARCH_PCALA_LO12	x
			14: R_LARCH_RELAX	\*ABS\*
  18:	1a000004 	pcalau12i   	\$a0, 0
			18: R_LARCH_PCALA_HI20	x
  1c:	02c00084 	addi.d      	\$a0, \$a0, 0
			1c: R_LARCH_PCALA_LO12	x
  20:	1a000004 	pcalau12i   	\$a0, 0
			20: R_LARCH_PCALA_HI20	x
			20: R_LARCH_RELAX	\*ABS\*
  24:	02c00084 	addi.d      	\$a0, \$a0, 0
			24: R_LARCH_PCALA_LO12	x
			24: R_LARCH_RELAX	\*ABS\*
