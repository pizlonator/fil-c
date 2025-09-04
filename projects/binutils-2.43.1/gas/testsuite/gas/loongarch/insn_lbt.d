#as:
#objdump: -d
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0+ <.*>:
   0:	00000820 	movgr2scr   	\$scr0, \$ra
   4:	00000c20 	movscr2gr   	\$zero, \$scr1
   8:	48006600 	jiscr0      	100
   c:	48006700 	jiscr1      	100
  10:	00290420 	addu12i.w   	\$zero, \$ra, 1
  14:	00298420 	addu12i.d   	\$zero, \$ra, 1
  18:	00300820 	adc.b       	\$zero, \$ra, \$tp
  1c:	00308820 	adc.h       	\$zero, \$ra, \$tp
  20:	00310820 	adc.w       	\$zero, \$ra, \$tp
  24:	00318820 	adc.d       	\$zero, \$ra, \$tp
  28:	00320820 	sbc.b       	\$zero, \$ra, \$tp
  2c:	00328820 	sbc.h       	\$zero, \$ra, \$tp
  30:	00330820 	sbc.w       	\$zero, \$ra, \$tp
  34:	00338820 	sbc.d       	\$zero, \$ra, \$tp
  38:	001a0820 	rotr.b      	\$zero, \$ra, \$tp
  3c:	001a8820 	rotr.h      	\$zero, \$ra, \$tp
  40:	004c2420 	rotri.b     	\$zero, \$ra, 0x1
  44:	004c4420 	rotri.h     	\$zero, \$ra, 0x1
  48:	00340820 	rcr.b       	\$zero, \$ra, \$tp
  4c:	00348820 	rcr.h       	\$zero, \$ra, \$tp
  50:	00350820 	rcr.w       	\$zero, \$ra, \$tp
  54:	00358820 	rcr.d       	\$zero, \$ra, \$tp
  58:	00502420 	rcri.b      	\$zero, \$ra, 0x1
  5c:	00504420 	rcri.h      	\$zero, \$ra, 0x1
  60:	00508420 	rcri.w      	\$zero, \$ra, 0x1
  64:	00510420 	rcri.d      	\$zero, \$ra, 0x1
  68:	0114e420 	fcvt.ud.d   	\$fa0, \$fa1
  6c:	0114e020 	fcvt.ld.d   	\$fa0, \$fa1
  70:	01150820 	fcvt.d.ld   	\$fa0, \$fa1, \$fa2
  74:	2e800420 	ldl.d       	\$zero, \$ra, 1
  78:	2e000420 	ldl.w       	\$zero, \$ra, 1
  7c:	2e400420 	ldr.w       	\$zero, \$ra, 1
  80:	2ec00420 	ldr.d       	\$zero, \$ra, 1
  84:	2f000420 	stl.w       	\$zero, \$ra, 1
  88:	2f800420 	stl.d       	\$zero, \$ra, 1
  8c:	2f400420 	str.w       	\$zero, \$ra, 1
  90:	2fc00420 	str.d       	\$zero, \$ra, 1
  94:	003f040c 	x86adc.b    	\$zero, \$ra
  98:	003f040d 	x86adc.h    	\$zero, \$ra
  9c:	003f040e 	x86adc.w    	\$zero, \$ra
  a0:	003f040f 	x86adc.d    	\$zero, \$ra
  a4:	003f0404 	x86add.b    	\$zero, \$ra
  a8:	003f0405 	x86add.h    	\$zero, \$ra
  ac:	003f0406 	x86add.w    	\$zero, \$ra
  b0:	003f0407 	x86add.d    	\$zero, \$ra
  b4:	003f0400 	x86add.wu   	\$zero, \$ra
  b8:	003f0401 	x86add.du   	\$zero, \$ra
  bc:	00008000 	x86inc.b    	\$zero
  c0:	00008001 	x86inc.h    	\$zero
  c4:	00008002 	x86inc.w    	\$zero
  c8:	00008003 	x86inc.d    	\$zero
  cc:	003f0410 	x86sbc.b    	\$zero, \$ra
  d0:	003f0411 	x86sbc.h    	\$zero, \$ra
  d4:	003f0412 	x86sbc.w    	\$zero, \$ra
  d8:	003f0413 	x86sbc.d    	\$zero, \$ra
  dc:	003f0408 	x86sub.b    	\$zero, \$ra
  e0:	003f0409 	x86sub.h    	\$zero, \$ra
  e4:	003f040a 	x86sub.w    	\$zero, \$ra
  e8:	003f040b 	x86sub.d    	\$zero, \$ra
  ec:	003f0402 	x86sub.wu   	\$zero, \$ra
  f0:	003f0403 	x86sub.du   	\$zero, \$ra
  f4:	00008004 	x86dec.b    	\$zero
  f8:	00008005 	x86dec.h    	\$zero
  fc:	00008006 	x86dec.w    	\$zero
 100:	00008007 	x86dec.d    	\$zero
 104:	003f8410 	x86and.b    	\$zero, \$ra
 108:	003f8411 	x86and.h    	\$zero, \$ra
 10c:	003f8412 	x86and.w    	\$zero, \$ra
 110:	003f8413 	x86and.d    	\$zero, \$ra
 114:	003f8414 	x86or.b     	\$zero, \$ra
 118:	003f8415 	x86or.h     	\$zero, \$ra
 11c:	003f8416 	x86or.w     	\$zero, \$ra
 120:	003f8417 	x86or.d     	\$zero, \$ra
 124:	003f8418 	x86xor.b    	\$zero, \$ra
 128:	003f8419 	x86xor.h    	\$zero, \$ra
 12c:	003f841a 	x86xor.w    	\$zero, \$ra
 130:	003f841b 	x86xor.d    	\$zero, \$ra
 134:	003e8400 	x86mul.b    	\$zero, \$ra
 138:	003e8401 	x86mul.h    	\$zero, \$ra
 13c:	003e8402 	x86mul.w    	\$zero, \$ra
 140:	003e8403 	x86mul.d    	\$zero, \$ra
 144:	003e8404 	x86mul.bu   	\$zero, \$ra
 148:	003e8405 	x86mul.hu   	\$zero, \$ra
 14c:	003e8406 	x86mul.wu   	\$zero, \$ra
 150:	003e8407 	x86mul.du   	\$zero, \$ra
 154:	003f840c 	x86rcl.b    	\$zero, \$ra
 158:	003f840d 	x86rcl.h    	\$zero, \$ra
 15c:	003f840e 	x86rcl.w    	\$zero, \$ra
 160:	003f840f 	x86rcl.d    	\$zero, \$ra
 164:	00542418 	x86rcli.b   	\$zero, 0x1
 168:	00544419 	x86rcli.h   	\$zero, 0x1
 16c:	0054841a 	x86rcli.w   	\$zero, 0x1
 170:	0055041b 	x86rcli.d   	\$zero, 0x1
 174:	003f8408 	x86rcr.b    	\$zero, \$ra
 178:	003f8409 	x86rcr.h    	\$zero, \$ra
 17c:	003f840a 	x86rcr.w    	\$zero, \$ra
 180:	003f840b 	x86rcr.d    	\$zero, \$ra
 184:	00542410 	x86rcri.b   	\$zero, 0x1
 188:	00544411 	x86rcri.h   	\$zero, 0x1
 18c:	00548412 	x86rcri.w   	\$zero, 0x1
 190:	00550413 	x86rcri.d   	\$zero, 0x1
 194:	003f8404 	x86rotl.b   	\$zero, \$ra
 198:	003f8405 	x86rotl.h   	\$zero, \$ra
 19c:	003f8406 	x86rotl.w   	\$zero, \$ra
 1a0:	003f8407 	x86rotl.d   	\$zero, \$ra
 1a4:	00542414 	x86rotli.b  	\$zero, 0x1
 1a8:	00544415 	x86rotli.h  	\$zero, 0x1
 1ac:	00548416 	x86rotli.w  	\$zero, 0x1
 1b0:	00550417 	x86rotli.d  	\$zero, 0x1
 1b4:	003f8400 	x86rotr.b   	\$zero, \$ra
 1b8:	003f8401 	x86rotr.h   	\$zero, \$ra
 1bc:	003f8402 	x86rotr.d   	\$zero, \$ra
 1c0:	003f8403 	x86rotr.w   	\$zero, \$ra
 1c4:	0054240c 	x86rotri.b  	\$zero, 0x1
 1c8:	0054440d 	x86rotri.h  	\$zero, 0x1
 1cc:	0054840e 	x86rotri.w  	\$zero, 0x1
 1d0:	0055040f 	x86rotri.d  	\$zero, 0x1
 1d4:	003f0414 	x86sll.b    	\$zero, \$ra
 1d8:	003f0415 	x86sll.h    	\$zero, \$ra
 1dc:	003f0416 	x86sll.w    	\$zero, \$ra
 1e0:	003f0417 	x86sll.d    	\$zero, \$ra
 1e4:	00542400 	x86slli.b   	\$zero, 0x1
 1e8:	00544401 	x86slli.h   	\$zero, 0x1
 1ec:	00548402 	x86slli.w   	\$zero, 0x1
 1f0:	00550403 	x86slli.d   	\$zero, 0x1
 1f4:	003f0418 	x86srl.b    	\$zero, \$ra
 1f8:	003f0419 	x86srl.h    	\$zero, \$ra
 1fc:	003f041a 	x86srl.w    	\$zero, \$ra
 200:	003f041b 	x86srl.d    	\$zero, \$ra
 204:	00542404 	x86srli.b   	\$zero, 0x1
 208:	00544405 	x86srli.h   	\$zero, 0x1
 20c:	00548406 	x86srli.w   	\$zero, 0x1
 210:	00550407 	x86srli.d   	\$zero, 0x1
 214:	003f041c 	x86sra.b    	\$zero, \$ra
 218:	003f041d 	x86sra.h    	\$zero, \$ra
 21c:	003f041e 	x86sra.w    	\$zero, \$ra
 220:	003f041f 	x86sra.d    	\$zero, \$ra
 224:	00542408 	x86srai.b   	\$zero, 0x1
 228:	00544409 	x86srai.h   	\$zero, 0x1
 22c:	0054840a 	x86srai.w   	\$zero, 0x1
 230:	0055040b 	x86srai.d   	\$zero, 0x1
 234:	00368400 	setx86j     	\$zero, 0x1
 238:	00007820 	setx86loope 	\$zero, \$ra
 23c:	00007c20 	setx86loopne	\$zero, \$ra
 240:	005c0400 	x86mfflag   	\$zero, 0x1
 244:	005c0420 	x86mtflag   	\$zero, 0x1
 248:	00007400 	x86mftop    	\$zero
 24c:	00007020 	x86mttop    	0x1
 250:	00008009 	x86inctop
 254:	00008029 	x86dectop
 258:	00008008 	x86settm
 25c:	00008028 	x86clrtm
 260:	00580420 	x86settag   	\$zero, 0x1, 0x1
 264:	00370411 	armadd.w    	\$zero, \$ra, 0x1
 268:	00378411 	armsub.w    	\$zero, \$ra, 0x1
 26c:	00380411 	armadc.w    	\$zero, \$ra, 0x1
 270:	00388411 	armsbc.w    	\$zero, \$ra, 0x1
 274:	00390411 	armand.w    	\$zero, \$ra, 0x1
 278:	00398411 	armor.w     	\$zero, \$ra, 0x1
 27c:	003a0411 	armxor.w    	\$zero, \$ra, 0x1
 280:	003fc41c 	armnot.w    	\$zero, 0x1
 284:	003a8411 	armsll.w    	\$zero, \$ra, 0x1
 288:	003b0411 	armsrl.w    	\$zero, \$ra, 0x1
 28c:	003b8411 	armsra.w    	\$zero, \$ra, 0x1
 290:	003c0411 	armrotr.w   	\$zero, \$ra, 0x1
 294:	003c8411 	armslli.w   	\$zero, 0x1, 0x1
 298:	003d0411 	armsrli.w   	\$zero, 0x1, 0x1
 29c:	003d8411 	armsrai.w   	\$zero, 0x1, 0x1
 2a0:	003e0411 	armrotri.w  	\$zero, 0x1, 0x1
 2a4:	003fc41f 	armrrx.w    	\$zero, 0x1
 2a8:	00364420 	armmove     	\$zero, \$ra, 0x1
 2ac:	003fc41d 	armmov.w    	\$zero, 0x1
 2b0:	003fc41e 	armmov.d    	\$zero, 0x1
 2b4:	005c0440 	armmfflag   	\$zero, 0x1
 2b8:	005c0460 	armmtflag   	\$zero, 0x1
 2bc:	0036c400 	setarmj     	\$zero, 0x1
