#as: -mla-global-with-pcrel
#ld: --no-relax -e0
#objdump: -dr
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0+120000100 <.*>:
   120000100:	1a000084 	pcalau12i   	\$a0, .*
   120000104:	02c52001 	li.d        	\$ra, .*
   120000108:	16000001 	lu32i.d     	\$ra, 0
   12000010c:	03000021 	lu52i.d     	\$ra, \$ra, 0
   120000110:	00108484 	add.d       	\$a0, \$a0, \$ra
   120000114:	28c00081 	ld.d        	\$ra, \$a0, 0
   120000118:	4c000021 	jirl        	\$ra, \$ra, 0
   12000011c:	1a000084 	pcalau12i   	\$a0, .*
   120000120:	02c5200d 	li.d        	\$t1, .*
   120000124:	1600000d 	lu32i.d     	\$t1, 0
   120000128:	030001ad 	lu52i.d     	\$t1, \$t1, 0
   12000012c:	0010b484 	add.d       	\$a0, \$a0, \$t1
   120000130:	28c00081 	ld.d        	\$ra, \$a0, 0
   120000134:	4c000021 	jirl        	\$ra, \$ra, 0
