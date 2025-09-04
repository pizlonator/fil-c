#as:
#objdump: -d
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0+ <.*>:
   0:	0a118820 	xvfmadd.s   	\$xr0, \$xr1, \$xr2, \$xr3
   4:	0a518820 	xvfmsub.s   	\$xr0, \$xr1, \$xr2, \$xr3
   8:	0a918820 	xvfnmadd.s  	\$xr0, \$xr1, \$xr2, \$xr3
   c:	0ad18820 	xvfnmsub.s  	\$xr0, \$xr1, \$xr2, \$xr3
  10:	0c900820 	xvfcmp.caf.s	\$xr0, \$xr1, \$xr2
  14:	0c908820 	xvfcmp.saf.s	\$xr0, \$xr1, \$xr2
  18:	0c910820 	xvfcmp.clt.s	\$xr0, \$xr1, \$xr2
  1c:	0c918820 	xvfcmp.slt.s	\$xr0, \$xr1, \$xr2
  20:	0c920820 	xvfcmp.ceq.s	\$xr0, \$xr1, \$xr2
  24:	0c928820 	xvfcmp.seq.s	\$xr0, \$xr1, \$xr2
  28:	0c930820 	xvfcmp.cle.s	\$xr0, \$xr1, \$xr2
  2c:	0c938820 	xvfcmp.sle.s	\$xr0, \$xr1, \$xr2
  30:	0c940820 	xvfcmp.cun.s	\$xr0, \$xr1, \$xr2
  34:	0c948820 	xvfcmp.sun.s	\$xr0, \$xr1, \$xr2
  38:	0c950820 	xvfcmp.cult.s	\$xr0, \$xr1, \$xr2
  3c:	0c958820 	xvfcmp.sult.s	\$xr0, \$xr1, \$xr2
  40:	0c960820 	xvfcmp.cueq.s	\$xr0, \$xr1, \$xr2
  44:	0c968820 	xvfcmp.sueq.s	\$xr0, \$xr1, \$xr2
  48:	0c970820 	xvfcmp.cule.s	\$xr0, \$xr1, \$xr2
  4c:	0c978820 	xvfcmp.sule.s	\$xr0, \$xr1, \$xr2
  50:	0c980820 	xvfcmp.cne.s	\$xr0, \$xr1, \$xr2
  54:	0c988820 	xvfcmp.sne.s	\$xr0, \$xr1, \$xr2
  58:	0c9a0820 	xvfcmp.cor.s	\$xr0, \$xr1, \$xr2
  5c:	0c9a8820 	xvfcmp.sor.s	\$xr0, \$xr1, \$xr2
  60:	0c9c0820 	xvfcmp.cune.s	\$xr0, \$xr1, \$xr2
  64:	0c9c8820 	xvfcmp.sune.s	\$xr0, \$xr1, \$xr2
  68:	0d218820 	xvbitsel.v  	\$xr0, \$xr1, \$xr2, \$xr3
  6c:	0d618820 	xvshuf.b    	\$xr0, \$xr1, \$xr2, \$xr3
  70:	0a218820 	xvfmadd.d   	\$xr0, \$xr1, \$xr2, \$xr3
  74:	0a618820 	xvfmsub.d   	\$xr0, \$xr1, \$xr2, \$xr3
  78:	0aa18820 	xvfnmadd.d  	\$xr0, \$xr1, \$xr2, \$xr3
  7c:	0ae18820 	xvfnmsub.d  	\$xr0, \$xr1, \$xr2, \$xr3
  80:	0ca00820 	xvfcmp.caf.d	\$xr0, \$xr1, \$xr2
  84:	0ca08820 	xvfcmp.saf.d	\$xr0, \$xr1, \$xr2
  88:	0ca10820 	xvfcmp.clt.d	\$xr0, \$xr1, \$xr2
  8c:	0ca18820 	xvfcmp.slt.d	\$xr0, \$xr1, \$xr2
  90:	0ca20820 	xvfcmp.ceq.d	\$xr0, \$xr1, \$xr2
  94:	0ca28820 	xvfcmp.seq.d	\$xr0, \$xr1, \$xr2
  98:	0ca30820 	xvfcmp.cle.d	\$xr0, \$xr1, \$xr2
  9c:	0ca38820 	xvfcmp.sle.d	\$xr0, \$xr1, \$xr2
  a0:	0ca40820 	xvfcmp.cun.d	\$xr0, \$xr1, \$xr2
  a4:	0ca48820 	xvfcmp.sun.d	\$xr0, \$xr1, \$xr2
  a8:	0ca50820 	xvfcmp.cult.d	\$xr0, \$xr1, \$xr2
  ac:	0ca58820 	xvfcmp.sult.d	\$xr0, \$xr1, \$xr2
  b0:	0ca60820 	xvfcmp.cueq.d	\$xr0, \$xr1, \$xr2
  b4:	0ca68820 	xvfcmp.sueq.d	\$xr0, \$xr1, \$xr2
  b8:	0ca70820 	xvfcmp.cule.d	\$xr0, \$xr1, \$xr2
  bc:	0ca78820 	xvfcmp.sule.d	\$xr0, \$xr1, \$xr2
  c0:	0ca80820 	xvfcmp.cne.d	\$xr0, \$xr1, \$xr2
  c4:	0ca88820 	xvfcmp.sne.d	\$xr0, \$xr1, \$xr2
  c8:	0caa0820 	xvfcmp.cor.d	\$xr0, \$xr1, \$xr2
  cc:	0caa8820 	xvfcmp.sor.d	\$xr0, \$xr1, \$xr2
  d0:	0cac0820 	xvfcmp.cune.d	\$xr0, \$xr1, \$xr2
  d4:	0cac8820 	xvfcmp.sune.d	\$xr0, \$xr1, \$xr2
  d8:	2c800420 	xvld        	\$xr0, \$ra, 1
  dc:	2cc00420 	xvst        	\$xr0, \$ra, 1
  e0:	38480820 	xvldx       	\$xr0, \$ra, \$tp
  e4:	384c0820 	xvstx       	\$xr0, \$ra, \$tp
  e8:	3211f420 	xvldrepl.d  	\$xr0, \$ra, 1000
  ec:	32206420 	xvldrepl.w  	\$xr0, \$ra, 100
  f0:	32401420 	xvldrepl.h  	\$xr0, \$ra, 10
  f4:	32800420 	xvldrepl.b  	\$xr0, \$ra, 1
  f8:	3315f420 	xvstelm.d   	\$xr0, \$ra, 1000, 0x1
  fc:	33246420 	xvstelm.w   	\$xr0, \$ra, 100, 0x1
 100:	33441420 	xvstelm.h   	\$xr0, \$ra, 10, 0x1
 104:	33840420 	xvstelm.b   	\$xr0, \$ra, 1, 0x1
 108:	74000820 	xvseq.b     	\$xr0, \$xr1, \$xr2
 10c:	74008820 	xvseq.h     	\$xr0, \$xr1, \$xr2
 110:	74010820 	xvseq.w     	\$xr0, \$xr1, \$xr2
 114:	74018820 	xvseq.d     	\$xr0, \$xr1, \$xr2
 118:	74020820 	xvsle.b     	\$xr0, \$xr1, \$xr2
 11c:	74028820 	xvsle.h     	\$xr0, \$xr1, \$xr2
 120:	74030820 	xvsle.w     	\$xr0, \$xr1, \$xr2
 124:	74038820 	xvsle.d     	\$xr0, \$xr1, \$xr2
 128:	74040820 	xvsle.bu    	\$xr0, \$xr1, \$xr2
 12c:	74048820 	xvsle.hu    	\$xr0, \$xr1, \$xr2
 130:	74050820 	xvsle.wu    	\$xr0, \$xr1, \$xr2
 134:	74058820 	xvsle.du    	\$xr0, \$xr1, \$xr2
 138:	74060820 	xvslt.b     	\$xr0, \$xr1, \$xr2
 13c:	74068820 	xvslt.h     	\$xr0, \$xr1, \$xr2
 140:	74070820 	xvslt.w     	\$xr0, \$xr1, \$xr2
 144:	74078820 	xvslt.d     	\$xr0, \$xr1, \$xr2
 148:	74080820 	xvslt.bu    	\$xr0, \$xr1, \$xr2
 14c:	74088820 	xvslt.hu    	\$xr0, \$xr1, \$xr2
 150:	74090820 	xvslt.wu    	\$xr0, \$xr1, \$xr2
 154:	74098820 	xvslt.du    	\$xr0, \$xr1, \$xr2
 158:	740a0820 	xvadd.b     	\$xr0, \$xr1, \$xr2
 15c:	740a8820 	xvadd.h     	\$xr0, \$xr1, \$xr2
 160:	740b0820 	xvadd.w     	\$xr0, \$xr1, \$xr2
 164:	740b8820 	xvadd.d     	\$xr0, \$xr1, \$xr2
 168:	740c0820 	xvsub.b     	\$xr0, \$xr1, \$xr2
 16c:	740c8820 	xvsub.h     	\$xr0, \$xr1, \$xr2
 170:	740d0820 	xvsub.w     	\$xr0, \$xr1, \$xr2
 174:	740d8820 	xvsub.d     	\$xr0, \$xr1, \$xr2
 178:	74460820 	xvsadd.b    	\$xr0, \$xr1, \$xr2
 17c:	74468820 	xvsadd.h    	\$xr0, \$xr1, \$xr2
 180:	74470820 	xvsadd.w    	\$xr0, \$xr1, \$xr2
 184:	74478820 	xvsadd.d    	\$xr0, \$xr1, \$xr2
 188:	74480820 	xvssub.b    	\$xr0, \$xr1, \$xr2
 18c:	74488820 	xvssub.h    	\$xr0, \$xr1, \$xr2
 190:	74490820 	xvssub.w    	\$xr0, \$xr1, \$xr2
 194:	74498820 	xvssub.d    	\$xr0, \$xr1, \$xr2
 198:	744a0820 	xvsadd.bu   	\$xr0, \$xr1, \$xr2
 19c:	744a8820 	xvsadd.hu   	\$xr0, \$xr1, \$xr2
 1a0:	744b0820 	xvsadd.wu   	\$xr0, \$xr1, \$xr2
 1a4:	744b8820 	xvsadd.du   	\$xr0, \$xr1, \$xr2
 1a8:	744c0820 	xvssub.bu   	\$xr0, \$xr1, \$xr2
 1ac:	744c8820 	xvssub.hu   	\$xr0, \$xr1, \$xr2
 1b0:	744d0820 	xvssub.wu   	\$xr0, \$xr1, \$xr2
 1b4:	744d8820 	xvssub.du   	\$xr0, \$xr1, \$xr2
 1b8:	74540820 	xvhaddw.h.b 	\$xr0, \$xr1, \$xr2
 1bc:	74548820 	xvhaddw.w.h 	\$xr0, \$xr1, \$xr2
 1c0:	74550820 	xvhaddw.d.w 	\$xr0, \$xr1, \$xr2
 1c4:	74558820 	xvhaddw.q.d 	\$xr0, \$xr1, \$xr2
 1c8:	74560820 	xvhsubw.h.b 	\$xr0, \$xr1, \$xr2
 1cc:	74568820 	xvhsubw.w.h 	\$xr0, \$xr1, \$xr2
 1d0:	74570820 	xvhsubw.d.w 	\$xr0, \$xr1, \$xr2
 1d4:	74578820 	xvhsubw.q.d 	\$xr0, \$xr1, \$xr2
 1d8:	74580820 	xvhaddw.hu.bu	\$xr0, \$xr1, \$xr2
 1dc:	74588820 	xvhaddw.wu.hu	\$xr0, \$xr1, \$xr2
 1e0:	74590820 	xvhaddw.du.wu	\$xr0, \$xr1, \$xr2
 1e4:	74598820 	xvhaddw.qu.du	\$xr0, \$xr1, \$xr2
 1e8:	745a0820 	xvhsubw.hu.bu	\$xr0, \$xr1, \$xr2
 1ec:	745a8820 	xvhsubw.wu.hu	\$xr0, \$xr1, \$xr2
 1f0:	745b0820 	xvhsubw.du.wu	\$xr0, \$xr1, \$xr2
 1f4:	745b8820 	xvhsubw.qu.du	\$xr0, \$xr1, \$xr2
 1f8:	741e0820 	xvaddwev.h.b	\$xr0, \$xr1, \$xr2
 1fc:	741e8820 	xvaddwev.w.h	\$xr0, \$xr1, \$xr2
 200:	741f0820 	xvaddwev.d.w	\$xr0, \$xr1, \$xr2
 204:	741f8820 	xvaddwev.q.d	\$xr0, \$xr1, \$xr2
 208:	742e0820 	xvaddwev.h.bu	\$xr0, \$xr1, \$xr2
 20c:	742e8820 	xvaddwev.w.hu	\$xr0, \$xr1, \$xr2
 210:	742f0820 	xvaddwev.d.wu	\$xr0, \$xr1, \$xr2
 214:	742f8820 	xvaddwev.q.du	\$xr0, \$xr1, \$xr2
 218:	743e0820 	xvaddwev.h.bu.b	\$xr0, \$xr1, \$xr2
 21c:	743e8820 	xvaddwev.w.hu.h	\$xr0, \$xr1, \$xr2
 220:	743f0820 	xvaddwev.d.wu.w	\$xr0, \$xr1, \$xr2
 224:	743f8820 	xvaddwev.q.du.d	\$xr0, \$xr1, \$xr2
 228:	74220820 	xvaddwod.h.b	\$xr0, \$xr1, \$xr2
 22c:	74228820 	xvaddwod.w.h	\$xr0, \$xr1, \$xr2
 230:	74230820 	xvaddwod.d.w	\$xr0, \$xr1, \$xr2
 234:	74238820 	xvaddwod.q.d	\$xr0, \$xr1, \$xr2
 238:	74320820 	xvaddwod.h.bu	\$xr0, \$xr1, \$xr2
 23c:	74328820 	xvaddwod.w.hu	\$xr0, \$xr1, \$xr2
 240:	74330820 	xvaddwod.d.wu	\$xr0, \$xr1, \$xr2
 244:	74338820 	xvaddwod.q.du	\$xr0, \$xr1, \$xr2
 248:	74400820 	xvaddwod.h.bu.b	\$xr0, \$xr1, \$xr2
 24c:	74408820 	xvaddwod.w.hu.h	\$xr0, \$xr1, \$xr2
 250:	74410820 	xvaddwod.d.wu.w	\$xr0, \$xr1, \$xr2
 254:	74418820 	xvaddwod.q.du.d	\$xr0, \$xr1, \$xr2
 258:	74ac0820 	xvmaddwev.h.b	\$xr0, \$xr1, \$xr2
 25c:	74ac8820 	xvmaddwev.w.h	\$xr0, \$xr1, \$xr2
 260:	74ad0820 	xvmaddwev.d.w	\$xr0, \$xr1, \$xr2
 264:	74ad8820 	xvmaddwev.q.d	\$xr0, \$xr1, \$xr2
 268:	74bc0820 	xvmaddwev.h.bu.b	\$xr0, \$xr1, \$xr2
 26c:	74bc8820 	xvmaddwev.w.hu.h	\$xr0, \$xr1, \$xr2
 270:	74bd0820 	xvmaddwev.d.wu.w	\$xr0, \$xr1, \$xr2
 274:	74bd8820 	xvmaddwev.q.du.d	\$xr0, \$xr1, \$xr2
 278:	74b40820 	xvmaddwev.h.bu	\$xr0, \$xr1, \$xr2
 27c:	74b48820 	xvmaddwev.w.hu	\$xr0, \$xr1, \$xr2
 280:	74b50820 	xvmaddwev.d.wu	\$xr0, \$xr1, \$xr2
 284:	74b58820 	xvmaddwev.q.du	\$xr0, \$xr1, \$xr2
 288:	74ae0820 	xvmaddwod.h.b	\$xr0, \$xr1, \$xr2
 28c:	74ae8820 	xvmaddwod.w.h	\$xr0, \$xr1, \$xr2
 290:	74af0820 	xvmaddwod.d.w	\$xr0, \$xr1, \$xr2
 294:	74af8820 	xvmaddwod.q.d	\$xr0, \$xr1, \$xr2
 298:	74b60820 	xvmaddwod.h.bu	\$xr0, \$xr1, \$xr2
 29c:	74b68820 	xvmaddwod.w.hu	\$xr0, \$xr1, \$xr2
 2a0:	74b70820 	xvmaddwod.d.wu	\$xr0, \$xr1, \$xr2
 2a4:	74b78820 	xvmaddwod.q.du	\$xr0, \$xr1, \$xr2
 2a8:	74be0820 	xvmaddwod.h.bu.b	\$xr0, \$xr1, \$xr2
 2ac:	74be8820 	xvmaddwod.w.hu.h	\$xr0, \$xr1, \$xr2
 2b0:	74bf0820 	xvmaddwod.d.wu.w	\$xr0, \$xr1, \$xr2
 2b4:	74bf8820 	xvmaddwod.q.du.d	\$xr0, \$xr1, \$xr2
 2b8:	74900820 	xvmulwev.h.b	\$xr0, \$xr1, \$xr2
 2bc:	74908820 	xvmulwev.w.h	\$xr0, \$xr1, \$xr2
 2c0:	74910820 	xvmulwev.d.w	\$xr0, \$xr1, \$xr2
 2c4:	74918820 	xvmulwev.q.d	\$xr0, \$xr1, \$xr2
 2c8:	74980820 	xvmulwev.h.bu	\$xr0, \$xr1, \$xr2
 2cc:	74988820 	xvmulwev.w.hu	\$xr0, \$xr1, \$xr2
 2d0:	74990820 	xvmulwev.d.wu	\$xr0, \$xr1, \$xr2
 2d4:	74998820 	xvmulwev.q.du	\$xr0, \$xr1, \$xr2
 2d8:	74a00820 	xvmulwev.h.bu.b	\$xr0, \$xr1, \$xr2
 2dc:	74a08820 	xvmulwev.w.hu.h	\$xr0, \$xr1, \$xr2
 2e0:	74a10820 	xvmulwev.d.wu.w	\$xr0, \$xr1, \$xr2
 2e4:	74a18820 	xvmulwev.q.du.d	\$xr0, \$xr1, \$xr2
 2e8:	74920820 	xvmulwod.h.b	\$xr0, \$xr1, \$xr2
 2ec:	74928820 	xvmulwod.w.h	\$xr0, \$xr1, \$xr2
 2f0:	74930820 	xvmulwod.d.w	\$xr0, \$xr1, \$xr2
 2f4:	74938820 	xvmulwod.q.d	\$xr0, \$xr1, \$xr2
 2f8:	749a0820 	xvmulwod.h.bu	\$xr0, \$xr1, \$xr2
 2fc:	749a8820 	xvmulwod.w.hu	\$xr0, \$xr1, \$xr2
 300:	749b0820 	xvmulwod.d.wu	\$xr0, \$xr1, \$xr2
 304:	749b8820 	xvmulwod.q.du	\$xr0, \$xr1, \$xr2
 308:	74a20820 	xvmulwod.h.bu.b	\$xr0, \$xr1, \$xr2
 30c:	74a28820 	xvmulwod.w.hu.h	\$xr0, \$xr1, \$xr2
 310:	74a30820 	xvmulwod.d.wu.w	\$xr0, \$xr1, \$xr2
 314:	74a38820 	xvmulwod.q.du.d	\$xr0, \$xr1, \$xr2
 318:	74200820 	xvsubwev.h.b	\$xr0, \$xr1, \$xr2
 31c:	74208820 	xvsubwev.w.h	\$xr0, \$xr1, \$xr2
 320:	74210820 	xvsubwev.d.w	\$xr0, \$xr1, \$xr2
 324:	74218820 	xvsubwev.q.d	\$xr0, \$xr1, \$xr2
 328:	74300820 	xvsubwev.h.bu	\$xr0, \$xr1, \$xr2
 32c:	74308820 	xvsubwev.w.hu	\$xr0, \$xr1, \$xr2
 330:	74310820 	xvsubwev.d.wu	\$xr0, \$xr1, \$xr2
 334:	74318820 	xvsubwev.q.du	\$xr0, \$xr1, \$xr2
 338:	74240820 	xvsubwod.h.b	\$xr0, \$xr1, \$xr2
 33c:	74248820 	xvsubwod.w.h	\$xr0, \$xr1, \$xr2
 340:	74250820 	xvsubwod.d.w	\$xr0, \$xr1, \$xr2
 344:	74258820 	xvsubwod.q.d	\$xr0, \$xr1, \$xr2
 348:	74340820 	xvsubwod.h.bu	\$xr0, \$xr1, \$xr2
 34c:	74348820 	xvsubwod.w.hu	\$xr0, \$xr1, \$xr2
 350:	74350820 	xvsubwod.d.wu	\$xr0, \$xr1, \$xr2
 354:	74358820 	xvsubwod.q.du	\$xr0, \$xr1, \$xr2
 358:	745c0820 	xvadda.b    	\$xr0, \$xr1, \$xr2
 35c:	745c8820 	xvadda.h    	\$xr0, \$xr1, \$xr2
 360:	745d0820 	xvadda.w    	\$xr0, \$xr1, \$xr2
 364:	745d8820 	xvadda.d    	\$xr0, \$xr1, \$xr2
 368:	74600820 	xvabsd.b    	\$xr0, \$xr1, \$xr2
 36c:	74608820 	xvabsd.h    	\$xr0, \$xr1, \$xr2
 370:	74610820 	xvabsd.w    	\$xr0, \$xr1, \$xr2
 374:	74618820 	xvabsd.d    	\$xr0, \$xr1, \$xr2
 378:	74620820 	xvabsd.bu   	\$xr0, \$xr1, \$xr2
 37c:	74628820 	xvabsd.hu   	\$xr0, \$xr1, \$xr2
 380:	74630820 	xvabsd.wu   	\$xr0, \$xr1, \$xr2
 384:	74638820 	xvabsd.du   	\$xr0, \$xr1, \$xr2
 388:	74640820 	xvavg.b     	\$xr0, \$xr1, \$xr2
 38c:	74648820 	xvavg.h     	\$xr0, \$xr1, \$xr2
 390:	74650820 	xvavg.w     	\$xr0, \$xr1, \$xr2
 394:	74658820 	xvavg.d     	\$xr0, \$xr1, \$xr2
 398:	74660820 	xvavg.bu    	\$xr0, \$xr1, \$xr2
 39c:	74668820 	xvavg.hu    	\$xr0, \$xr1, \$xr2
 3a0:	74670820 	xvavg.wu    	\$xr0, \$xr1, \$xr2
 3a4:	74678820 	xvavg.du    	\$xr0, \$xr1, \$xr2
 3a8:	74680820 	xvavgr.b    	\$xr0, \$xr1, \$xr2
 3ac:	74688820 	xvavgr.h    	\$xr0, \$xr1, \$xr2
 3b0:	74690820 	xvavgr.w    	\$xr0, \$xr1, \$xr2
 3b4:	74698820 	xvavgr.d    	\$xr0, \$xr1, \$xr2
 3b8:	746a0820 	xvavgr.bu   	\$xr0, \$xr1, \$xr2
 3bc:	746a8820 	xvavgr.hu   	\$xr0, \$xr1, \$xr2
 3c0:	746b0820 	xvavgr.wu   	\$xr0, \$xr1, \$xr2
 3c4:	746b8820 	xvavgr.du   	\$xr0, \$xr1, \$xr2
 3c8:	74700820 	xvmax.b     	\$xr0, \$xr1, \$xr2
 3cc:	74708820 	xvmax.h     	\$xr0, \$xr1, \$xr2
 3d0:	74710820 	xvmax.w     	\$xr0, \$xr1, \$xr2
 3d4:	74718820 	xvmax.d     	\$xr0, \$xr1, \$xr2
 3d8:	74720820 	xvmin.b     	\$xr0, \$xr1, \$xr2
 3dc:	74728820 	xvmin.h     	\$xr0, \$xr1, \$xr2
 3e0:	74730820 	xvmin.w     	\$xr0, \$xr1, \$xr2
 3e4:	74738820 	xvmin.d     	\$xr0, \$xr1, \$xr2
 3e8:	74740820 	xvmax.bu    	\$xr0, \$xr1, \$xr2
 3ec:	74748820 	xvmax.hu    	\$xr0, \$xr1, \$xr2
 3f0:	74750820 	xvmax.wu    	\$xr0, \$xr1, \$xr2
 3f4:	74758820 	xvmax.du    	\$xr0, \$xr1, \$xr2
 3f8:	74760820 	xvmin.bu    	\$xr0, \$xr1, \$xr2
 3fc:	74768820 	xvmin.hu    	\$xr0, \$xr1, \$xr2
 400:	74770820 	xvmin.wu    	\$xr0, \$xr1, \$xr2
 404:	74778820 	xvmin.du    	\$xr0, \$xr1, \$xr2
 408:	74840820 	xvmul.b     	\$xr0, \$xr1, \$xr2
 40c:	74848820 	xvmul.h     	\$xr0, \$xr1, \$xr2
 410:	74850820 	xvmul.w     	\$xr0, \$xr1, \$xr2
 414:	74858820 	xvmul.d     	\$xr0, \$xr1, \$xr2
 418:	74860820 	xvmuh.b     	\$xr0, \$xr1, \$xr2
 41c:	74868820 	xvmuh.h     	\$xr0, \$xr1, \$xr2
 420:	74870820 	xvmuh.w     	\$xr0, \$xr1, \$xr2
 424:	74878820 	xvmuh.d     	\$xr0, \$xr1, \$xr2
 428:	74880820 	xvmuh.bu    	\$xr0, \$xr1, \$xr2
 42c:	74888820 	xvmuh.hu    	\$xr0, \$xr1, \$xr2
 430:	74890820 	xvmuh.wu    	\$xr0, \$xr1, \$xr2
 434:	74898820 	xvmuh.du    	\$xr0, \$xr1, \$xr2
 438:	74a80820 	xvmadd.b    	\$xr0, \$xr1, \$xr2
 43c:	74a88820 	xvmadd.h    	\$xr0, \$xr1, \$xr2
 440:	74a90820 	xvmadd.w    	\$xr0, \$xr1, \$xr2
 444:	74a98820 	xvmadd.d    	\$xr0, \$xr1, \$xr2
 448:	74aa0820 	xvmsub.b    	\$xr0, \$xr1, \$xr2
 44c:	74aa8820 	xvmsub.h    	\$xr0, \$xr1, \$xr2
 450:	74ab0820 	xvmsub.w    	\$xr0, \$xr1, \$xr2
 454:	74ab8820 	xvmsub.d    	\$xr0, \$xr1, \$xr2
 458:	74e00820 	xvdiv.b     	\$xr0, \$xr1, \$xr2
 45c:	74e08820 	xvdiv.h     	\$xr0, \$xr1, \$xr2
 460:	74e10820 	xvdiv.w     	\$xr0, \$xr1, \$xr2
 464:	74e18820 	xvdiv.d     	\$xr0, \$xr1, \$xr2
 468:	74e20820 	xvmod.b     	\$xr0, \$xr1, \$xr2
 46c:	74e28820 	xvmod.h     	\$xr0, \$xr1, \$xr2
 470:	74e30820 	xvmod.w     	\$xr0, \$xr1, \$xr2
 474:	74e38820 	xvmod.d     	\$xr0, \$xr1, \$xr2
 478:	74e40820 	xvdiv.bu    	\$xr0, \$xr1, \$xr2
 47c:	74e48820 	xvdiv.hu    	\$xr0, \$xr1, \$xr2
 480:	74e50820 	xvdiv.wu    	\$xr0, \$xr1, \$xr2
 484:	74e58820 	xvdiv.du    	\$xr0, \$xr1, \$xr2
 488:	74e60820 	xvmod.bu    	\$xr0, \$xr1, \$xr2
 48c:	74e68820 	xvmod.hu    	\$xr0, \$xr1, \$xr2
 490:	74e70820 	xvmod.wu    	\$xr0, \$xr1, \$xr2
 494:	74e78820 	xvmod.du    	\$xr0, \$xr1, \$xr2
 498:	74e80820 	xvsll.b     	\$xr0, \$xr1, \$xr2
 49c:	74e88820 	xvsll.h     	\$xr0, \$xr1, \$xr2
 4a0:	74e90820 	xvsll.w     	\$xr0, \$xr1, \$xr2
 4a4:	74e98820 	xvsll.d     	\$xr0, \$xr1, \$xr2
 4a8:	74ea0820 	xvsrl.b     	\$xr0, \$xr1, \$xr2
 4ac:	74ea8820 	xvsrl.h     	\$xr0, \$xr1, \$xr2
 4b0:	74eb0820 	xvsrl.w     	\$xr0, \$xr1, \$xr2
 4b4:	74eb8820 	xvsrl.d     	\$xr0, \$xr1, \$xr2
 4b8:	74ec0820 	xvsra.b     	\$xr0, \$xr1, \$xr2
 4bc:	74ec8820 	xvsra.h     	\$xr0, \$xr1, \$xr2
 4c0:	74ed0820 	xvsra.w     	\$xr0, \$xr1, \$xr2
 4c4:	74ed8820 	xvsra.d     	\$xr0, \$xr1, \$xr2
 4c8:	74ee0820 	xvrotr.b    	\$xr0, \$xr1, \$xr2
 4cc:	74ee8820 	xvrotr.h    	\$xr0, \$xr1, \$xr2
 4d0:	74ef0820 	xvrotr.w    	\$xr0, \$xr1, \$xr2
 4d4:	74ef8820 	xvrotr.d    	\$xr0, \$xr1, \$xr2
 4d8:	74f00820 	xvsrlr.b    	\$xr0, \$xr1, \$xr2
 4dc:	74f08820 	xvsrlr.h    	\$xr0, \$xr1, \$xr2
 4e0:	74f10820 	xvsrlr.w    	\$xr0, \$xr1, \$xr2
 4e4:	74f18820 	xvsrlr.d    	\$xr0, \$xr1, \$xr2
 4e8:	74f20820 	xvsrar.b    	\$xr0, \$xr1, \$xr2
 4ec:	74f28820 	xvsrar.h    	\$xr0, \$xr1, \$xr2
 4f0:	74f30820 	xvsrar.w    	\$xr0, \$xr1, \$xr2
 4f4:	74f38820 	xvsrar.d    	\$xr0, \$xr1, \$xr2
 4f8:	74f48820 	xvsrln.b.h  	\$xr0, \$xr1, \$xr2
 4fc:	74f50820 	xvsrln.h.w  	\$xr0, \$xr1, \$xr2
 500:	74f58820 	xvsrln.w.d  	\$xr0, \$xr1, \$xr2
 504:	74f68820 	xvsran.b.h  	\$xr0, \$xr1, \$xr2
 508:	74f70820 	xvsran.h.w  	\$xr0, \$xr1, \$xr2
 50c:	74f78820 	xvsran.w.d  	\$xr0, \$xr1, \$xr2
 510:	74f88820 	xvsrlrn.b.h 	\$xr0, \$xr1, \$xr2
 514:	74f90820 	xvsrlrn.h.w 	\$xr0, \$xr1, \$xr2
 518:	74f98820 	xvsrlrn.w.d 	\$xr0, \$xr1, \$xr2
 51c:	74fa8820 	xvsrarn.b.h 	\$xr0, \$xr1, \$xr2
 520:	74fb0820 	xvsrarn.h.w 	\$xr0, \$xr1, \$xr2
 524:	74fb8820 	xvsrarn.w.d 	\$xr0, \$xr1, \$xr2
 528:	74fc8820 	xvssrln.b.h 	\$xr0, \$xr1, \$xr2
 52c:	74fd0820 	xvssrln.h.w 	\$xr0, \$xr1, \$xr2
 530:	74fd8820 	xvssrln.w.d 	\$xr0, \$xr1, \$xr2
 534:	74fe8820 	xvssran.b.h 	\$xr0, \$xr1, \$xr2
 538:	74ff0820 	xvssran.h.w 	\$xr0, \$xr1, \$xr2
 53c:	74ff8820 	xvssran.w.d 	\$xr0, \$xr1, \$xr2
 540:	75008820 	xvssrlrn.b.h	\$xr0, \$xr1, \$xr2
 544:	75010820 	xvssrlrn.h.w	\$xr0, \$xr1, \$xr2
 548:	75018820 	xvssrlrn.w.d	\$xr0, \$xr1, \$xr2
 54c:	75028820 	xvssrarn.b.h	\$xr0, \$xr1, \$xr2
 550:	75030820 	xvssrarn.h.w	\$xr0, \$xr1, \$xr2
 554:	75038820 	xvssrarn.w.d	\$xr0, \$xr1, \$xr2
 558:	75048820 	xvssrln.bu.h	\$xr0, \$xr1, \$xr2
 55c:	75050820 	xvssrln.hu.w	\$xr0, \$xr1, \$xr2
 560:	75058820 	xvssrln.wu.d	\$xr0, \$xr1, \$xr2
 564:	75068820 	xvssran.bu.h	\$xr0, \$xr1, \$xr2
 568:	75070820 	xvssran.hu.w	\$xr0, \$xr1, \$xr2
 56c:	75078820 	xvssran.wu.d	\$xr0, \$xr1, \$xr2
 570:	75088820 	xvssrlrn.bu.h	\$xr0, \$xr1, \$xr2
 574:	75090820 	xvssrlrn.hu.w	\$xr0, \$xr1, \$xr2
 578:	75098820 	xvssrlrn.wu.d	\$xr0, \$xr1, \$xr2
 57c:	750a8820 	xvssrarn.bu.h	\$xr0, \$xr1, \$xr2
 580:	750b0820 	xvssrarn.hu.w	\$xr0, \$xr1, \$xr2
 584:	750b8820 	xvssrarn.wu.d	\$xr0, \$xr1, \$xr2
 588:	750c0820 	xvbitclr.b  	\$xr0, \$xr1, \$xr2
 58c:	750c8820 	xvbitclr.h  	\$xr0, \$xr1, \$xr2
 590:	750d0820 	xvbitclr.w  	\$xr0, \$xr1, \$xr2
 594:	750d8820 	xvbitclr.d  	\$xr0, \$xr1, \$xr2
 598:	750e0820 	xvbitset.b  	\$xr0, \$xr1, \$xr2
 59c:	750e8820 	xvbitset.h  	\$xr0, \$xr1, \$xr2
 5a0:	750f0820 	xvbitset.w  	\$xr0, \$xr1, \$xr2
 5a4:	750f8820 	xvbitset.d  	\$xr0, \$xr1, \$xr2
 5a8:	75100820 	xvbitrev.b  	\$xr0, \$xr1, \$xr2
 5ac:	75108820 	xvbitrev.h  	\$xr0, \$xr1, \$xr2
 5b0:	75110820 	xvbitrev.w  	\$xr0, \$xr1, \$xr2
 5b4:	75118820 	xvbitrev.d  	\$xr0, \$xr1, \$xr2
 5b8:	75160820 	xvpackev.b  	\$xr0, \$xr1, \$xr2
 5bc:	75168820 	xvpackev.h  	\$xr0, \$xr1, \$xr2
 5c0:	75170820 	xvpackev.w  	\$xr0, \$xr1, \$xr2
 5c4:	75178820 	xvpackev.d  	\$xr0, \$xr1, \$xr2
 5c8:	75180820 	xvpackod.b  	\$xr0, \$xr1, \$xr2
 5cc:	75188820 	xvpackod.h  	\$xr0, \$xr1, \$xr2
 5d0:	75190820 	xvpackod.w  	\$xr0, \$xr1, \$xr2
 5d4:	75198820 	xvpackod.d  	\$xr0, \$xr1, \$xr2
 5d8:	751a0820 	xvilvl.b    	\$xr0, \$xr1, \$xr2
 5dc:	751a8820 	xvilvl.h    	\$xr0, \$xr1, \$xr2
 5e0:	751b0820 	xvilvl.w    	\$xr0, \$xr1, \$xr2
 5e4:	751b8820 	xvilvl.d    	\$xr0, \$xr1, \$xr2
 5e8:	751c0820 	xvilvh.b    	\$xr0, \$xr1, \$xr2
 5ec:	751c8820 	xvilvh.h    	\$xr0, \$xr1, \$xr2
 5f0:	751d0820 	xvilvh.w    	\$xr0, \$xr1, \$xr2
 5f4:	751d8820 	xvilvh.d    	\$xr0, \$xr1, \$xr2
 5f8:	751e0820 	xvpickev.b  	\$xr0, \$xr1, \$xr2
 5fc:	751e8820 	xvpickev.h  	\$xr0, \$xr1, \$xr2
 600:	751f0820 	xvpickev.w  	\$xr0, \$xr1, \$xr2
 604:	751f8820 	xvpickev.d  	\$xr0, \$xr1, \$xr2
 608:	75200820 	xvpickod.b  	\$xr0, \$xr1, \$xr2
 60c:	75208820 	xvpickod.h  	\$xr0, \$xr1, \$xr2
 610:	75210820 	xvpickod.w  	\$xr0, \$xr1, \$xr2
 614:	75218820 	xvpickod.d  	\$xr0, \$xr1, \$xr2
 618:	75220820 	xvreplve.b  	\$xr0, \$xr1, \$tp
 61c:	75228820 	xvreplve.h  	\$xr0, \$xr1, \$tp
 620:	75230820 	xvreplve.w  	\$xr0, \$xr1, \$tp
 624:	75238820 	xvreplve.d  	\$xr0, \$xr1, \$tp
 628:	75260820 	xvand.v     	\$xr0, \$xr1, \$xr2
 62c:	75268820 	xvor.v      	\$xr0, \$xr1, \$xr2
 630:	75270820 	xvxor.v     	\$xr0, \$xr1, \$xr2
 634:	75278820 	xvnor.v     	\$xr0, \$xr1, \$xr2
 638:	75280820 	xvandn.v    	\$xr0, \$xr1, \$xr2
 63c:	75288820 	xvorn.v     	\$xr0, \$xr1, \$xr2
 640:	752b0820 	xvfrstp.b   	\$xr0, \$xr1, \$xr2
 644:	752b8820 	xvfrstp.h   	\$xr0, \$xr1, \$xr2
 648:	752d0820 	xvadd.q     	\$xr0, \$xr1, \$xr2
 64c:	752d8820 	xvsub.q     	\$xr0, \$xr1, \$xr2
 650:	752e0820 	xvsigncov.b 	\$xr0, \$xr1, \$xr2
 654:	752e8820 	xvsigncov.h 	\$xr0, \$xr1, \$xr2
 658:	752f0820 	xvsigncov.w 	\$xr0, \$xr1, \$xr2
 65c:	752f8820 	xvsigncov.d 	\$xr0, \$xr1, \$xr2
 660:	75308820 	xvfadd.s    	\$xr0, \$xr1, \$xr2
 664:	75310820 	xvfadd.d    	\$xr0, \$xr1, \$xr2
 668:	75328820 	xvfsub.s    	\$xr0, \$xr1, \$xr2
 66c:	75330820 	xvfsub.d    	\$xr0, \$xr1, \$xr2
 670:	75388820 	xvfmul.s    	\$xr0, \$xr1, \$xr2
 674:	75390820 	xvfmul.d    	\$xr0, \$xr1, \$xr2
 678:	753a8820 	xvfdiv.s    	\$xr0, \$xr1, \$xr2
 67c:	753b0820 	xvfdiv.d    	\$xr0, \$xr1, \$xr2
 680:	753c8820 	xvfmax.s    	\$xr0, \$xr1, \$xr2
 684:	753d0820 	xvfmax.d    	\$xr0, \$xr1, \$xr2
 688:	753e8820 	xvfmin.s    	\$xr0, \$xr1, \$xr2
 68c:	753f0820 	xvfmin.d    	\$xr0, \$xr1, \$xr2
 690:	75408820 	xvfmaxa.s   	\$xr0, \$xr1, \$xr2
 694:	75410820 	xvfmaxa.d   	\$xr0, \$xr1, \$xr2
 698:	75428820 	xvfmina.s   	\$xr0, \$xr1, \$xr2
 69c:	75430820 	xvfmina.d   	\$xr0, \$xr1, \$xr2
 6a0:	75460820 	xvfcvt.h.s  	\$xr0, \$xr1, \$xr2
 6a4:	75468820 	xvfcvt.s.d  	\$xr0, \$xr1, \$xr2
 6a8:	75480820 	xvffint.s.l 	\$xr0, \$xr1, \$xr2
 6ac:	75498820 	xvftint.w.d 	\$xr0, \$xr1, \$xr2
 6b0:	754a0820 	xvftintrm.w.d	\$xr0, \$xr1, \$xr2
 6b4:	754a8820 	xvftintrp.w.d	\$xr0, \$xr1, \$xr2
 6b8:	754b0820 	xvftintrz.w.d	\$xr0, \$xr1, \$xr2
 6bc:	754b8820 	xvftintrne.w.d	\$xr0, \$xr1, \$xr2
 6c0:	757a8820 	xvshuf.h    	\$xr0, \$xr1, \$xr2
 6c4:	757b0820 	xvshuf.w    	\$xr0, \$xr1, \$xr2
 6c8:	757b8820 	xvshuf.d    	\$xr0, \$xr1, \$xr2
 6cc:	757d0820 	xvperm.w    	\$xr0, \$xr1, \$xr2
 6d0:	76800420 	xvseqi.b    	\$xr0, \$xr1, 1
 6d4:	76808420 	xvseqi.h    	\$xr0, \$xr1, 1
 6d8:	76810420 	xvseqi.w    	\$xr0, \$xr1, 1
 6dc:	76818420 	xvseqi.d    	\$xr0, \$xr1, 1
 6e0:	76820420 	xvslei.b    	\$xr0, \$xr1, 1
 6e4:	76828420 	xvslei.h    	\$xr0, \$xr1, 1
 6e8:	76830420 	xvslei.w    	\$xr0, \$xr1, 1
 6ec:	76838420 	xvslei.d    	\$xr0, \$xr1, 1
 6f0:	76840420 	xvslei.bu   	\$xr0, \$xr1, 0x1
 6f4:	76848420 	xvslei.hu   	\$xr0, \$xr1, 0x1
 6f8:	76850420 	xvslei.wu   	\$xr0, \$xr1, 0x1
 6fc:	76858420 	xvslei.du   	\$xr0, \$xr1, 0x1
 700:	76860420 	xvslti.b    	\$xr0, \$xr1, 1
 704:	76868420 	xvslti.h    	\$xr0, \$xr1, 1
 708:	76870420 	xvslti.w    	\$xr0, \$xr1, 1
 70c:	76878420 	xvslti.d    	\$xr0, \$xr1, 1
 710:	76880420 	xvslti.bu   	\$xr0, \$xr1, 0x1
 714:	76888420 	xvslti.hu   	\$xr0, \$xr1, 0x1
 718:	76890420 	xvslti.wu   	\$xr0, \$xr1, 0x1
 71c:	76898420 	xvslti.du   	\$xr0, \$xr1, 0x1
 720:	768a0420 	xvaddi.bu   	\$xr0, \$xr1, 0x1
 724:	768a8420 	xvaddi.hu   	\$xr0, \$xr1, 0x1
 728:	768b0420 	xvaddi.wu   	\$xr0, \$xr1, 0x1
 72c:	768b8420 	xvaddi.du   	\$xr0, \$xr1, 0x1
 730:	768c0420 	xvsubi.bu   	\$xr0, \$xr1, 0x1
 734:	768c8420 	xvsubi.hu   	\$xr0, \$xr1, 0x1
 738:	768d0420 	xvsubi.wu   	\$xr0, \$xr1, 0x1
 73c:	768d8420 	xvsubi.du   	\$xr0, \$xr1, 0x1
 740:	768e0420 	xvbsll.v    	\$xr0, \$xr1, 0x1
 744:	768e8420 	xvbsrl.v    	\$xr0, \$xr1, 0x1
 748:	76900420 	xvmaxi.b    	\$xr0, \$xr1, 1
 74c:	76908420 	xvmaxi.h    	\$xr0, \$xr1, 1
 750:	76910420 	xvmaxi.w    	\$xr0, \$xr1, 1
 754:	76918420 	xvmaxi.d    	\$xr0, \$xr1, 1
 758:	76920420 	xvmini.b    	\$xr0, \$xr1, 1
 75c:	76928420 	xvmini.h    	\$xr0, \$xr1, 1
 760:	76930420 	xvmini.w    	\$xr0, \$xr1, 1
 764:	76938420 	xvmini.d    	\$xr0, \$xr1, 1
 768:	76940420 	xvmaxi.bu   	\$xr0, \$xr1, 0x1
 76c:	76948420 	xvmaxi.hu   	\$xr0, \$xr1, 0x1
 770:	76950420 	xvmaxi.wu   	\$xr0, \$xr1, 0x1
 774:	76958420 	xvmaxi.du   	\$xr0, \$xr1, 0x1
 778:	76960420 	xvmini.bu   	\$xr0, \$xr1, 0x1
 77c:	76968420 	xvmini.hu   	\$xr0, \$xr1, 0x1
 780:	76970420 	xvmini.wu   	\$xr0, \$xr1, 0x1
 784:	76978420 	xvmini.du   	\$xr0, \$xr1, 0x1
 788:	769a0420 	xvfrstpi.b  	\$xr0, \$xr1, 0x1
 78c:	769a8420 	xvfrstpi.h  	\$xr0, \$xr1, 0x1
 790:	769c0020 	xvclo.b     	\$xr0, \$xr1
 794:	769c0420 	xvclo.h     	\$xr0, \$xr1
 798:	769c0820 	xvclo.w     	\$xr0, \$xr1
 79c:	769c0c20 	xvclo.d     	\$xr0, \$xr1
 7a0:	769c1020 	xvclz.b     	\$xr0, \$xr1
 7a4:	769c1420 	xvclz.h     	\$xr0, \$xr1
 7a8:	769c1820 	xvclz.w     	\$xr0, \$xr1
 7ac:	769c1c20 	xvclz.d     	\$xr0, \$xr1
 7b0:	769c2020 	xvpcnt.b    	\$xr0, \$xr1
 7b4:	769c2420 	xvpcnt.h    	\$xr0, \$xr1
 7b8:	769c2820 	xvpcnt.w    	\$xr0, \$xr1
 7bc:	769c2c20 	xvpcnt.d    	\$xr0, \$xr1
 7c0:	769c3020 	xvneg.b     	\$xr0, \$xr1
 7c4:	769c3420 	xvneg.h     	\$xr0, \$xr1
 7c8:	769c3820 	xvneg.w     	\$xr0, \$xr1
 7cc:	769c3c20 	xvneg.d     	\$xr0, \$xr1
 7d0:	769c4020 	xvmskltz.b  	\$xr0, \$xr1
 7d4:	769c4420 	xvmskltz.h  	\$xr0, \$xr1
 7d8:	769c4820 	xvmskltz.w  	\$xr0, \$xr1
 7dc:	769c4c20 	xvmskltz.d  	\$xr0, \$xr1
 7e0:	769c5020 	xvmskgez.b  	\$xr0, \$xr1
 7e4:	769c6020 	xvmsknz.b   	\$xr0, \$xr1
 7e8:	769c9820 	xvseteqz.v  	\$fcc0, \$xr1
 7ec:	769c9c20 	xvsetnez.v  	\$fcc0, \$xr1
 7f0:	769ca020 	xvsetanyeqz.b	\$fcc0, \$xr1
 7f4:	769ca420 	xvsetanyeqz.h	\$fcc0, \$xr1
 7f8:	769ca820 	xvsetanyeqz.w	\$fcc0, \$xr1
 7fc:	769cac20 	xvsetanyeqz.d	\$fcc0, \$xr1
 800:	769cb020 	xvsetallnez.b	\$fcc0, \$xr1
 804:	769cb420 	xvsetallnez.h	\$fcc0, \$xr1
 808:	769cb820 	xvsetallnez.w	\$fcc0, \$xr1
 80c:	769cbc20 	xvsetallnez.d	\$fcc0, \$xr1
 810:	769cc420 	xvflogb.s   	\$xr0, \$xr1
 814:	769cc820 	xvflogb.d   	\$xr0, \$xr1
 818:	769cd420 	xvfclass.s  	\$xr0, \$xr1
 81c:	769cd820 	xvfclass.d  	\$xr0, \$xr1
 820:	769ce420 	xvfsqrt.s   	\$xr0, \$xr1
 824:	769ce820 	xvfsqrt.d   	\$xr0, \$xr1
 828:	769cf420 	xvfrecip.s  	\$xr0, \$xr1
 82c:	769cf820 	xvfrecip.d  	\$xr0, \$xr1
 830:	769d0420 	xvfrsqrt.s  	\$xr0, \$xr1
 834:	769d0820 	xvfrsqrt.d  	\$xr0, \$xr1
 838:	769d3420 	xvfrint.s   	\$xr0, \$xr1
 83c:	769d3820 	xvfrint.d   	\$xr0, \$xr1
 840:	769d4420 	xvfrintrm.s 	\$xr0, \$xr1
 844:	769d4820 	xvfrintrm.d 	\$xr0, \$xr1
 848:	769d5420 	xvfrintrp.s 	\$xr0, \$xr1
 84c:	769d5820 	xvfrintrp.d 	\$xr0, \$xr1
 850:	769d6420 	xvfrintrz.s 	\$xr0, \$xr1
 854:	769d6820 	xvfrintrz.d 	\$xr0, \$xr1
 858:	769d7420 	xvfrintrne.s	\$xr0, \$xr1
 85c:	769d7820 	xvfrintrne.d	\$xr0, \$xr1
 860:	769de820 	xvfcvtl.s.h 	\$xr0, \$xr1
 864:	769dec20 	xvfcvth.s.h 	\$xr0, \$xr1
 868:	769df020 	xvfcvtl.d.s 	\$xr0, \$xr1
 86c:	769df420 	xvfcvth.d.s 	\$xr0, \$xr1
 870:	769e0020 	xvffint.s.w 	\$xr0, \$xr1
 874:	769e0420 	xvffint.s.wu	\$xr0, \$xr1
 878:	769e0820 	xvffint.d.l 	\$xr0, \$xr1
 87c:	769e0c20 	xvffint.d.lu	\$xr0, \$xr1
 880:	769e1020 	xvffintl.d.w	\$xr0, \$xr1
 884:	769e1420 	xvffinth.d.w	\$xr0, \$xr1
 888:	769e3020 	xvftint.w.s 	\$xr0, \$xr1
 88c:	769e3420 	xvftint.l.d 	\$xr0, \$xr1
 890:	769e3820 	xvftintrm.w.s	\$xr0, \$xr1
 894:	769e3c20 	xvftintrm.l.d	\$xr0, \$xr1
 898:	769e4020 	xvftintrp.w.s	\$xr0, \$xr1
 89c:	769e4420 	xvftintrp.l.d	\$xr0, \$xr1
 8a0:	769e4820 	xvftintrz.w.s	\$xr0, \$xr1
 8a4:	769e4c20 	xvftintrz.l.d	\$xr0, \$xr1
 8a8:	769e5020 	xvftintrne.w.s	\$xr0, \$xr1
 8ac:	769e5420 	xvftintrne.l.d	\$xr0, \$xr1
 8b0:	769e5820 	xvftint.wu.s	\$xr0, \$xr1
 8b4:	769e5c20 	xvftint.lu.d	\$xr0, \$xr1
 8b8:	769e7020 	xvftintrz.wu.s	\$xr0, \$xr1
 8bc:	769e7420 	xvftintrz.lu.d	\$xr0, \$xr1
 8c0:	769e8020 	xvftintl.l.s	\$xr0, \$xr1
 8c4:	769e8420 	xvftinth.l.s	\$xr0, \$xr1
 8c8:	769e8820 	xvftintrml.l.s	\$xr0, \$xr1
 8cc:	769e8c20 	xvftintrmh.l.s	\$xr0, \$xr1
 8d0:	769e9020 	xvftintrpl.l.s	\$xr0, \$xr1
 8d4:	769e9420 	xvftintrph.l.s	\$xr0, \$xr1
 8d8:	769e9820 	xvftintrzl.l.s	\$xr0, \$xr1
 8dc:	769e9c20 	xvftintrzh.l.s	\$xr0, \$xr1
 8e0:	769ea020 	xvftintrnel.l.s	\$xr0, \$xr1
 8e4:	769ea420 	xvftintrneh.l.s	\$xr0, \$xr1
 8e8:	769ee020 	xvexth.h.b  	\$xr0, \$xr1
 8ec:	769ee420 	xvexth.w.h  	\$xr0, \$xr1
 8f0:	769ee820 	xvexth.d.w  	\$xr0, \$xr1
 8f4:	769eec20 	xvexth.q.d  	\$xr0, \$xr1
 8f8:	769ef020 	xvexth.hu.bu	\$xr0, \$xr1
 8fc:	769ef420 	xvexth.wu.hu	\$xr0, \$xr1
 900:	769ef820 	xvexth.du.wu	\$xr0, \$xr1
 904:	769efc20 	xvexth.qu.du	\$xr0, \$xr1
 908:	769f0020 	xvreplgr2vr.b	\$xr0, \$ra
 90c:	769f0420 	xvreplgr2vr.h	\$xr0, \$ra
 910:	769f0820 	xvreplgr2vr.w	\$xr0, \$ra
 914:	769f0c20 	xvreplgr2vr.d	\$xr0, \$ra
 918:	769f8420 	xvhseli.d   	\$xr0, \$xr1, 0x1
 91c:	76a02420 	xvrotri.b   	\$xr0, \$xr1, 0x1
 920:	76a04420 	xvrotri.h   	\$xr0, \$xr1, 0x1
 924:	76a08420 	xvrotri.w   	\$xr0, \$xr1, 0x1
 928:	76a10420 	xvrotri.d   	\$xr0, \$xr1, 0x1
 92c:	76a42420 	xvsrlri.b   	\$xr0, \$xr1, 0x1
 930:	76a44420 	xvsrlri.h   	\$xr0, \$xr1, 0x1
 934:	76a48420 	xvsrlri.w   	\$xr0, \$xr1, 0x1
 938:	76a50420 	xvsrlri.d   	\$xr0, \$xr1, 0x1
 93c:	76a82420 	xvsrari.b   	\$xr0, \$xr1, 0x1
 940:	76a84420 	xvsrari.h   	\$xr0, \$xr1, 0x1
 944:	76a88420 	xvsrari.w   	\$xr0, \$xr1, 0x1
 948:	76a90420 	xvsrari.d   	\$xr0, \$xr1, 0x1
 94c:	76ebc420 	xvinsgr2vr.w	\$xr0, \$ra, 0x1
 950:	76ebe420 	xvinsgr2vr.d	\$xr0, \$ra, 0x1
 954:	76efc420 	xvpickve2gr.w	\$zero, \$xr1, 0x1
 958:	76efe420 	xvpickve2gr.d	\$zero, \$xr1, 0x1
 95c:	76f3c420 	xvpickve2gr.wu	\$zero, \$xr1, 0x1
 960:	76f3e420 	xvpickve2gr.du	\$zero, \$xr1, 0x1
 964:	76f78420 	xvrepl128vei.b	\$xr0, \$xr1, 0x1
 968:	76f7c420 	xvrepl128vei.h	\$xr0, \$xr1, 0x1
 96c:	76f7e420 	xvrepl128vei.w	\$xr0, \$xr1, 0x1
 970:	76f7f420 	xvrepl128vei.d	\$xr0, \$xr1, 0x1
 974:	76ffc420 	xvinsve0.w  	\$xr0, \$xr1, 0x1
 978:	76ffe420 	xvinsve0.d  	\$xr0, \$xr1, 0x1
 97c:	7703c420 	xvpickve.w  	\$xr0, \$xr1, 0x1
 980:	7703e420 	xvpickve.d  	\$xr0, \$xr1, 0x1
 984:	77070020 	xvreplve0.b 	\$xr0, \$xr1
 988:	77078020 	xvreplve0.h 	\$xr0, \$xr1
 98c:	7707c020 	xvreplve0.w 	\$xr0, \$xr1
 990:	7707e020 	xvreplve0.d 	\$xr0, \$xr1
 994:	7707f020 	xvreplve0.q 	\$xr0, \$xr1
 998:	77082420 	xvsllwil.h.b	\$xr0, \$xr1, 0x1
 99c:	77084420 	xvsllwil.w.h	\$xr0, \$xr1, 0x1
 9a0:	77088420 	xvsllwil.d.w	\$xr0, \$xr1, 0x1
 9a4:	77090020 	xvextl.q.d  	\$xr0, \$xr1
 9a8:	770c2420 	xvsllwil.hu.bu	\$xr0, \$xr1, 0x1
 9ac:	770c4420 	xvsllwil.wu.hu	\$xr0, \$xr1, 0x1
 9b0:	770c8420 	xvsllwil.du.wu	\$xr0, \$xr1, 0x1
 9b4:	770d0020 	xvextl.qu.du	\$xr0, \$xr1
 9b8:	77102420 	xvbitclri.b 	\$xr0, \$xr1, 0x1
 9bc:	77104420 	xvbitclri.h 	\$xr0, \$xr1, 0x1
 9c0:	77108420 	xvbitclri.w 	\$xr0, \$xr1, 0x1
 9c4:	77110420 	xvbitclri.d 	\$xr0, \$xr1, 0x1
 9c8:	77142420 	xvbitseti.b 	\$xr0, \$xr1, 0x1
 9cc:	77144420 	xvbitseti.h 	\$xr0, \$xr1, 0x1
 9d0:	77148420 	xvbitseti.w 	\$xr0, \$xr1, 0x1
 9d4:	77150420 	xvbitseti.d 	\$xr0, \$xr1, 0x1
 9d8:	77182420 	xvbitrevi.b 	\$xr0, \$xr1, 0x1
 9dc:	77184420 	xvbitrevi.h 	\$xr0, \$xr1, 0x1
 9e0:	77188420 	xvbitrevi.w 	\$xr0, \$xr1, 0x1
 9e4:	77190420 	xvbitrevi.d 	\$xr0, \$xr1, 0x1
 9e8:	77242420 	xvsat.b     	\$xr0, \$xr1, 0x1
 9ec:	77244420 	xvsat.h     	\$xr0, \$xr1, 0x1
 9f0:	77248420 	xvsat.w     	\$xr0, \$xr1, 0x1
 9f4:	77250420 	xvsat.d     	\$xr0, \$xr1, 0x1
 9f8:	77282420 	xvsat.bu    	\$xr0, \$xr1, 0x1
 9fc:	77284420 	xvsat.hu    	\$xr0, \$xr1, 0x1
 a00:	77288420 	xvsat.wu    	\$xr0, \$xr1, 0x1
 a04:	77290420 	xvsat.du    	\$xr0, \$xr1, 0x1
 a08:	772c2420 	xvslli.b    	\$xr0, \$xr1, 0x1
 a0c:	772c4420 	xvslli.h    	\$xr0, \$xr1, 0x1
 a10:	772c8420 	xvslli.w    	\$xr0, \$xr1, 0x1
 a14:	772d0420 	xvslli.d    	\$xr0, \$xr1, 0x1
 a18:	77302420 	xvsrli.b    	\$xr0, \$xr1, 0x1
 a1c:	77304420 	xvsrli.h    	\$xr0, \$xr1, 0x1
 a20:	77308420 	xvsrli.w    	\$xr0, \$xr1, 0x1
 a24:	77310420 	xvsrli.d    	\$xr0, \$xr1, 0x1
 a28:	77342420 	xvsrai.b    	\$xr0, \$xr1, 0x1
 a2c:	77344420 	xvsrai.h    	\$xr0, \$xr1, 0x1
 a30:	77348420 	xvsrai.w    	\$xr0, \$xr1, 0x1
 a34:	77350420 	xvsrai.d    	\$xr0, \$xr1, 0x1
 a38:	77404420 	xvsrlni.b.h 	\$xr0, \$xr1, 0x1
 a3c:	77408420 	xvsrlni.h.w 	\$xr0, \$xr1, 0x1
 a40:	77410420 	xvsrlni.w.d 	\$xr0, \$xr1, 0x1
 a44:	77420420 	xvsrlni.d.q 	\$xr0, \$xr1, 0x1
 a48:	77444420 	xvsrlrni.b.h	\$xr0, \$xr1, 0x1
 a4c:	77448420 	xvsrlrni.h.w	\$xr0, \$xr1, 0x1
 a50:	77450420 	xvsrlrni.w.d	\$xr0, \$xr1, 0x1
 a54:	77460420 	xvsrlrni.d.q	\$xr0, \$xr1, 0x1
 a58:	77484420 	xvssrlni.b.h	\$xr0, \$xr1, 0x1
 a5c:	77488420 	xvssrlni.h.w	\$xr0, \$xr1, 0x1
 a60:	77490420 	xvssrlni.w.d	\$xr0, \$xr1, 0x1
 a64:	774a0420 	xvssrlni.d.q	\$xr0, \$xr1, 0x1
 a68:	774c4420 	xvssrlni.bu.h	\$xr0, \$xr1, 0x1
 a6c:	774c8420 	xvssrlni.hu.w	\$xr0, \$xr1, 0x1
 a70:	774d0420 	xvssrlni.wu.d	\$xr0, \$xr1, 0x1
 a74:	774e0420 	xvssrlni.du.q	\$xr0, \$xr1, 0x1
 a78:	77504420 	xvssrlrni.b.h	\$xr0, \$xr1, 0x1
 a7c:	77508420 	xvssrlrni.h.w	\$xr0, \$xr1, 0x1
 a80:	77510420 	xvssrlrni.w.d	\$xr0, \$xr1, 0x1
 a84:	77520420 	xvssrlrni.d.q	\$xr0, \$xr1, 0x1
 a88:	77544420 	xvssrlrni.bu.h	\$xr0, \$xr1, 0x1
 a8c:	77548420 	xvssrlrni.hu.w	\$xr0, \$xr1, 0x1
 a90:	77550420 	xvssrlrni.wu.d	\$xr0, \$xr1, 0x1
 a94:	77560420 	xvssrlrni.du.q	\$xr0, \$xr1, 0x1
 a98:	77584420 	xvsrani.b.h 	\$xr0, \$xr1, 0x1
 a9c:	77588420 	xvsrani.h.w 	\$xr0, \$xr1, 0x1
 aa0:	77590420 	xvsrani.w.d 	\$xr0, \$xr1, 0x1
 aa4:	775a0420 	xvsrani.d.q 	\$xr0, \$xr1, 0x1
 aa8:	775c4420 	xvsrarni.b.h	\$xr0, \$xr1, 0x1
 aac:	775c8420 	xvsrarni.h.w	\$xr0, \$xr1, 0x1
 ab0:	775d0420 	xvsrarni.w.d	\$xr0, \$xr1, 0x1
 ab4:	775e0420 	xvsrarni.d.q	\$xr0, \$xr1, 0x1
 ab8:	77604420 	xvssrani.b.h	\$xr0, \$xr1, 0x1
 abc:	77608420 	xvssrani.h.w	\$xr0, \$xr1, 0x1
 ac0:	77610420 	xvssrani.w.d	\$xr0, \$xr1, 0x1
 ac4:	77620420 	xvssrani.d.q	\$xr0, \$xr1, 0x1
 ac8:	77644420 	xvssrani.bu.h	\$xr0, \$xr1, 0x1
 acc:	77648420 	xvssrani.hu.w	\$xr0, \$xr1, 0x1
 ad0:	77650420 	xvssrani.wu.d	\$xr0, \$xr1, 0x1
 ad4:	77660420 	xvssrani.du.q	\$xr0, \$xr1, 0x1
 ad8:	77684420 	xvssrarni.b.h	\$xr0, \$xr1, 0x1
 adc:	77688420 	xvssrarni.h.w	\$xr0, \$xr1, 0x1
 ae0:	77690420 	xvssrarni.w.d	\$xr0, \$xr1, 0x1
 ae4:	776a0420 	xvssrarni.d.q	\$xr0, \$xr1, 0x1
 ae8:	776c4420 	xvssrarni.bu.h	\$xr0, \$xr1, 0x1
 aec:	776c8420 	xvssrarni.hu.w	\$xr0, \$xr1, 0x1
 af0:	776d0420 	xvssrarni.wu.d	\$xr0, \$xr1, 0x1
 af4:	776e0420 	xvssrarni.du.q	\$xr0, \$xr1, 0x1
 af8:	77800420 	xvextrins.d 	\$xr0, \$xr1, 0x1
 afc:	77840420 	xvextrins.w 	\$xr0, \$xr1, 0x1
 b00:	77880420 	xvextrins.h 	\$xr0, \$xr1, 0x1
 b04:	778c0420 	xvextrins.b 	\$xr0, \$xr1, 0x1
 b08:	77900420 	xvshuf4i.b  	\$xr0, \$xr1, 0x1
 b0c:	77940420 	xvshuf4i.h  	\$xr0, \$xr1, 0x1
 b10:	77980420 	xvshuf4i.w  	\$xr0, \$xr1, 0x1
 b14:	779c0420 	xvshuf4i.d  	\$xr0, \$xr1, 0x1
 b18:	77c40420 	xvbitseli.b 	\$xr0, \$xr1, 0x1
 b1c:	77d00420 	xvandi.b    	\$xr0, \$xr1, 0x1
 b20:	77d40420 	xvori.b     	\$xr0, \$xr1, 0x1
 b24:	77d80420 	xvxori.b    	\$xr0, \$xr1, 0x1
 b28:	77dc0420 	xvnori.b    	\$xr0, \$xr1, 0x1
 b2c:	77e00020 	xvldi       	\$xr0, 1
 b30:	77e18020 	xvldi       	\$xr0, 3073
 b34:	77e08020 	xvldi       	\$xr0, 1025
 b38:	77e10020 	xvldi       	\$xr0, 2049
 b3c:	77e00020 	xvldi       	\$xr0, 1
 b40:	77e40420 	xvpermi.w   	\$xr0, \$xr1, 0x1
 b44:	77e80420 	xvpermi.d   	\$xr0, \$xr1, 0x1
 b48:	77ec0420 	xvpermi.q   	\$xr0, \$xr1, 0x1
 b4c:	769d1420 	xvfrecipe.s 	\$xr0, \$xr1
 b50:	769d1820 	xvfrecipe.d 	\$xr0, \$xr1
 b54:	769d2420 	xvfrsqrte.s 	\$xr0, \$xr1
 b58:	769d2820 	xvfrsqrte.d 	\$xr0, \$xr1
