#ld:
#objdump: -d -M no-aliases

.*:[    ]+file format .*


Disassembly of section .text:

[0-9a-f]+ <_start>:
   +[0-9a-f]+:	03802005 	ori         	\$a1, \$zero, 0x8
   +[0-9a-f]+:	02c02045 	addi.d      	\$a1, \$tp, 8
   +[0-9a-f]+:	03802004 	ori         	\$a0, \$zero, 0x8
   +[0-9a-f]+:	4c000020 	jirl        	\$zero, \$ra, 0
