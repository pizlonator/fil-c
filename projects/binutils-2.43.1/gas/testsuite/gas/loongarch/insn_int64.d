#as-new:
#objdump: -d -M no-aliases
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0+ <.*>:
   0:	020000a4 	slti        	\$a0, \$a1, 0
   4:	021ffca4 	slti        	\$a0, \$a1, 2047
   8:	022004a4 	slti        	\$a0, \$a1, -2047
   c:	024000a4 	sltui       	\$a0, \$a1, 0
  10:	025ffca4 	sltui       	\$a0, \$a1, 2047
  14:	026004a4 	sltui       	\$a0, \$a1, -2047
  18:	028000a4 	addi.w      	\$a0, \$a1, 0
  1c:	029ffca4 	addi.w      	\$a0, \$a1, 2047
  20:	02a004a4 	addi.w      	\$a0, \$a1, -2047
  24:	02c000a4 	addi.d      	\$a0, \$a1, 0
  28:	02dffca4 	addi.d      	\$a0, \$a1, 2047
  2c:	02e004a4 	addi.d      	\$a0, \$a1, -2047
  30:	030000a4 	lu52i.d     	\$a0, \$a1, 0
  34:	031ffca4 	lu52i.d     	\$a0, \$a1, 2047
  38:	032004a4 	lu52i.d     	\$a0, \$a1, -2047
  3c:	034000a4 	andi        	\$a0, \$a1, 0x0
  40:	035ffca4 	andi        	\$a0, \$a1, 0x7ff
  44:	038000a4 	ori         	\$a0, \$a1, 0x0
  48:	039ffca4 	ori         	\$a0, \$a1, 0x7ff
  4c:	03c000a4 	xori        	\$a0, \$a1, 0x0
  50:	03dffca4 	xori        	\$a0, \$a1, 0x7ff
  54:	100000a4 	addu16i.d   	\$a0, \$a1, 0
  58:	11fffca4 	addu16i.d   	\$a0, \$a1, 32767
  5c:	120004a4 	addu16i.d   	\$a0, \$a1, -32767
  60:	14000004 	lu12i.w     	\$a0, 0
  64:	14ffffe4 	lu12i.w     	\$a0, 524287
  68:	17000024 	lu32i.d     	\$a0, -524287
  6c:	18000004 	pcaddi      	\$a0, 0
  70:	18ffffe4 	pcaddi      	\$a0, 524287
  74:	19000004 	pcaddi      	\$a0, -524288
  78:	1a000004 	pcalau12i   	\$a0, 0
  7c:	1affffe4 	pcalau12i   	\$a0, 524287
  80:	1b000024 	pcalau12i   	\$a0, -524287
  84:	1c000004 	pcaddu12i   	\$a0, 0
  88:	1cffffe4 	pcaddu12i   	\$a0, 524287
  8c:	1d000024 	pcaddu12i   	\$a0, -524287
  90:	1e000004 	pcaddu18i   	\$a0, 0
  94:	1effffe4 	pcaddu18i   	\$a0, 524287
  98:	1f000024 	pcaddu18i   	\$a0, -524287
  9c:	0004b58b 	alsl.w      	\$a7, \$t0, \$t1, 0x2
  a0:	0006b58b 	alsl.wu     	\$a7, \$t0, \$t1, 0x2
  a4:	0009358b 	bytepick.w  	\$a7, \$t0, \$t1, 0x2
  a8:	000d358b 	bytepick.d  	\$a7, \$t0, \$t1, 0x2
  ac:	002a0002 	break       	0x2
  b0:	002a8002 	dbcl        	0x2
  b4:	002b0002 	syscall     	0x2
  b8:	002cb58b 	alsl.d      	\$a7, \$t0, \$t1, 0x2
  bc:	0040898b 	slli.w      	\$a7, \$t0, 0x2
  c0:	0041098b 	slli.d      	\$a7, \$t0, 0x2
  c4:	0044898b 	srli.w      	\$a7, \$t0, 0x2
  c8:	004509ac 	srli.d      	\$t0, \$t1, 0x2
  cc:	004889ac 	srai.w      	\$t0, \$t1, 0x2
  d0:	004909ac 	srai.d      	\$t0, \$t1, 0x2
  d4:	006209ac 	bstrins.w   	\$t0, \$t1, 0x2, 0x2
  d8:	008209ac 	bstrins.d   	\$t0, \$t1, 0x2, 0x2
  dc:	00c209ac 	bstrpick.d  	\$t0, \$t1, 0x2, 0x2
  e0:	00c209ac 	bstrpick.d  	\$t0, \$t1, 0x2, 0x2
  e4:	02048dac 	slti        	\$t0, \$t1, 291
  e8:	02448dac 	sltui       	\$t0, \$t1, 291
  ec:	02848dac 	addi.w      	\$t0, \$t1, 291
  f0:	02c48dac 	addi.d      	\$t0, \$t1, 291
  f4:	03048dac 	lu52i.d     	\$t0, \$t1, 291
  f8:	034009ac 	andi        	\$t0, \$t1, 0x2
  fc:	038009ac 	ori         	\$t0, \$t1, 0x2
 100:	03c009ac 	xori        	\$t0, \$t1, 0x2
 104:	100009ac 	addu16i.d   	\$t0, \$t1, 2
 108:	1400246c 	lu12i.w     	\$t0, 291
 10c:	1600246c 	lu32i.d     	\$t0, 291
 110:	1800000c 	pcaddi      	\$t0, 0
 114:	1a00246c 	pcalau12i   	\$t0, 291
 118:	1c00246c 	pcaddu12i   	\$t0, 291
 11c:	1e00246c 	pcaddu18i   	\$t0, 291
 120:	04048c0c 	csrrd       	\$t0, 0x123
 124:	04048c2c 	csrwr       	\$t0, 0x123
 128:	040009ac 	csrxchg     	\$t0, \$t1, 0x2
 12c:	060009a2 	cacop       	0x2, \$t1, 2
 130:	064009ac 	lddir       	\$t0, \$t1, 0x2
 134:	06440980 	ldpte       	\$t0, 0x2
 138:	0649b9a2 	invtlb      	0x2, \$t1, \$t2
 13c:	000010a4 	clo.w       	\$a0, \$a1
 140:	000014a4 	clz.w       	\$a0, \$a1
 144:	000018a4 	cto.w       	\$a0, \$a1
 148:	00001ca4 	ctz.w       	\$a0, \$a1
 14c:	000020a4 	clo.d       	\$a0, \$a1
 150:	000024a4 	clz.d       	\$a0, \$a1
 154:	000028a4 	cto.d       	\$a0, \$a1
 158:	00002ca4 	ctz.d       	\$a0, \$a1
 15c:	000030a4 	revb.2h     	\$a0, \$a1
 160:	000034a4 	revb.4h     	\$a0, \$a1
 164:	000038a4 	revb.2w     	\$a0, \$a1
 168:	00003ca4 	revb.d      	\$a0, \$a1
 16c:	000040a4 	revh.2w     	\$a0, \$a1
 170:	000044a4 	revh.d      	\$a0, \$a1
 174:	000048a4 	bitrev.4b   	\$a0, \$a1
 178:	00004ca4 	bitrev.8b   	\$a0, \$a1
 17c:	000050a4 	bitrev.w    	\$a0, \$a1
 180:	000054a4 	bitrev.d    	\$a0, \$a1
 184:	000058a4 	ext.w.h     	\$a0, \$a1
 188:	00005ca4 	ext.w.b     	\$a0, \$a1
 18c:	000060a4 	rdtimel.w   	\$a0, \$a1
 190:	000064a4 	rdtimeh.w   	\$a0, \$a1
 194:	000068a4 	rdtime.d    	\$a0, \$a1
 198:	00006ca4 	cpucfg      	\$a0, \$a1
 19c:	000118a0 	asrtle.d    	\$a1, \$a2
 1a0:	000198a0 	asrtgt.d    	\$a1, \$a2
 1a4:	000418a4 	alsl.w      	\$a0, \$a1, \$a2, 0x1
 1a8:	000598a4 	alsl.w      	\$a0, \$a1, \$a2, 0x4
 1ac:	000618a4 	alsl.wu     	\$a0, \$a1, \$a2, 0x1
 1b0:	000798a4 	alsl.wu     	\$a0, \$a1, \$a2, 0x4
 1b4:	000818a4 	bytepick.w  	\$a0, \$a1, \$a2, 0x0
 1b8:	000998a4 	bytepick.w  	\$a0, \$a1, \$a2, 0x3
 1bc:	000c18a4 	bytepick.d  	\$a0, \$a1, \$a2, 0x0
 1c0:	000f98a4 	bytepick.d  	\$a0, \$a1, \$a2, 0x7
 1c4:	001018a4 	add.w       	\$a0, \$a1, \$a2
 1c8:	001098a4 	add.d       	\$a0, \$a1, \$a2
 1cc:	001118a4 	sub.w       	\$a0, \$a1, \$a2
 1d0:	001198a4 	sub.d       	\$a0, \$a1, \$a2
 1d4:	001218a4 	slt         	\$a0, \$a1, \$a2
 1d8:	001298a4 	sltu        	\$a0, \$a1, \$a2
 1dc:	001318a4 	maskeqz     	\$a0, \$a1, \$a2
 1e0:	001398a4 	masknez     	\$a0, \$a1, \$a2
 1e4:	001418a4 	nor         	\$a0, \$a1, \$a2
 1e8:	001498a4 	and         	\$a0, \$a1, \$a2
 1ec:	001518a4 	or          	\$a0, \$a1, \$a2
 1f0:	001598a4 	xor         	\$a0, \$a1, \$a2
 1f4:	001618a4 	orn         	\$a0, \$a1, \$a2
 1f8:	001698a4 	andn        	\$a0, \$a1, \$a2
 1fc:	001718a4 	sll.w       	\$a0, \$a1, \$a2
 200:	001798a4 	srl.w       	\$a0, \$a1, \$a2
 204:	001818a4 	sra.w       	\$a0, \$a1, \$a2
 208:	001898a4 	sll.d       	\$a0, \$a1, \$a2
 20c:	001918a4 	srl.d       	\$a0, \$a1, \$a2
 210:	001998a4 	sra.d       	\$a0, \$a1, \$a2
 214:	001b18a4 	rotr.w      	\$a0, \$a1, \$a2
 218:	001b98a4 	rotr.d      	\$a0, \$a1, \$a2
 21c:	001c18a4 	mul.w       	\$a0, \$a1, \$a2
 220:	001c98a4 	mulh.w      	\$a0, \$a1, \$a2
 224:	001d18a4 	mulh.wu     	\$a0, \$a1, \$a2
 228:	001d98a4 	mul.d       	\$a0, \$a1, \$a2
 22c:	001e18a4 	mulh.d      	\$a0, \$a1, \$a2
 230:	001e98a4 	mulh.du     	\$a0, \$a1, \$a2
 234:	001f18a4 	mulw.d.w    	\$a0, \$a1, \$a2
 238:	001f98a4 	mulw.d.wu   	\$a0, \$a1, \$a2
 23c:	002018a4 	div.w       	\$a0, \$a1, \$a2
 240:	002098a4 	mod.w       	\$a0, \$a1, \$a2
 244:	002118a4 	div.wu      	\$a0, \$a1, \$a2
 248:	002198a4 	mod.wu      	\$a0, \$a1, \$a2
 24c:	002218a4 	div.d       	\$a0, \$a1, \$a2
 250:	002298a4 	mod.d       	\$a0, \$a1, \$a2
 254:	002318a4 	div.du      	\$a0, \$a1, \$a2
 258:	002398a4 	mod.du      	\$a0, \$a1, \$a2
 25c:	002418a4 	crc.w.b.w   	\$a0, \$a1, \$a2
 260:	002498a4 	crc.w.h.w   	\$a0, \$a1, \$a2
 264:	002518a4 	crc.w.w.w   	\$a0, \$a1, \$a2
 268:	002598a4 	crc.w.d.w   	\$a0, \$a1, \$a2
 26c:	002618a4 	crcc.w.b.w  	\$a0, \$a1, \$a2
 270:	002698a4 	crcc.w.h.w  	\$a0, \$a1, \$a2
 274:	002718a4 	crcc.w.w.w  	\$a0, \$a1, \$a2
 278:	002798a4 	crcc.w.d.w  	\$a0, \$a1, \$a2
 27c:	002a0000 	break       	0x0
 280:	002a7fff 	break       	0x7fff
 284:	002a8000 	dbcl        	0x0
 288:	002affff 	dbcl        	0x7fff
 28c:	002c18a4 	alsl.d      	\$a0, \$a1, \$a2, 0x1
 290:	002d98a4 	alsl.d      	\$a0, \$a1, \$a2, 0x4
 294:	004080a4 	slli.w      	\$a0, \$a1, 0x0
 298:	004084a4 	slli.w      	\$a0, \$a1, 0x1
 29c:	0040fca4 	slli.w      	\$a0, \$a1, 0x1f
 2a0:	004100a4 	slli.d      	\$a0, \$a1, 0x0
 2a4:	004104a4 	slli.d      	\$a0, \$a1, 0x1
 2a8:	0041fca4 	slli.d      	\$a0, \$a1, 0x3f
 2ac:	004480a4 	srli.w      	\$a0, \$a1, 0x0
 2b0:	004484a4 	srli.w      	\$a0, \$a1, 0x1
 2b4:	0044fca4 	srli.w      	\$a0, \$a1, 0x1f
 2b8:	004500a4 	srli.d      	\$a0, \$a1, 0x0
 2bc:	004504a4 	srli.d      	\$a0, \$a1, 0x1
 2c0:	0045fca4 	srli.d      	\$a0, \$a1, 0x3f
 2c4:	004880a4 	srai.w      	\$a0, \$a1, 0x0
 2c8:	004884a4 	srai.w      	\$a0, \$a1, 0x1
 2cc:	0048fca4 	srai.w      	\$a0, \$a1, 0x1f
 2d0:	004900a4 	srai.d      	\$a0, \$a1, 0x0
 2d4:	004904a4 	srai.d      	\$a0, \$a1, 0x1
 2d8:	0049fca4 	srai.d      	\$a0, \$a1, 0x3f
 2dc:	004c80a4 	rotri.w     	\$a0, \$a1, 0x0
 2e0:	004c84a4 	rotri.w     	\$a0, \$a1, 0x1
 2e4:	004cfca4 	rotri.w     	\$a0, \$a1, 0x1f
 2e8:	004d00a4 	rotri.d     	\$a0, \$a1, 0x0
 2ec:	004d04a4 	rotri.d     	\$a0, \$a1, 0x1
 2f0:	004dfca4 	rotri.d     	\$a0, \$a1, 0x3f
 2f4:	006000a4 	bstrins.w   	\$a0, \$a1, 0x0, 0x0
 2f8:	006204a4 	bstrins.w   	\$a0, \$a1, 0x2, 0x1
 2fc:	007f00a4 	bstrins.w   	\$a0, \$a1, 0x1f, 0x0
 300:	006080a4 	bstrpick.w  	\$a0, \$a1, 0x0, 0x0
 304:	006284a4 	bstrpick.w  	\$a0, \$a1, 0x2, 0x1
 308:	007f80a4 	bstrpick.w  	\$a0, \$a1, 0x1f, 0x0
 30c:	008000a4 	bstrins.d   	\$a0, \$a1, 0x0, 0x0
 310:	009f04a4 	bstrins.d   	\$a0, \$a1, 0x1f, 0x1
 314:	00a000a4 	bstrins.d   	\$a0, \$a1, 0x20, 0x0
 318:	00bf00a4 	bstrins.d   	\$a0, \$a1, 0x3f, 0x0
 31c:	00c000a4 	bstrpick.d  	\$a0, \$a1, 0x0, 0x0
 320:	00df04a4 	bstrpick.d  	\$a0, \$a1, 0x1f, 0x1
 324:	00e000a4 	bstrpick.d  	\$a0, \$a1, 0x20, 0x0
 328:	00ff00a4 	bstrpick.d  	\$a0, \$a1, 0x3f, 0x0
 32c:	200000a4 	ll.w        	\$a0, \$a1, 0
 330:	203ffca4 	ll.w        	\$a0, \$a1, 16380
 334:	210000a4 	sc.w        	\$a0, \$a1, 0
 338:	213ffca4 	sc.w        	\$a0, \$a1, 16380
 33c:	220000a4 	ll.d        	\$a0, \$a1, 0
 340:	223ffca4 	ll.d        	\$a0, \$a1, 16380
 344:	230000a4 	sc.d        	\$a0, \$a1, 0
 348:	233ffca4 	sc.d        	\$a0, \$a1, 16380
 34c:	240000a4 	ldptr.w     	\$a0, \$a1, 0
 350:	243ffca4 	ldptr.w     	\$a0, \$a1, 16380
 354:	250000a4 	stptr.w     	\$a0, \$a1, 0
 358:	253ffca4 	stptr.w     	\$a0, \$a1, 16380
 35c:	260000a4 	ldptr.d     	\$a0, \$a1, 0
 360:	263ffca4 	ldptr.d     	\$a0, \$a1, 16380
 364:	270000a4 	stptr.d     	\$a0, \$a1, 0
 368:	273ffca4 	stptr.d     	\$a0, \$a1, 16380
 36c:	280000a4 	ld.b        	\$a0, \$a1, 0
 370:	281ffca4 	ld.b        	\$a0, \$a1, 2047
 374:	282004a4 	ld.b        	\$a0, \$a1, -2047
 378:	284000a4 	ld.h        	\$a0, \$a1, 0
 37c:	285ffca4 	ld.h        	\$a0, \$a1, 2047
 380:	286004a4 	ld.h        	\$a0, \$a1, -2047
 384:	288000a4 	ld.w        	\$a0, \$a1, 0
 388:	289ffca4 	ld.w        	\$a0, \$a1, 2047
 38c:	28a004a4 	ld.w        	\$a0, \$a1, -2047
 390:	28c000a4 	ld.d        	\$a0, \$a1, 0
 394:	28dffca4 	ld.d        	\$a0, \$a1, 2047
 398:	28e004a4 	ld.d        	\$a0, \$a1, -2047
 39c:	290000a4 	st.b        	\$a0, \$a1, 0
 3a0:	291ffca4 	st.b        	\$a0, \$a1, 2047
 3a4:	292004a4 	st.b        	\$a0, \$a1, -2047
 3a8:	294000a4 	st.h        	\$a0, \$a1, 0
 3ac:	295ffca4 	st.h        	\$a0, \$a1, 2047
 3b0:	296004a4 	st.h        	\$a0, \$a1, -2047
 3b4:	298000a4 	st.w        	\$a0, \$a1, 0
 3b8:	299ffca4 	st.w        	\$a0, \$a1, 2047
 3bc:	29a004a4 	st.w        	\$a0, \$a1, -2047
 3c0:	29c000a4 	st.d        	\$a0, \$a1, 0
 3c4:	29dffca4 	st.d        	\$a0, \$a1, 2047
 3c8:	29e004a4 	st.d        	\$a0, \$a1, -2047
 3cc:	2a0000a4 	ld.bu       	\$a0, \$a1, 0
 3d0:	2a1ffca4 	ld.bu       	\$a0, \$a1, 2047
 3d4:	2a2004a4 	ld.bu       	\$a0, \$a1, -2047
 3d8:	2a4000a4 	ld.hu       	\$a0, \$a1, 0
 3dc:	2a5ffca4 	ld.hu       	\$a0, \$a1, 2047
 3e0:	2a6004a4 	ld.hu       	\$a0, \$a1, -2047
 3e4:	2a8000a4 	ld.wu       	\$a0, \$a1, 0
 3e8:	2a9ffca4 	ld.wu       	\$a0, \$a1, 2047
 3ec:	2aa004a4 	ld.wu       	\$a0, \$a1, -2047
 3f0:	2ac000a0 	preld       	0x0, \$a1, 0
 3f4:	2adffcbf 	preld       	0x1f, \$a1, 2047
 3f8:	2ae004bf 	preld       	0x1f, \$a1, -2047
 3fc:	380018a4 	ldx.b       	\$a0, \$a1, \$a2
 400:	380418a4 	ldx.h       	\$a0, \$a1, \$a2
 404:	380818a4 	ldx.w       	\$a0, \$a1, \$a2
 408:	380c18a4 	ldx.d       	\$a0, \$a1, \$a2
 40c:	381018a4 	stx.b       	\$a0, \$a1, \$a2
 410:	381418a4 	stx.h       	\$a0, \$a1, \$a2
 414:	381818a4 	stx.w       	\$a0, \$a1, \$a2
 418:	381c18a4 	stx.d       	\$a0, \$a1, \$a2
 41c:	382018a4 	ldx.bu      	\$a0, \$a1, \$a2
 420:	382418a4 	ldx.hu      	\$a0, \$a1, \$a2
 424:	382818a4 	ldx.wu      	\$a0, \$a1, \$a2
 428:	382c18a0 	preldx      	0x0, \$a1, \$a2
 42c:	382c18bf 	preldx      	0x1f, \$a1, \$a2
 430:	38720000 	dbar        	0x0
 434:	38727fff 	dbar        	0x7fff
 438:	38728000 	ibar        	0x0
 43c:	3872ffff 	ibar        	0x7fff
 440:	386014c4 	amswap.w    	\$a0, \$a1, \$a2
 444:	386018a4 	amswap.w    	\$a0, \$a2, \$a1
 448:	386094c4 	amswap.d    	\$a0, \$a1, \$a2
 44c:	386098a4 	amswap.d    	\$a0, \$a2, \$a1
 450:	386114c4 	amadd.w     	\$a0, \$a1, \$a2
 454:	386118a4 	amadd.w     	\$a0, \$a2, \$a1
 458:	386194c4 	amadd.d     	\$a0, \$a1, \$a2
 45c:	386198a4 	amadd.d     	\$a0, \$a2, \$a1
 460:	386214c4 	amand.w     	\$a0, \$a1, \$a2
 464:	386218a4 	amand.w     	\$a0, \$a2, \$a1
 468:	386294c4 	amand.d     	\$a0, \$a1, \$a2
 46c:	386298a4 	amand.d     	\$a0, \$a2, \$a1
 470:	386314c4 	amor.w      	\$a0, \$a1, \$a2
 474:	386318a4 	amor.w      	\$a0, \$a2, \$a1
 478:	386394c4 	amor.d      	\$a0, \$a1, \$a2
 47c:	386398a4 	amor.d      	\$a0, \$a2, \$a1
 480:	386414c4 	amxor.w     	\$a0, \$a1, \$a2
 484:	386418a4 	amxor.w     	\$a0, \$a2, \$a1
 488:	386494c4 	amxor.d     	\$a0, \$a1, \$a2
 48c:	386498a4 	amxor.d     	\$a0, \$a2, \$a1
 490:	386514c4 	ammax.w     	\$a0, \$a1, \$a2
 494:	386518a4 	ammax.w     	\$a0, \$a2, \$a1
 498:	386594c4 	ammax.d     	\$a0, \$a1, \$a2
 49c:	386598a4 	ammax.d     	\$a0, \$a2, \$a1
 4a0:	386614c4 	ammin.w     	\$a0, \$a1, \$a2
 4a4:	386618a4 	ammin.w     	\$a0, \$a2, \$a1
 4a8:	386694c4 	ammin.d     	\$a0, \$a1, \$a2
 4ac:	386698a4 	ammin.d     	\$a0, \$a2, \$a1
 4b0:	386714c4 	ammax.wu    	\$a0, \$a1, \$a2
 4b4:	386718a4 	ammax.wu    	\$a0, \$a2, \$a1
 4b8:	386794c4 	ammax.du    	\$a0, \$a1, \$a2
 4bc:	386798a4 	ammax.du    	\$a0, \$a2, \$a1
 4c0:	386814c4 	ammin.wu    	\$a0, \$a1, \$a2
 4c4:	386818a4 	ammin.wu    	\$a0, \$a2, \$a1
 4c8:	386894c4 	ammin.du    	\$a0, \$a1, \$a2
 4cc:	386898a4 	ammin.du    	\$a0, \$a2, \$a1
 4d0:	386914c4 	amswap_db.w 	\$a0, \$a1, \$a2
 4d4:	386918a4 	amswap_db.w 	\$a0, \$a2, \$a1
 4d8:	386994c4 	amswap_db.d 	\$a0, \$a1, \$a2
 4dc:	386998a4 	amswap_db.d 	\$a0, \$a2, \$a1
 4e0:	386a14c4 	amadd_db.w  	\$a0, \$a1, \$a2
 4e4:	386a18a4 	amadd_db.w  	\$a0, \$a2, \$a1
 4e8:	386a94c4 	amadd_db.d  	\$a0, \$a1, \$a2
 4ec:	386a98a4 	amadd_db.d  	\$a0, \$a2, \$a1
 4f0:	386b14c4 	amand_db.w  	\$a0, \$a1, \$a2
 4f4:	386b18a4 	amand_db.w  	\$a0, \$a2, \$a1
 4f8:	386b94c4 	amand_db.d  	\$a0, \$a1, \$a2
 4fc:	386b98a4 	amand_db.d  	\$a0, \$a2, \$a1
 500:	386c14c4 	amor_db.w   	\$a0, \$a1, \$a2
 504:	386c18a4 	amor_db.w   	\$a0, \$a2, \$a1
 508:	386c94c4 	amor_db.d   	\$a0, \$a1, \$a2
 50c:	386c98a4 	amor_db.d   	\$a0, \$a2, \$a1
 510:	386d14c4 	amxor_db.w  	\$a0, \$a1, \$a2
 514:	386d18a4 	amxor_db.w  	\$a0, \$a2, \$a1
 518:	386d94c4 	amxor_db.d  	\$a0, \$a1, \$a2
 51c:	386d98a4 	amxor_db.d  	\$a0, \$a2, \$a1
 520:	386e14c4 	ammax_db.w  	\$a0, \$a1, \$a2
 524:	386e18a4 	ammax_db.w  	\$a0, \$a2, \$a1
 528:	386e94c4 	ammax_db.d  	\$a0, \$a1, \$a2
 52c:	386e98a4 	ammax_db.d  	\$a0, \$a2, \$a1
 530:	386f14c4 	ammin_db.w  	\$a0, \$a1, \$a2
 534:	386f18a4 	ammin_db.w  	\$a0, \$a2, \$a1
 538:	386f94c4 	ammin_db.d  	\$a0, \$a1, \$a2
 53c:	386f98a4 	ammin_db.d  	\$a0, \$a2, \$a1
 540:	387014c4 	ammax_db.wu 	\$a0, \$a1, \$a2
 544:	387018a4 	ammax_db.wu 	\$a0, \$a2, \$a1
 548:	387094c4 	ammax_db.du 	\$a0, \$a1, \$a2
 54c:	387098a4 	ammax_db.du 	\$a0, \$a2, \$a1
 550:	387114c4 	ammin_db.wu 	\$a0, \$a1, \$a2
 554:	387118a4 	ammin_db.wu 	\$a0, \$a2, \$a1
 558:	387194c4 	ammin_db.du 	\$a0, \$a1, \$a2
 55c:	387198a4 	ammin_db.du 	\$a0, \$a2, \$a1
 560:	387818a4 	ldgt.b      	\$a0, \$a1, \$a2
 564:	387898a4 	ldgt.h      	\$a0, \$a1, \$a2
 568:	387918a4 	ldgt.w      	\$a0, \$a1, \$a2
 56c:	387998a4 	ldgt.d      	\$a0, \$a1, \$a2
 570:	387a18a4 	ldle.b      	\$a0, \$a1, \$a2
 574:	387a98a4 	ldle.h      	\$a0, \$a1, \$a2
 578:	387b18a4 	ldle.w      	\$a0, \$a1, \$a2
 57c:	387b98a4 	ldle.d      	\$a0, \$a1, \$a2
 580:	387c18a4 	stgt.b      	\$a0, \$a1, \$a2
 584:	387c98a4 	stgt.h      	\$a0, \$a1, \$a2
 588:	387d18a4 	stgt.w      	\$a0, \$a1, \$a2
 58c:	387d98a4 	stgt.d      	\$a0, \$a1, \$a2
 590:	387e18a4 	stle.b      	\$a0, \$a1, \$a2
 594:	387e98a4 	stle.h      	\$a0, \$a1, \$a2
 598:	387f18a4 	stle.w      	\$a0, \$a1, \$a2
 59c:	387f98a4 	stle.d      	\$a0, \$a1, \$a2
 5a0:	385714c4 	sc.q        	\$a0, \$a1, \$a2
 5a4:	385714c4 	sc.q        	\$a0, \$a1, \$a2
 5a8:	385780a4 	llacq.w     	\$a0, \$a1
 5ac:	385780a4 	llacq.w     	\$a0, \$a1
 5b0:	385784a4 	screl.w     	\$a0, \$a1
 5b4:	385784a4 	screl.w     	\$a0, \$a1
 5b8:	385788a4 	llacq.d     	\$a0, \$a1
 5bc:	385788a4 	llacq.d     	\$a0, \$a1
 5c0:	38578ca4 	screl.d     	\$a0, \$a1
 5c4:	38578ca4 	screl.d     	\$a0, \$a1
 5c8:	385814c4 	amcas.b     	\$a0, \$a1, \$a2
 5cc:	385818a4 	amcas.b     	\$a0, \$a2, \$a1
 5d0:	385894c4 	amcas.h     	\$a0, \$a1, \$a2
 5d4:	385898a4 	amcas.h     	\$a0, \$a2, \$a1
 5d8:	385914c4 	amcas.w     	\$a0, \$a1, \$a2
 5dc:	385918a4 	amcas.w     	\$a0, \$a2, \$a1
 5e0:	385994c4 	amcas.d     	\$a0, \$a1, \$a2
 5e4:	385998a4 	amcas.d     	\$a0, \$a2, \$a1
 5e8:	385a14c4 	amcas_db.b  	\$a0, \$a1, \$a2
 5ec:	385a18a4 	amcas_db.b  	\$a0, \$a2, \$a1
 5f0:	385a94c4 	amcas_db.h  	\$a0, \$a1, \$a2
 5f4:	385a98a4 	amcas_db.h  	\$a0, \$a2, \$a1
 5f8:	385b14c4 	amcas_db.w  	\$a0, \$a1, \$a2
 5fc:	385b18a4 	amcas_db.w  	\$a0, \$a2, \$a1
 600:	385b94c4 	amcas_db.d  	\$a0, \$a1, \$a2
 604:	385b98a4 	amcas_db.d  	\$a0, \$a2, \$a1
 608:	385c14c4 	amswap.b    	\$a0, \$a1, \$a2
 60c:	385c18a4 	amswap.b    	\$a0, \$a2, \$a1
 610:	385c94c4 	amswap.h    	\$a0, \$a1, \$a2
 614:	385c98a4 	amswap.h    	\$a0, \$a2, \$a1
 618:	385d14c4 	amadd.b     	\$a0, \$a1, \$a2
 61c:	385d18a4 	amadd.b     	\$a0, \$a2, \$a1
 620:	385d94c4 	amadd.h     	\$a0, \$a1, \$a2
 624:	385d98a4 	amadd.h     	\$a0, \$a2, \$a1
 628:	385e14c4 	amswap_db.b 	\$a0, \$a1, \$a2
 62c:	385e18a4 	amswap_db.b 	\$a0, \$a2, \$a1
 630:	385e94c4 	amswap_db.h 	\$a0, \$a1, \$a2
 634:	385e98a4 	amswap_db.h 	\$a0, \$a2, \$a1
 638:	385f14c4 	amadd_db.b  	\$a0, \$a1, \$a2
 63c:	385f18a4 	amadd_db.b  	\$a0, \$a2, \$a1
 640:	385f94c4 	amadd_db.h  	\$a0, \$a1, \$a2
 644:	385f98a4 	amadd_db.h  	\$a0, \$a2, \$a1

