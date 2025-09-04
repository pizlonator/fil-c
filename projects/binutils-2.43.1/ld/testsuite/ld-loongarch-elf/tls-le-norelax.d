#ld: --no-relax
#objdump: -d -M no-aliases

.*:[    ]+file format .*


Disassembly of section .text:

[0-9a-f]+ <_start>:
   +[0-9a-f]+:	14000004 	lu12i.w     	\$a0, 0
   +[0-9a-f]+:	03802085 	ori         	\$a1, \$a0, 0x8
   +[0-9a-f]+:	14000004 	lu12i.w     	\$a0, 0
   +[0-9a-f]+:	02c02085 	addi.d      	\$a1, \$a0, 8
   +[0-9a-f]+:	14000004 	lu12i.w     	\$a0, 0
   +[0-9a-f]+:	03802084 	ori         	\$a0, \$a0, 0x8
   +[0-9a-f]+:	16000004 	lu32i.d     	\$a0, 0
   +[0-9a-f]+:	03000084 	lu52i.d     	\$a0, \$a0, 0
   +[0-9a-f]+:	4c000020 	jirl        	\$zero, \$ra, 0
