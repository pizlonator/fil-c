#as: -march=kv3-1
#source: np2-detection.s
#objdump: -d

.*\/np2-detection.o:     file format elf64-kvx


Disassembly of section .text:

0000000000000000 <.text>:
   0:	00 80 00 e1                                     	addd \$r0 = \$r0, -512 \(0xfffffe00\)
   4:	00 00 00 e0                                     	make \$r0 = 0 \(0x0\)
   8:	c0 ff 00 e0 c0 ff 00 e0 ff ff df 97             	make \$r0 = 9223372034707292159 \(0x7fffffff7fffffff\)
  14:	ff ff ff 93 ff ff df 9f ff ff ff 1b             	make \$r0 = 9223372034707292159 \(0x7fffffff7fffffff\);;