0+648 <.L1>:
 648:	03400000 	andi        	\$zero, \$zero, 0x0
 64c:	43fffc9f 	beqz        	\$a0, -4	# 648 <.L1>
 650:	47fff89f 	bnez        	\$a0, -8	# 648 <.L1>
 654:	53fff7ff 	b           	-12	# 648 <.L1>
 658:	57fff3ff 	bl          	-16	# 648 <.L1>
 65c:	5bffec85 	beq         	\$a0, \$a1, -20	# 648 <.L1>
 660:	5fffe885 	bne         	\$a0, \$a1, -24	# 648 <.L1>
 664:	63ffe485 	blt         	\$a0, \$a1, -28	# 648 <.L1>
 668:	63ffe0a4 	blt         	\$a1, \$a0, -32	# 648 <.L1>
 66c:	67ffdc85 	bge         	\$a0, \$a1, -36	# 648 <.L1>
 670:	67ffd8a4 	bge         	\$a1, \$a0, -40	# 648 <.L1>
 674:	6bffd485 	bltu        	\$a0, \$a1, -44	# 648 <.L1>
 678:	6bffd0a4 	bltu        	\$a1, \$a0, -48	# 648 <.L1>
 67c:	6fffcc85 	bgeu        	\$a0, \$a1, -52	# 648 <.L1>
 680:	6fffc8a4 	bgeu        	\$a1, \$a0, -56	# 648 <.L1>
 684:	4c000080 	jirl        	\$zero, \$a0, 0
