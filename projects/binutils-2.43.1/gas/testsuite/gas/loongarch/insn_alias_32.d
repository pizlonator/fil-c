#as:
#objdump: -d -M no-aliases
#skip: loongarch64-*-*

.*     file format .*


Disassembly of section .text:

0+ <L1>:
   0:	001500a4 	or          	\$a0, \$a1, \$zero
   4:	02bffc04 	addi.w      	\$a0, \$zero, -1
   8:	03400000 	andi        	\$zero, \$zero, 0x0
   c:	03800404 	ori         	\$a0, \$zero, 0x1
  10:	4c000020 	jirl        	\$zero, \$ra, 0
  14:	4c000020 	jirl        	\$zero, \$ra, 0
  18:	60000080 	blt         	\$a0, \$zero, 0	# 18 <L1\+0x18>
  1c:	64000080 	bge         	\$a0, \$zero, 0	# 1c <L1\+0x1c>
  20:	64000004 	bge         	\$zero, \$a0, 0	# 20 <L1\+0x20>
