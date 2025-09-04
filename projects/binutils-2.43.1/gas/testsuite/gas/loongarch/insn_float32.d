#as-new:
#objdump: -d
#skip: loongarch64-*-*

.*:     file format .*


Disassembly of section .text:

0+ <.*>:
   0:	01008820 	fadd.s      	\$fa0, \$fa1, \$fa2
   4:	01010820 	fadd.d      	\$fa0, \$fa1, \$fa2
   8:	01028820 	fsub.s      	\$fa0, \$fa1, \$fa2
   c:	01030820 	fsub.d      	\$fa0, \$fa1, \$fa2
  10:	01048820 	fmul.s      	\$fa0, \$fa1, \$fa2
  14:	01050820 	fmul.d      	\$fa0, \$fa1, \$fa2
  18:	01068820 	fdiv.s      	\$fa0, \$fa1, \$fa2
  1c:	01070820 	fdiv.d      	\$fa0, \$fa1, \$fa2
  20:	01088820 	fmax.s      	\$fa0, \$fa1, \$fa2
  24:	01090820 	fmax.d      	\$fa0, \$fa1, \$fa2
  28:	010a8820 	fmin.s      	\$fa0, \$fa1, \$fa2
  2c:	010b0820 	fmin.d      	\$fa0, \$fa1, \$fa2
  30:	010c8820 	fmaxa.s     	\$fa0, \$fa1, \$fa2
  34:	010d0820 	fmaxa.d     	\$fa0, \$fa1, \$fa2
  38:	010e8820 	fmina.s     	\$fa0, \$fa1, \$fa2
  3c:	010f0820 	fmina.d     	\$fa0, \$fa1, \$fa2
  40:	01108820 	fscaleb.s   	\$fa0, \$fa1, \$fa2
  44:	01110820 	fscaleb.d   	\$fa0, \$fa1, \$fa2
  48:	01128820 	fcopysign.s 	\$fa0, \$fa1, \$fa2
  4c:	01130820 	fcopysign.d 	\$fa0, \$fa1, \$fa2
  50:	01140420 	fabs.s      	\$fa0, \$fa1
  54:	01140820 	fabs.d      	\$fa0, \$fa1
  58:	01141420 	fneg.s      	\$fa0, \$fa1
  5c:	01141820 	fneg.d      	\$fa0, \$fa1
  60:	01142420 	flogb.s     	\$fa0, \$fa1
  64:	01142820 	flogb.d     	\$fa0, \$fa1
  68:	01143420 	fclass.s    	\$fa0, \$fa1
  6c:	01143820 	fclass.d    	\$fa0, \$fa1
  70:	01144420 	fsqrt.s     	\$fa0, \$fa1
  74:	01144820 	fsqrt.d     	\$fa0, \$fa1
  78:	01145420 	frecip.s    	\$fa0, \$fa1
  7c:	01145820 	frecip.d    	\$fa0, \$fa1
  80:	01146420 	frsqrt.s    	\$fa0, \$fa1
  84:	01146820 	frsqrt.d    	\$fa0, \$fa1
  88:	01149420 	fmov.s      	\$fa0, \$fa1
  8c:	01149820 	fmov.d      	\$fa0, \$fa1
  90:	0114a4a0 	movgr2fr.w  	\$fa0, \$a1
  94:	0114a8a0 	movgr2fr.d  	\$fa0, \$a1
  98:	0114aca0 	movgr2frh.w 	\$fa0, \$a1
  9c:	0114b424 	movfr2gr.s  	\$a0, \$fa1
  a0:	0114b824 	movfr2gr.d  	\$a0, \$fa1
  a4:	0114bc24 	movfrh2gr.s 	\$a0, \$fa1
  a8:	0114c0a0 	movgr2fcsr  	\$fcsr0, \$a1
  ac:	0114c804 	movfcsr2gr  	\$a0, \$fcsr0
  b0:	0114d020 	movfr2cf    	\$fcc0, \$fa1
  b4:	0114d4a0 	movcf2fr    	\$fa0, \$fcc5
  b8:	0114d8a0 	movgr2cf    	\$fcc0, \$a1
  bc:	0114dca4 	movcf2gr    	\$a0, \$fcc5
  c0:	01191820 	fcvt.s.d    	\$fa0, \$fa1
  c4:	01192420 	fcvt.d.s    	\$fa0, \$fa1
  c8:	011a0420 	ftintrm.w.s 	\$fa0, \$fa1
  cc:	011a0820 	ftintrm.w.d 	\$fa0, \$fa1
  d0:	011a2420 	ftintrm.l.s 	\$fa0, \$fa1
  d4:	011a2820 	ftintrm.l.d 	\$fa0, \$fa1
  d8:	011a4420 	ftintrp.w.s 	\$fa0, \$fa1
  dc:	011a4820 	ftintrp.w.d 	\$fa0, \$fa1
  e0:	011a6420 	ftintrp.l.s 	\$fa0, \$fa1
  e4:	011a6820 	ftintrp.l.d 	\$fa0, \$fa1
  e8:	011a8420 	ftintrz.w.s 	\$fa0, \$fa1
  ec:	011a8820 	ftintrz.w.d 	\$fa0, \$fa1
  f0:	011aa420 	ftintrz.l.s 	\$fa0, \$fa1
  f4:	011aa820 	ftintrz.l.d 	\$fa0, \$fa1
  f8:	011ac420 	ftintrne.w.s	\$fa0, \$fa1
  fc:	011ac820 	ftintrne.w.d	\$fa0, \$fa1
 100:	011ae420 	ftintrne.l.s	\$fa0, \$fa1
 104:	011ae820 	ftintrne.l.d	\$fa0, \$fa1
 108:	011b0420 	ftint.w.s   	\$fa0, \$fa1
 10c:	011b0820 	ftint.w.d   	\$fa0, \$fa1
 110:	011b2420 	ftint.l.s   	\$fa0, \$fa1
 114:	011b2820 	ftint.l.d   	\$fa0, \$fa1
 118:	011d1020 	ffint.s.w   	\$fa0, \$fa1
 11c:	011d1820 	ffint.s.l   	\$fa0, \$fa1
 120:	011d2020 	ffint.d.w   	\$fa0, \$fa1
 124:	011d2820 	ffint.d.l   	\$fa0, \$fa1
 128:	011e4420 	frint.s     	\$fa0, \$fa1
 12c:	011e4820 	frint.d     	\$fa0, \$fa1
 130:	01147420 	frecipe.s   	\$fa0, \$fa1
 134:	01147820 	frecipe.d   	\$fa0, \$fa1
 138:	01148420 	frsqrte.s   	\$fa0, \$fa1
 13c:	01148820 	frsqrte.d   	\$fa0, \$fa1
 140:	08118820 	fmadd.s     	\$fa0, \$fa1, \$fa2, \$fa3
 144:	08218820 	fmadd.d     	\$fa0, \$fa1, \$fa2, \$fa3
 148:	08518820 	fmsub.s     	\$fa0, \$fa1, \$fa2, \$fa3
 14c:	08618820 	fmsub.d     	\$fa0, \$fa1, \$fa2, \$fa3
 150:	08918820 	fnmadd.s    	\$fa0, \$fa1, \$fa2, \$fa3
 154:	08a18820 	fnmadd.d    	\$fa0, \$fa1, \$fa2, \$fa3
 158:	08d18820 	fnmsub.s    	\$fa0, \$fa1, \$fa2, \$fa3
 15c:	08e18820 	fnmsub.d    	\$fa0, \$fa1, \$fa2, \$fa3
 160:	0c100820 	fcmp.caf.s  	\$fcc0, \$fa1, \$fa2
 164:	0c108820 	fcmp.saf.s  	\$fcc0, \$fa1, \$fa2
 168:	0c110820 	fcmp.clt.s  	\$fcc0, \$fa1, \$fa2
 16c:	0c118820 	fcmp.slt.s  	\$fcc0, \$fa1, \$fa2
 170:	0c118820 	fcmp.slt.s  	\$fcc0, \$fa1, \$fa2
 174:	0c120820 	fcmp.ceq.s  	\$fcc0, \$fa1, \$fa2
 178:	0c128820 	fcmp.seq.s  	\$fcc0, \$fa1, \$fa2
 17c:	0c130820 	fcmp.cle.s  	\$fcc0, \$fa1, \$fa2
 180:	0c138820 	fcmp.sle.s  	\$fcc0, \$fa1, \$fa2
 184:	0c138820 	fcmp.sle.s  	\$fcc0, \$fa1, \$fa2
 188:	0c140820 	fcmp.cun.s  	\$fcc0, \$fa1, \$fa2
 18c:	0c148820 	fcmp.sun.s  	\$fcc0, \$fa1, \$fa2
 190:	0c150820 	fcmp.cult.s 	\$fcc0, \$fa1, \$fa2
 194:	0c150820 	fcmp.cult.s 	\$fcc0, \$fa1, \$fa2
 198:	0c158820 	fcmp.sult.s 	\$fcc0, \$fa1, \$fa2
 19c:	0c160820 	fcmp.cueq.s 	\$fcc0, \$fa1, \$fa2
 1a0:	0c168820 	fcmp.sueq.s 	\$fcc0, \$fa1, \$fa2
 1a4:	0c170820 	fcmp.cule.s 	\$fcc0, \$fa1, \$fa2
 1a8:	0c170820 	fcmp.cule.s 	\$fcc0, \$fa1, \$fa2
 1ac:	0c178820 	fcmp.sule.s 	\$fcc0, \$fa1, \$fa2
 1b0:	0c180820 	fcmp.cne.s  	\$fcc0, \$fa1, \$fa2
 1b4:	0c188820 	fcmp.sne.s  	\$fcc0, \$fa1, \$fa2
 1b8:	0c1a0820 	fcmp.cor.s  	\$fcc0, \$fa1, \$fa2
 1bc:	0c1a8820 	fcmp.sor.s  	\$fcc0, \$fa1, \$fa2
 1c0:	0c1c0820 	fcmp.cune.s 	\$fcc0, \$fa1, \$fa2
 1c4:	0c1c8820 	fcmp.sune.s 	\$fcc0, \$fa1, \$fa2
 1c8:	0c200820 	fcmp.caf.d  	\$fcc0, \$fa1, \$fa2
 1cc:	0c208820 	fcmp.saf.d  	\$fcc0, \$fa1, \$fa2
 1d0:	0c210820 	fcmp.clt.d  	\$fcc0, \$fa1, \$fa2
 1d4:	0c218820 	fcmp.slt.d  	\$fcc0, \$fa1, \$fa2
 1d8:	0c218820 	fcmp.slt.d  	\$fcc0, \$fa1, \$fa2
 1dc:	0c220820 	fcmp.ceq.d  	\$fcc0, \$fa1, \$fa2
 1e0:	0c228820 	fcmp.seq.d  	\$fcc0, \$fa1, \$fa2
 1e4:	0c230820 	fcmp.cle.d  	\$fcc0, \$fa1, \$fa2
 1e8:	0c238820 	fcmp.sle.d  	\$fcc0, \$fa1, \$fa2
 1ec:	0c238820 	fcmp.sle.d  	\$fcc0, \$fa1, \$fa2
 1f0:	0c240820 	fcmp.cun.d  	\$fcc0, \$fa1, \$fa2
 1f4:	0c248820 	fcmp.sun.d  	\$fcc0, \$fa1, \$fa2
 1f8:	0c250820 	fcmp.cult.d 	\$fcc0, \$fa1, \$fa2
 1fc:	0c250820 	fcmp.cult.d 	\$fcc0, \$fa1, \$fa2
 200:	0c258820 	fcmp.sult.d 	\$fcc0, \$fa1, \$fa2
 204:	0c260820 	fcmp.cueq.d 	\$fcc0, \$fa1, \$fa2
 208:	0c268820 	fcmp.sueq.d 	\$fcc0, \$fa1, \$fa2
 20c:	0c270820 	fcmp.cule.d 	\$fcc0, \$fa1, \$fa2
 210:	0c270820 	fcmp.cule.d 	\$fcc0, \$fa1, \$fa2
 214:	0c278820 	fcmp.sule.d 	\$fcc0, \$fa1, \$fa2
 218:	0c280820 	fcmp.cne.d  	\$fcc0, \$fa1, \$fa2
 21c:	0c288820 	fcmp.sne.d  	\$fcc0, \$fa1, \$fa2
 220:	0c2a0820 	fcmp.cor.d  	\$fcc0, \$fa1, \$fa2
 224:	0c2a8820 	fcmp.sor.d  	\$fcc0, \$fa1, \$fa2
 228:	0c2c0820 	fcmp.cune.d 	\$fcc0, \$fa1, \$fa2
 22c:	0c2c8820 	fcmp.sune.d 	\$fcc0, \$fa1, \$fa2
 230:	0d000820 	fsel        	\$fa0, \$fa1, \$fa2, \$fcc0
 234:	2b00058a 	fld.s       	\$ft2, \$t0, 1
 238:	2b40058a 	fst.s       	\$ft2, \$t0, 1
 23c:	2b80058a 	fld.d       	\$ft2, \$t0, 1
 240:	2bc0058a 	fst.d       	\$ft2, \$t0, 1
 244:	48000000 	bceqz       	\$fcc0, 0	# 0x244
 248:	48000100 	bcnez       	\$fcc0, 0	# 0x248
