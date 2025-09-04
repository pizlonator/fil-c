#as:
#ld: -e0 --no-relax
#objdump: -dr
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

[0-9a-f]+ <fn1>:
   +[0-9a-f]+:	14000004 	lu12i.w     	\$a0, .*
   +[0-9a-f]+:	03800084 	ori         	\$a0, \$a0, .*
   +[0-9a-f]+:	03400000 	nop
   +[0-9a-f]+:	03400000 	nop
