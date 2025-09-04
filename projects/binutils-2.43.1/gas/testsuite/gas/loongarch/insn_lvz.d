#as:
#objdump: -d
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0+ <.*>:
   0:	05000400 	gcsrrd      	\$zero, 0x1
   4:	05000420 	gcsrwr      	\$zero, 0x1
   8:	05000483 	gcsrxchg    	\$sp, \$a0, 0x1
   c:	06482401 	gtlbflush
  10:	002b8001 	hvcl        	0x1
