#as:
#objdump: -d
#skip: loongarch32-*-*

.*:[    ]+file format .*


Disassembly of section .text:

0+ <.*>:
   0:	00682041 	bstrins\.w   	\$ra, \$tp, 0x8, 0x8
   4:	00882041 	bstrins\.d   	\$ra, \$tp, 0x8, 0x8
   8:	0068a041 	bstrpick\.w  	\$ra, \$tp, 0x8, 0x8
   c:	00c82041 	bstrpick\.d  	\$ra, \$tp, 0x8, 0x8
  10:	00680041 	bstrins\.w   	\$ra, \$tp, 0x8, 0x0
  14:	00880041 	bstrins\.d   	\$ra, \$tp, 0x8, 0x0
  18:	00688041 	bstrpick\.w  	\$ra, \$tp, 0x8, 0x0
  1c:	00c80041 	bstrpick\.d  	\$ra, \$tp, 0x8, 0x0
