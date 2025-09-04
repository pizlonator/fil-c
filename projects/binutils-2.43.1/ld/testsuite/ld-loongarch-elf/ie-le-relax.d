#as:
#ld: -e0
#objdump: -dr -M no-aliases
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

[0-9a-f]+ <fn1>:
   +[0-9a-f]+:	03800004 	ori         	\$a0, \$zero, 0x0
   +[0-9a-f]+:	03801004 	ori         	\$a0, \$zero, 0x4
