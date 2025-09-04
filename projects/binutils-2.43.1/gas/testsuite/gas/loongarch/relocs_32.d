#as: -mthin-add-sub
#objdump: -dr
#skip: loongarch64-*-*

.*:[    ]+file format .*


Disassembly of section .text:

0+ <.*>:
   0:	4c0050a4 	jirl        	\$a0, \$a1, 80
			0: R_LARCH_B16	.L1
   4:	50004c00 	b           	76	# 50 <.L1>
			4: R_LARCH_B26	.L1
   8:	14000004 	lu12i.w     	\$a0, 0
			8: R_LARCH_ABS_HI20	.L1
   c:	038000a4 	ori         	\$a0, \$a1, 0x0
			c: R_LARCH_ABS_LO12	.L1
  10:	1a000004 	pcalau12i   	\$a0, 0
			10: R_LARCH_PCALA_HI20	.L1
  14:	02800085 	addi.w      	\$a1, \$a0, 0
			14: R_LARCH_PCALA_LO12	.L1
  18:	1a000004 	pcalau12i   	\$a0, 0
			18: R_LARCH_GOT_PC_HI20	.L1
  1c:	28800085 	ld.w        	\$a1, \$a0, 0
			1c: R_LARCH_GOT_PC_LO12	.L1
  20:	14000004 	lu12i.w     	\$a0, 0
			20: R_LARCH_GOT_HI20	.L1
  24:	03800084 	ori         	\$a0, \$a0, 0x0
			24: R_LARCH_GOT_LO12	.L1
  28:	14000004 	lu12i.w     	\$a0, 0
			28: R_LARCH_TLS_LE_HI20	TLSL1
			28: R_LARCH_RELAX	\*ABS\*
  2c:	03800085 	ori         	\$a1, \$a0, 0x0
			2c: R_LARCH_TLS_LE_LO12	TLSL1
			2c: R_LARCH_RELAX	\*ABS\*
  30:	1a000004 	pcalau12i   	\$a0, 0
			30: R_LARCH_TLS_IE_PC_HI20	TLSL1
  34:	02c00005 	li.d        	\$a1, 0
			34: R_LARCH_TLS_IE_PC_LO12	TLSL1
  38:	14000004 	lu12i.w     	\$a0, 0
			38: R_LARCH_TLS_IE_HI20	TLSL1
  3c:	03800084 	ori         	\$a0, \$a0, 0x0
			3c: R_LARCH_TLS_IE_LO12	TLSL1
  40:	1a000004 	pcalau12i   	\$a0, 0
			40: R_LARCH_TLS_LD_PC_HI20	TLSL1
  44:	14000004 	lu12i.w     	\$a0, 0
			44: R_LARCH_TLS_LD_HI20	TLSL1
  48:	1a000004 	pcalau12i   	\$a0, 0
			48: R_LARCH_TLS_GD_PC_HI20	TLSL1
  4c:	14000004 	lu12i.w     	\$a0, 0
			4c: R_LARCH_TLS_GD_HI20	TLSL1

0+50 <.L1>:
  50:	00000000 	.word		0x00000000
			50: R_LARCH_32_PCREL	.L2

0+54 <.L2>:
  54:	03400000 	nop
  58:	03400000 	nop
			58: R_LARCH_ALIGN	.*
  5c:	03400000 	nop
  60:	03400000 	nop
  64:	1800000c 	pcaddi      	\$t0, 0
			64: R_LARCH_PCREL20_S2	.L1
  68:	1a000004 	pcalau12i   	\$a0, 0
			68: R_LARCH_TLS_DESC_PC_HI20	TLSL1
  6c:	028000a5 	addi.w      	\$a1, \$a1, 0
			6c: R_LARCH_TLS_DESC_PC_LO12	TLSL1
  70:	14000004 	lu12i.w     	\$a0, 0
			70: R_LARCH_TLS_DESC_HI20	TLSL1
  74:	03800084 	ori         	\$a0, \$a0, 0x0
			74: R_LARCH_TLS_DESC_LO12	TLSL1
  78:	28800081 	ld.w        	\$ra, \$a0, 0
			78: R_LARCH_TLS_DESC_LD	TLSL1
  7c:	4c000021 	jirl        	\$ra, \$ra, 0
			7c: R_LARCH_TLS_DESC_CALL	TLSL1
