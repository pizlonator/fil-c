#as-new:
#objdump: -d -M no-aliases
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0+ <L1>:
   0:	001500a4 	or          	\$a0, \$a1, \$zero
   4:	02bffc04 	addi.w      	\$a0, \$zero, -1
   8:	02bffc04 	addi.w      	\$a0, \$zero, -1
   c:	03400000 	andi        	\$zero, \$zero, 0x0
  10:	03800404 	ori         	\$a0, \$zero, 0x1
  14:	4c000020 	jirl        	\$zero, \$ra, 0
  18:	4c000020 	jirl        	\$zero, \$ra, 0
  1c:	60000080 	blt         	\$a0, \$zero, 0	# 1c <L1\+0x1c>
  20:	64000080 	bge         	\$a0, \$zero, 0	# 20 <L1\+0x20>
  24:	64000004 	bge         	\$zero, \$a0, 0	# 24 <L1\+0x24>
