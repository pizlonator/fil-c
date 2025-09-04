#as:
#objdump: -d
#skip: loongarch32-*-*

.*:     file format .*


Disassembly of section .text:

0+ <.*>:
   0:	09118820 	vfmadd.s    	\$vr0, \$vr1, \$vr2, \$vr3
   4:	09518820 	vfmsub.s    	\$vr0, \$vr1, \$vr2, \$vr3
   8:	09918820 	vfnmadd.s   	\$vr0, \$vr1, \$vr2, \$vr3
   c:	09d18820 	vfnmsub.s   	\$vr0, \$vr1, \$vr2, \$vr3
  10:	0c500820 	vfcmp.caf.s 	\$vr0, \$vr1, \$vr2
  14:	0c508820 	vfcmp.saf.s 	\$vr0, \$vr1, \$vr2
  18:	0c510820 	vfcmp.clt.s 	\$vr0, \$vr1, \$vr2
  1c:	0c518820 	vfcmp.slt.s 	\$vr0, \$vr1, \$vr2
  20:	0c520820 	vfcmp.ceq.s 	\$vr0, \$vr1, \$vr2
  24:	0c528820 	vfcmp.seq.s 	\$vr0, \$vr1, \$vr2
  28:	0c530820 	vfcmp.cle.s 	\$vr0, \$vr1, \$vr2
  2c:	0c538820 	vfcmp.sle.s 	\$vr0, \$vr1, \$vr2
  30:	0c540820 	vfcmp.cun.s 	\$vr0, \$vr1, \$vr2
  34:	0c548820 	vfcmp.sun.s 	\$vr0, \$vr1, \$vr2
  38:	0c550820 	vfcmp.cult.s	\$vr0, \$vr1, \$vr2
  3c:	0c558820 	vfcmp.sult.s	\$vr0, \$vr1, \$vr2
  40:	0c560820 	vfcmp.cueq.s	\$vr0, \$vr1, \$vr2
  44:	0c568820 	vfcmp.sueq.s	\$vr0, \$vr1, \$vr2
  48:	0c570820 	vfcmp.cule.s	\$vr0, \$vr1, \$vr2
  4c:	0c578820 	vfcmp.sule.s	\$vr0, \$vr1, \$vr2
  50:	0c580820 	vfcmp.cne.s 	\$vr0, \$vr1, \$vr2
  54:	0c588820 	vfcmp.sne.s 	\$vr0, \$vr1, \$vr2
  58:	0c5a0820 	vfcmp.cor.s 	\$vr0, \$vr1, \$vr2
  5c:	0c5a8820 	vfcmp.sor.s 	\$vr0, \$vr1, \$vr2
  60:	0c5c0820 	vfcmp.cune.s	\$vr0, \$vr1, \$vr2
  64:	0c5c8820 	vfcmp.sune.s	\$vr0, \$vr1, \$vr2
  68:	0d118820 	vbitsel.v   	\$vr0, \$vr1, \$vr2, \$vr3
  6c:	0d518820 	vshuf.b     	\$vr0, \$vr1, \$vr2, \$vr3
  70:	09218820 	vfmadd.d    	\$vr0, \$vr1, \$vr2, \$vr3
  74:	09618820 	vfmsub.d    	\$vr0, \$vr1, \$vr2, \$vr3
  78:	09a18820 	vfnmadd.d   	\$vr0, \$vr1, \$vr2, \$vr3
  7c:	09e18820 	vfnmsub.d   	\$vr0, \$vr1, \$vr2, \$vr3
  80:	0c600820 	vfcmp.caf.d 	\$vr0, \$vr1, \$vr2
  84:	0c608820 	vfcmp.saf.d 	\$vr0, \$vr1, \$vr2
  88:	0c610820 	vfcmp.clt.d 	\$vr0, \$vr1, \$vr2
  8c:	0c618820 	vfcmp.slt.d 	\$vr0, \$vr1, \$vr2
  90:	0c620820 	vfcmp.ceq.d 	\$vr0, \$vr1, \$vr2
  94:	0c628820 	vfcmp.seq.d 	\$vr0, \$vr1, \$vr2
  98:	0c630820 	vfcmp.cle.d 	\$vr0, \$vr1, \$vr2
  9c:	0c638820 	vfcmp.sle.d 	\$vr0, \$vr1, \$vr2
  a0:	0c640820 	vfcmp.cun.d 	\$vr0, \$vr1, \$vr2
  a4:	0c648820 	vfcmp.sun.d 	\$vr0, \$vr1, \$vr2
  a8:	0c650820 	vfcmp.cult.d	\$vr0, \$vr1, \$vr2
  ac:	0c658820 	vfcmp.sult.d	\$vr0, \$vr1, \$vr2
  b0:	0c660820 	vfcmp.cueq.d	\$vr0, \$vr1, \$vr2
  b4:	0c668820 	vfcmp.sueq.d	\$vr0, \$vr1, \$vr2
  b8:	0c670820 	vfcmp.cule.d	\$vr0, \$vr1, \$vr2
  bc:	0c678820 	vfcmp.sule.d	\$vr0, \$vr1, \$vr2
  c0:	0c680820 	vfcmp.cne.d 	\$vr0, \$vr1, \$vr2
  c4:	0c688820 	vfcmp.sne.d 	\$vr0, \$vr1, \$vr2
  c8:	0c6a0820 	vfcmp.cor.d 	\$vr0, \$vr1, \$vr2
  cc:	0c6a8820 	vfcmp.sor.d 	\$vr0, \$vr1, \$vr2
  d0:	0c6c0820 	vfcmp.cune.d	\$vr0, \$vr1, \$vr2
  d4:	0c6c8820 	vfcmp.sune.d	\$vr0, \$vr1, \$vr2
  d8:	2c000420 	vld         	\$vr0, \$ra, 1
  dc:	2c400420 	vst         	\$vr0, \$ra, 1
  e0:	38400820 	vldx        	\$vr0, \$ra, \$tp
  e4:	38440820 	vstx        	\$vr0, \$ra, \$tp
  e8:	3011f420 	vldrepl.d   	\$vr0, \$ra, 1000
  ec:	30206420 	vldrepl.w   	\$vr0, \$ra, 100
  f0:	30401420 	vldrepl.h   	\$vr0, \$ra, 10
  f4:	30800420 	vldrepl.b   	\$vr0, \$ra, 1
  f8:	3115f420 	vstelm.d    	\$vr0, \$ra, 1000, 0x1
  fc:	31246420 	vstelm.w    	\$vr0, \$ra, 100, 0x1
 100:	31441420 	vstelm.h    	\$vr0, \$ra, 10, 0x1
 104:	31840420 	vstelm.b    	\$vr0, \$ra, 1, 0x1
 108:	70000820 	vseq.b      	\$vr0, \$vr1, \$vr2
 10c:	70008820 	vseq.h      	\$vr0, \$vr1, \$vr2
 110:	70010820 	vseq.w      	\$vr0, \$vr1, \$vr2
 114:	70018820 	vseq.d      	\$vr0, \$vr1, \$vr2
 118:	70020820 	vsle.b      	\$vr0, \$vr1, \$vr2
 11c:	70028820 	vsle.h      	\$vr0, \$vr1, \$vr2
 120:	70030820 	vsle.w      	\$vr0, \$vr1, \$vr2
 124:	70038820 	vsle.d      	\$vr0, \$vr1, \$vr2
 128:	70040820 	vsle.bu     	\$vr0, \$vr1, \$vr2
 12c:	70048820 	vsle.hu     	\$vr0, \$vr1, \$vr2
 130:	70050820 	vsle.wu     	\$vr0, \$vr1, \$vr2
 134:	70058820 	vsle.du     	\$vr0, \$vr1, \$vr2
 138:	70060820 	vslt.b      	\$vr0, \$vr1, \$vr2
 13c:	70068820 	vslt.h      	\$vr0, \$vr1, \$vr2
 140:	70070820 	vslt.w      	\$vr0, \$vr1, \$vr2
 144:	70078820 	vslt.d      	\$vr0, \$vr1, \$vr2
 148:	70080820 	vslt.bu     	\$vr0, \$vr1, \$vr2
 14c:	70088820 	vslt.hu     	\$vr0, \$vr1, \$vr2
 150:	70090820 	vslt.wu     	\$vr0, \$vr1, \$vr2
 154:	70098820 	vslt.du     	\$vr0, \$vr1, \$vr2
 158:	700a0820 	vadd.b      	\$vr0, \$vr1, \$vr2
 15c:	700a8820 	vadd.h      	\$vr0, \$vr1, \$vr2
 160:	700b0820 	vadd.w      	\$vr0, \$vr1, \$vr2
 164:	700b8820 	vadd.d      	\$vr0, \$vr1, \$vr2
 168:	700c0820 	vsub.b      	\$vr0, \$vr1, \$vr2
 16c:	700c8820 	vsub.h      	\$vr0, \$vr1, \$vr2
 170:	700d0820 	vsub.w      	\$vr0, \$vr1, \$vr2
 174:	700d8820 	vsub.d      	\$vr0, \$vr1, \$vr2
 178:	70460820 	vsadd.b     	\$vr0, \$vr1, \$vr2
 17c:	70468820 	vsadd.h     	\$vr0, \$vr1, \$vr2
 180:	70470820 	vsadd.w     	\$vr0, \$vr1, \$vr2
 184:	70478820 	vsadd.d     	\$vr0, \$vr1, \$vr2
 188:	70480820 	vssub.b     	\$vr0, \$vr1, \$vr2
 18c:	70488820 	vssub.h     	\$vr0, \$vr1, \$vr2
 190:	70490820 	vssub.w     	\$vr0, \$vr1, \$vr2
 194:	70498820 	vssub.d     	\$vr0, \$vr1, \$vr2
 198:	704a0820 	vsadd.bu    	\$vr0, \$vr1, \$vr2
 19c:	704a8820 	vsadd.hu    	\$vr0, \$vr1, \$vr2
 1a0:	704b0820 	vsadd.wu    	\$vr0, \$vr1, \$vr2
 1a4:	704b8820 	vsadd.du    	\$vr0, \$vr1, \$vr2
 1a8:	704c0820 	vssub.bu    	\$vr0, \$vr1, \$vr2
 1ac:	704c8820 	vssub.hu    	\$vr0, \$vr1, \$vr2
 1b0:	704d0820 	vssub.wu    	\$vr0, \$vr1, \$vr2
 1b4:	704d8820 	vssub.du    	\$vr0, \$vr1, \$vr2
 1b8:	70540820 	vhaddw.h.b  	\$vr0, \$vr1, \$vr2
 1bc:	70548820 	vhaddw.w.h  	\$vr0, \$vr1, \$vr2
 1c0:	70550820 	vhaddw.d.w  	\$vr0, \$vr1, \$vr2
 1c4:	70558820 	vhaddw.q.d  	\$vr0, \$vr1, \$vr2
 1c8:	70560820 	vhsubw.h.b  	\$vr0, \$vr1, \$vr2
 1cc:	70568820 	vhsubw.w.h  	\$vr0, \$vr1, \$vr2
 1d0:	70570820 	vhsubw.d.w  	\$vr0, \$vr1, \$vr2
 1d4:	70578820 	vhsubw.q.d  	\$vr0, \$vr1, \$vr2
 1d8:	70580820 	vhaddw.hu.bu	\$vr0, \$vr1, \$vr2
 1dc:	70588820 	vhaddw.wu.hu	\$vr0, \$vr1, \$vr2
 1e0:	70590820 	vhaddw.du.wu	\$vr0, \$vr1, \$vr2
 1e4:	70598820 	vhaddw.qu.du	\$vr0, \$vr1, \$vr2
 1e8:	705a0820 	vhsubw.hu.bu	\$vr0, \$vr1, \$vr2
 1ec:	705a8820 	vhsubw.wu.hu	\$vr0, \$vr1, \$vr2
 1f0:	705b0820 	vhsubw.du.wu	\$vr0, \$vr1, \$vr2
 1f4:	705b8820 	vhsubw.qu.du	\$vr0, \$vr1, \$vr2
 1f8:	705c0820 	vadda.b     	\$vr0, \$vr1, \$vr2
 1fc:	705c8820 	vadda.h     	\$vr0, \$vr1, \$vr2
 200:	705d0820 	vadda.w     	\$vr0, \$vr1, \$vr2
 204:	705d8820 	vadda.d     	\$vr0, \$vr1, \$vr2
 208:	70600820 	vabsd.b     	\$vr0, \$vr1, \$vr2
 20c:	70608820 	vabsd.h     	\$vr0, \$vr1, \$vr2
 210:	70610820 	vabsd.w     	\$vr0, \$vr1, \$vr2
 214:	70618820 	vabsd.d     	\$vr0, \$vr1, \$vr2
 218:	70620820 	vabsd.bu    	\$vr0, \$vr1, \$vr2
 21c:	70628820 	vabsd.hu    	\$vr0, \$vr1, \$vr2
 220:	70630820 	vabsd.wu    	\$vr0, \$vr1, \$vr2
 224:	70638820 	vabsd.du    	\$vr0, \$vr1, \$vr2
 228:	70640820 	vavg.b      	\$vr0, \$vr1, \$vr2
 22c:	70648820 	vavg.h      	\$vr0, \$vr1, \$vr2
 230:	70650820 	vavg.w      	\$vr0, \$vr1, \$vr2
 234:	70658820 	vavg.d      	\$vr0, \$vr1, \$vr2
 238:	70660820 	vavg.bu     	\$vr0, \$vr1, \$vr2
 23c:	70668820 	vavg.hu     	\$vr0, \$vr1, \$vr2
 240:	70670820 	vavg.wu     	\$vr0, \$vr1, \$vr2
 244:	70678820 	vavg.du     	\$vr0, \$vr1, \$vr2
 248:	70680820 	vavgr.b     	\$vr0, \$vr1, \$vr2
 24c:	70688820 	vavgr.h     	\$vr0, \$vr1, \$vr2
 250:	70690820 	vavgr.w     	\$vr0, \$vr1, \$vr2
 254:	70698820 	vavgr.d     	\$vr0, \$vr1, \$vr2
 258:	706a0820 	vavgr.bu    	\$vr0, \$vr1, \$vr2
 25c:	706a8820 	vavgr.hu    	\$vr0, \$vr1, \$vr2
 260:	706b0820 	vavgr.wu    	\$vr0, \$vr1, \$vr2
 264:	706b8820 	vavgr.du    	\$vr0, \$vr1, \$vr2
 268:	70700820 	vmax.b      	\$vr0, \$vr1, \$vr2
 26c:	70708820 	vmax.h      	\$vr0, \$vr1, \$vr2
 270:	70710820 	vmax.w      	\$vr0, \$vr1, \$vr2
 274:	70718820 	vmax.d      	\$vr0, \$vr1, \$vr2
 278:	70720820 	vmin.b      	\$vr0, \$vr1, \$vr2
 27c:	70728820 	vmin.h      	\$vr0, \$vr1, \$vr2
 280:	70730820 	vmin.w      	\$vr0, \$vr1, \$vr2
 284:	70738820 	vmin.d      	\$vr0, \$vr1, \$vr2
 288:	70740820 	vmax.bu     	\$vr0, \$vr1, \$vr2
 28c:	70748820 	vmax.hu     	\$vr0, \$vr1, \$vr2
 290:	70750820 	vmax.wu     	\$vr0, \$vr1, \$vr2
 294:	70758820 	vmax.du     	\$vr0, \$vr1, \$vr2
 298:	70760820 	vmin.bu     	\$vr0, \$vr1, \$vr2
 29c:	70768820 	vmin.hu     	\$vr0, \$vr1, \$vr2
 2a0:	70770820 	vmin.wu     	\$vr0, \$vr1, \$vr2
 2a4:	70778820 	vmin.du     	\$vr0, \$vr1, \$vr2
 2a8:	70840820 	vmul.b      	\$vr0, \$vr1, \$vr2
 2ac:	70848820 	vmul.h      	\$vr0, \$vr1, \$vr2
 2b0:	70850820 	vmul.w      	\$vr0, \$vr1, \$vr2
 2b4:	70858820 	vmul.d      	\$vr0, \$vr1, \$vr2
 2b8:	70860820 	vmuh.b      	\$vr0, \$vr1, \$vr2
 2bc:	70868820 	vmuh.h      	\$vr0, \$vr1, \$vr2
 2c0:	70870820 	vmuh.w      	\$vr0, \$vr1, \$vr2
 2c4:	70878820 	vmuh.d      	\$vr0, \$vr1, \$vr2
 2c8:	70880820 	vmuh.bu     	\$vr0, \$vr1, \$vr2
 2cc:	70888820 	vmuh.hu     	\$vr0, \$vr1, \$vr2
 2d0:	70890820 	vmuh.wu     	\$vr0, \$vr1, \$vr2
 2d4:	70898820 	vmuh.du     	\$vr0, \$vr1, \$vr2
 2d8:	70a80820 	vmadd.b     	\$vr0, \$vr1, \$vr2
 2dc:	70a88820 	vmadd.h     	\$vr0, \$vr1, \$vr2
 2e0:	70a90820 	vmadd.w     	\$vr0, \$vr1, \$vr2
 2e4:	70a98820 	vmadd.d     	\$vr0, \$vr1, \$vr2
 2e8:	70aa0820 	vmsub.b     	\$vr0, \$vr1, \$vr2
 2ec:	70aa8820 	vmsub.h     	\$vr0, \$vr1, \$vr2
 2f0:	70ab0820 	vmsub.w     	\$vr0, \$vr1, \$vr2
 2f4:	70ab8820 	vmsub.d     	\$vr0, \$vr1, \$vr2
 2f8:	70e00820 	vdiv.b      	\$vr0, \$vr1, \$vr2
 2fc:	70e08820 	vdiv.h      	\$vr0, \$vr1, \$vr2
 300:	70e10820 	vdiv.w      	\$vr0, \$vr1, \$vr2
 304:	70e18820 	vdiv.d      	\$vr0, \$vr1, \$vr2
 308:	70e20820 	vmod.b      	\$vr0, \$vr1, \$vr2
 30c:	70e28820 	vmod.h      	\$vr0, \$vr1, \$vr2
 310:	70e30820 	vmod.w      	\$vr0, \$vr1, \$vr2
 314:	70e38820 	vmod.d      	\$vr0, \$vr1, \$vr2
 318:	70e40820 	vdiv.bu     	\$vr0, \$vr1, \$vr2
 31c:	70e48820 	vdiv.hu     	\$vr0, \$vr1, \$vr2
 320:	70e50820 	vdiv.wu     	\$vr0, \$vr1, \$vr2
 324:	70e58820 	vdiv.du     	\$vr0, \$vr1, \$vr2
 328:	70e60820 	vmod.bu     	\$vr0, \$vr1, \$vr2
 32c:	70e68820 	vmod.hu     	\$vr0, \$vr1, \$vr2
 330:	70e70820 	vmod.wu     	\$vr0, \$vr1, \$vr2
 334:	70e78820 	vmod.du     	\$vr0, \$vr1, \$vr2
 338:	70e80820 	vsll.b      	\$vr0, \$vr1, \$vr2
 33c:	70e88820 	vsll.h      	\$vr0, \$vr1, \$vr2
 340:	70e90820 	vsll.w      	\$vr0, \$vr1, \$vr2
 344:	70e98820 	vsll.d      	\$vr0, \$vr1, \$vr2
 348:	70ea0820 	vsrl.b      	\$vr0, \$vr1, \$vr2
 34c:	70ea8820 	vsrl.h      	\$vr0, \$vr1, \$vr2
 350:	70eb0820 	vsrl.w      	\$vr0, \$vr1, \$vr2
 354:	70eb8820 	vsrl.d      	\$vr0, \$vr1, \$vr2
 358:	70ec0820 	vsra.b      	\$vr0, \$vr1, \$vr2
 35c:	70ec8820 	vsra.h      	\$vr0, \$vr1, \$vr2
 360:	70ed0820 	vsra.w      	\$vr0, \$vr1, \$vr2
 364:	70ed8820 	vsra.d      	\$vr0, \$vr1, \$vr2
 368:	70ee0820 	vrotr.b     	\$vr0, \$vr1, \$vr2
 36c:	70ee8820 	vrotr.h     	\$vr0, \$vr1, \$vr2
 370:	70ef0820 	vrotr.w     	\$vr0, \$vr1, \$vr2
 374:	70ef8820 	vrotr.d     	\$vr0, \$vr1, \$vr2
 378:	70f00820 	vsrlr.b     	\$vr0, \$vr1, \$vr2
 37c:	70f08820 	vsrlr.h     	\$vr0, \$vr1, \$vr2
 380:	70f10820 	vsrlr.w     	\$vr0, \$vr1, \$vr2
 384:	70f18820 	vsrlr.d     	\$vr0, \$vr1, \$vr2
 388:	70f20820 	vsrar.b     	\$vr0, \$vr1, \$vr2
 38c:	70f28820 	vsrar.h     	\$vr0, \$vr1, \$vr2
 390:	70f30820 	vsrar.w     	\$vr0, \$vr1, \$vr2
 394:	70f38820 	vsrar.d     	\$vr0, \$vr1, \$vr2
 398:	70f48820 	vsrln.b.h   	\$vr0, \$vr1, \$vr2
 39c:	70f50820 	vsrln.h.w   	\$vr0, \$vr1, \$vr2
 3a0:	70f58820 	vsrln.w.d   	\$vr0, \$vr1, \$vr2
 3a4:	70f68820 	vsran.b.h   	\$vr0, \$vr1, \$vr2
 3a8:	70f70820 	vsran.h.w   	\$vr0, \$vr1, \$vr2
 3ac:	70f78820 	vsran.w.d   	\$vr0, \$vr1, \$vr2
 3b0:	70f88820 	vsrlrn.b.h  	\$vr0, \$vr1, \$vr2
 3b4:	70f90820 	vsrlrn.h.w  	\$vr0, \$vr1, \$vr2
 3b8:	70f98820 	vsrlrn.w.d  	\$vr0, \$vr1, \$vr2
 3bc:	70fa8820 	vsrarn.b.h  	\$vr0, \$vr1, \$vr2
 3c0:	70fb0820 	vsrarn.h.w  	\$vr0, \$vr1, \$vr2
 3c4:	70fb8820 	vsrarn.w.d  	\$vr0, \$vr1, \$vr2
 3c8:	70fc8820 	vssrln.b.h  	\$vr0, \$vr1, \$vr2
 3cc:	70fd0820 	vssrln.h.w  	\$vr0, \$vr1, \$vr2
 3d0:	70fd8820 	vssrln.w.d  	\$vr0, \$vr1, \$vr2
 3d4:	70fe8820 	vssran.b.h  	\$vr0, \$vr1, \$vr2
 3d8:	70ff0820 	vssran.h.w  	\$vr0, \$vr1, \$vr2
 3dc:	70ff8820 	vssran.w.d  	\$vr0, \$vr1, \$vr2
 3e0:	71008820 	vssrlrn.b.h 	\$vr0, \$vr1, \$vr2
 3e4:	71010820 	vssrlrn.h.w 	\$vr0, \$vr1, \$vr2
 3e8:	71018820 	vssrlrn.w.d 	\$vr0, \$vr1, \$vr2
 3ec:	71028820 	vssrarn.b.h 	\$vr0, \$vr1, \$vr2
 3f0:	71030820 	vssrarn.h.w 	\$vr0, \$vr1, \$vr2
 3f4:	71038820 	vssrarn.w.d 	\$vr0, \$vr1, \$vr2
 3f8:	71048820 	vssrln.bu.h 	\$vr0, \$vr1, \$vr2
 3fc:	71050820 	vssrln.hu.w 	\$vr0, \$vr1, \$vr2
 400:	71058820 	vssrln.wu.d 	\$vr0, \$vr1, \$vr2
 404:	71068820 	vssran.bu.h 	\$vr0, \$vr1, \$vr2
 408:	71070820 	vssran.hu.w 	\$vr0, \$vr1, \$vr2
 40c:	71078820 	vssran.wu.d 	\$vr0, \$vr1, \$vr2
 410:	71088820 	vssrlrn.bu.h	\$vr0, \$vr1, \$vr2
 414:	71090820 	vssrlrn.hu.w	\$vr0, \$vr1, \$vr2
 418:	71098820 	vssrlrn.wu.d	\$vr0, \$vr1, \$vr2
 41c:	710a8820 	vssrarn.bu.h	\$vr0, \$vr1, \$vr2
 420:	710b0820 	vssrarn.hu.w	\$vr0, \$vr1, \$vr2
 424:	710b8820 	vssrarn.wu.d	\$vr0, \$vr1, \$vr2
 428:	710c0820 	vbitclr.b   	\$vr0, \$vr1, \$vr2
 42c:	710c8820 	vbitclr.h   	\$vr0, \$vr1, \$vr2
 430:	710d0820 	vbitclr.w   	\$vr0, \$vr1, \$vr2
 434:	710d8820 	vbitclr.d   	\$vr0, \$vr1, \$vr2
 438:	710e0820 	vbitset.b   	\$vr0, \$vr1, \$vr2
 43c:	710e8820 	vbitset.h   	\$vr0, \$vr1, \$vr2
 440:	710f0820 	vbitset.w   	\$vr0, \$vr1, \$vr2
 444:	710f8820 	vbitset.d   	\$vr0, \$vr1, \$vr2
 448:	71100820 	vbitrev.b   	\$vr0, \$vr1, \$vr2
 44c:	71108820 	vbitrev.h   	\$vr0, \$vr1, \$vr2
 450:	71110820 	vbitrev.w   	\$vr0, \$vr1, \$vr2
 454:	71118820 	vbitrev.d   	\$vr0, \$vr1, \$vr2
 458:	71160820 	vpackev.b   	\$vr0, \$vr1, \$vr2
 45c:	71168820 	vpackev.h   	\$vr0, \$vr1, \$vr2
 460:	71170820 	vpackev.w   	\$vr0, \$vr1, \$vr2
 464:	71178820 	vpackev.d   	\$vr0, \$vr1, \$vr2
 468:	71180820 	vpackod.b   	\$vr0, \$vr1, \$vr2
 46c:	71188820 	vpackod.h   	\$vr0, \$vr1, \$vr2
 470:	71190820 	vpackod.w   	\$vr0, \$vr1, \$vr2
 474:	71198820 	vpackod.d   	\$vr0, \$vr1, \$vr2
 478:	711a0820 	vilvl.b     	\$vr0, \$vr1, \$vr2
 47c:	711a8820 	vilvl.h     	\$vr0, \$vr1, \$vr2
 480:	711b0820 	vilvl.w     	\$vr0, \$vr1, \$vr2
 484:	711b8820 	vilvl.d     	\$vr0, \$vr1, \$vr2
 488:	711c0820 	vilvh.b     	\$vr0, \$vr1, \$vr2
 48c:	711c8820 	vilvh.h     	\$vr0, \$vr1, \$vr2
 490:	711d0820 	vilvh.w     	\$vr0, \$vr1, \$vr2
 494:	711d8820 	vilvh.d     	\$vr0, \$vr1, \$vr2
 498:	711e0820 	vpickev.b   	\$vr0, \$vr1, \$vr2
 49c:	711e8820 	vpickev.h   	\$vr0, \$vr1, \$vr2
 4a0:	711f0820 	vpickev.w   	\$vr0, \$vr1, \$vr2
 4a4:	711f8820 	vpickev.d   	\$vr0, \$vr1, \$vr2
 4a8:	71200820 	vpickod.b   	\$vr0, \$vr1, \$vr2
 4ac:	71208820 	vpickod.h   	\$vr0, \$vr1, \$vr2
 4b0:	71210820 	vpickod.w   	\$vr0, \$vr1, \$vr2
 4b4:	71218820 	vpickod.d   	\$vr0, \$vr1, \$vr2
 4b8:	71220820 	vreplve.b   	\$vr0, \$vr1, \$tp
 4bc:	71228820 	vreplve.h   	\$vr0, \$vr1, \$tp
 4c0:	71230820 	vreplve.w   	\$vr0, \$vr1, \$tp
 4c4:	71238820 	vreplve.d   	\$vr0, \$vr1, \$tp
 4c8:	71260820 	vand.v      	\$vr0, \$vr1, \$vr2
 4cc:	71268820 	vor.v       	\$vr0, \$vr1, \$vr2
 4d0:	71270820 	vxor.v      	\$vr0, \$vr1, \$vr2
 4d4:	71278820 	vnor.v      	\$vr0, \$vr1, \$vr2
 4d8:	71280820 	vandn.v     	\$vr0, \$vr1, \$vr2
 4dc:	71288820 	vorn.v      	\$vr0, \$vr1, \$vr2
 4e0:	712b0820 	vfrstp.b    	\$vr0, \$vr1, \$vr2
 4e4:	712b8820 	vfrstp.h    	\$vr0, \$vr1, \$vr2
 4e8:	712d0820 	vadd.q      	\$vr0, \$vr1, \$vr2
 4ec:	712d8820 	vsub.q      	\$vr0, \$vr1, \$vr2
 4f0:	712e0820 	vsigncov.b  	\$vr0, \$vr1, \$vr2
 4f4:	712e8820 	vsigncov.h  	\$vr0, \$vr1, \$vr2
 4f8:	712f0820 	vsigncov.w  	\$vr0, \$vr1, \$vr2
 4fc:	712f8820 	vsigncov.d  	\$vr0, \$vr1, \$vr2
 500:	71308820 	vfadd.s     	\$vr0, \$vr1, \$vr2
 504:	71310820 	vfadd.d     	\$vr0, \$vr1, \$vr2
 508:	71328820 	vfsub.s     	\$vr0, \$vr1, \$vr2
 50c:	71330820 	vfsub.d     	\$vr0, \$vr1, \$vr2
 510:	71388820 	vfmul.s     	\$vr0, \$vr1, \$vr2
 514:	71390820 	vfmul.d     	\$vr0, \$vr1, \$vr2
 518:	713a8820 	vfdiv.s     	\$vr0, \$vr1, \$vr2
 51c:	713b0820 	vfdiv.d     	\$vr0, \$vr1, \$vr2
 520:	713c8820 	vfmax.s     	\$vr0, \$vr1, \$vr2
 524:	713d0820 	vfmax.d     	\$vr0, \$vr1, \$vr2
 528:	713e8820 	vfmin.s     	\$vr0, \$vr1, \$vr2
 52c:	713f0820 	vfmin.d     	\$vr0, \$vr1, \$vr2
 530:	71408820 	vfmaxa.s    	\$vr0, \$vr1, \$vr2
 534:	71410820 	vfmaxa.d    	\$vr0, \$vr1, \$vr2
 538:	71428820 	vfmina.s    	\$vr0, \$vr1, \$vr2
 53c:	71430820 	vfmina.d    	\$vr0, \$vr1, \$vr2
 540:	71460820 	vfcvt.h.s   	\$vr0, \$vr1, \$vr2
 544:	71468820 	vfcvt.s.d   	\$vr0, \$vr1, \$vr2
 548:	71480820 	vffint.s.l  	\$vr0, \$vr1, \$vr2
 54c:	71498820 	vftint.w.d  	\$vr0, \$vr1, \$vr2
 550:	714a0820 	vftintrm.w.d	\$vr0, \$vr1, \$vr2
 554:	714a8820 	vftintrp.w.d	\$vr0, \$vr1, \$vr2
 558:	714b0820 	vftintrz.w.d	\$vr0, \$vr1, \$vr2
 55c:	714b8820 	vftintrne.w.d	\$vr0, \$vr1, \$vr2
 560:	717a8820 	vshuf.h     	\$vr0, \$vr1, \$vr2
 564:	717b0820 	vshuf.w     	\$vr0, \$vr1, \$vr2
 568:	717b8820 	vshuf.d     	\$vr0, \$vr1, \$vr2
 56c:	72800420 	vseqi.b     	\$vr0, \$vr1, 1
 570:	72808420 	vseqi.h     	\$vr0, \$vr1, 1
 574:	72810420 	vseqi.w     	\$vr0, \$vr1, 1
 578:	72818420 	vseqi.d     	\$vr0, \$vr1, 1
 57c:	72820420 	vslei.b     	\$vr0, \$vr1, 1
 580:	72828420 	vslei.h     	\$vr0, \$vr1, 1
 584:	72830420 	vslei.w     	\$vr0, \$vr1, 1
 588:	72838420 	vslei.d     	\$vr0, \$vr1, 1
 58c:	72840420 	vslei.bu    	\$vr0, \$vr1, 0x1
 590:	72848420 	vslei.hu    	\$vr0, \$vr1, 0x1
 594:	72850420 	vslei.wu    	\$vr0, \$vr1, 0x1
 598:	72858420 	vslei.du    	\$vr0, \$vr1, 0x1
 59c:	72860420 	vslti.b     	\$vr0, \$vr1, 1
 5a0:	72868420 	vslti.h     	\$vr0, \$vr1, 1
 5a4:	72870420 	vslti.w     	\$vr0, \$vr1, 1
 5a8:	72878420 	vslti.d     	\$vr0, \$vr1, 1
 5ac:	72880420 	vslti.bu    	\$vr0, \$vr1, 0x1
 5b0:	72888420 	vslti.hu    	\$vr0, \$vr1, 0x1
 5b4:	72890420 	vslti.wu    	\$vr0, \$vr1, 0x1
 5b8:	72898420 	vslti.du    	\$vr0, \$vr1, 0x1
 5bc:	728a0420 	vaddi.bu    	\$vr0, \$vr1, 0x1
 5c0:	728a8420 	vaddi.hu    	\$vr0, \$vr1, 0x1
 5c4:	728b0420 	vaddi.wu    	\$vr0, \$vr1, 0x1
 5c8:	728b8420 	vaddi.du    	\$vr0, \$vr1, 0x1
 5cc:	728c0420 	vsubi.bu    	\$vr0, \$vr1, 0x1
 5d0:	728c8420 	vsubi.hu    	\$vr0, \$vr1, 0x1
 5d4:	728d0420 	vsubi.wu    	\$vr0, \$vr1, 0x1
 5d8:	728d8420 	vsubi.du    	\$vr0, \$vr1, 0x1
 5dc:	728e0420 	vbsll.v     	\$vr0, \$vr1, 0x1
 5e0:	728e8420 	vbsrl.v     	\$vr0, \$vr1, 0x1
 5e4:	72900420 	vmaxi.b     	\$vr0, \$vr1, 1
 5e8:	72908420 	vmaxi.h     	\$vr0, \$vr1, 1
 5ec:	72910420 	vmaxi.w     	\$vr0, \$vr1, 1
 5f0:	72918420 	vmaxi.d     	\$vr0, \$vr1, 1
 5f4:	72920420 	vmini.b     	\$vr0, \$vr1, 1
 5f8:	72928420 	vmini.h     	\$vr0, \$vr1, 1
 5fc:	72930420 	vmini.w     	\$vr0, \$vr1, 1
 600:	72938420 	vmini.d     	\$vr0, \$vr1, 1
 604:	72940420 	vmaxi.bu    	\$vr0, \$vr1, 0x1
 608:	72948420 	vmaxi.hu    	\$vr0, \$vr1, 0x1
 60c:	72950420 	vmaxi.wu    	\$vr0, \$vr1, 0x1
 610:	72958420 	vmaxi.du    	\$vr0, \$vr1, 0x1
 614:	72960420 	vmini.bu    	\$vr0, \$vr1, 0x1
 618:	72968420 	vmini.hu    	\$vr0, \$vr1, 0x1
 61c:	72970420 	vmini.wu    	\$vr0, \$vr1, 0x1
 620:	72978420 	vmini.du    	\$vr0, \$vr1, 0x1
 624:	729a0420 	vfrstpi.b   	\$vr0, \$vr1, 0x1
 628:	729a8420 	vfrstpi.h   	\$vr0, \$vr1, 0x1
 62c:	729c0020 	vclo.b      	\$vr0, \$vr1
 630:	729c0420 	vclo.h      	\$vr0, \$vr1
 634:	729c0820 	vclo.w      	\$vr0, \$vr1
 638:	729c0c20 	vclo.d      	\$vr0, \$vr1
 63c:	729c1020 	vclz.b      	\$vr0, \$vr1
 640:	729c1420 	vclz.h      	\$vr0, \$vr1
 644:	729c1820 	vclz.w      	\$vr0, \$vr1
 648:	729c1c20 	vclz.d      	\$vr0, \$vr1
 64c:	729c2020 	vpcnt.b     	\$vr0, \$vr1
 650:	729c2420 	vpcnt.h     	\$vr0, \$vr1
 654:	729c2820 	vpcnt.w     	\$vr0, \$vr1
 658:	729c2c20 	vpcnt.d     	\$vr0, \$vr1
 65c:	729c3020 	vneg.b      	\$vr0, \$vr1
 660:	729c3420 	vneg.h      	\$vr0, \$vr1
 664:	729c3820 	vneg.w      	\$vr0, \$vr1
 668:	729c3c20 	vneg.d      	\$vr0, \$vr1
 66c:	729c4020 	vmskltz.b   	\$vr0, \$vr1
 670:	729c4420 	vmskltz.h   	\$vr0, \$vr1
 674:	729c4820 	vmskltz.w   	\$vr0, \$vr1
 678:	729c4c20 	vmskltz.d   	\$vr0, \$vr1
 67c:	729c5020 	vmskgez.b   	\$vr0, \$vr1
 680:	729c6020 	vmsknz.b    	\$vr0, \$vr1
 684:	729c9820 	vseteqz.v   	\$fcc0, \$vr1
 688:	729c9c20 	vsetnez.v   	\$fcc0, \$vr1
 68c:	729ca020 	vsetanyeqz.b	\$fcc0, \$vr1
 690:	729ca420 	vsetanyeqz.h	\$fcc0, \$vr1
 694:	729ca820 	vsetanyeqz.w	\$fcc0, \$vr1
 698:	729cac20 	vsetanyeqz.d	\$fcc0, \$vr1
 69c:	729cb020 	vsetallnez.b	\$fcc0, \$vr1
 6a0:	729cb420 	vsetallnez.h	\$fcc0, \$vr1
 6a4:	729cb820 	vsetallnez.w	\$fcc0, \$vr1
 6a8:	729cbc20 	vsetallnez.d	\$fcc0, \$vr1
 6ac:	729cc420 	vflogb.s    	\$vr0, \$vr1
 6b0:	729cc820 	vflogb.d    	\$vr0, \$vr1
 6b4:	729cd420 	vfclass.s   	\$vr0, \$vr1
 6b8:	729cd820 	vfclass.d   	\$vr0, \$vr1
 6bc:	729ce420 	vfsqrt.s    	\$vr0, \$vr1
 6c0:	729ce820 	vfsqrt.d    	\$vr0, \$vr1
 6c4:	729cf420 	vfrecip.s   	\$vr0, \$vr1
 6c8:	729cf820 	vfrecip.d   	\$vr0, \$vr1
 6cc:	729d0420 	vfrsqrt.s   	\$vr0, \$vr1
 6d0:	729d0820 	vfrsqrt.d   	\$vr0, \$vr1
 6d4:	729d3420 	vfrint.s    	\$vr0, \$vr1
 6d8:	729d3820 	vfrint.d    	\$vr0, \$vr1
 6dc:	729d4420 	vfrintrm.s  	\$vr0, \$vr1
 6e0:	729d4820 	vfrintrm.d  	\$vr0, \$vr1
 6e4:	729d5420 	vfrintrp.s  	\$vr0, \$vr1
 6e8:	729d5820 	vfrintrp.d  	\$vr0, \$vr1
 6ec:	729d6420 	vfrintrz.s  	\$vr0, \$vr1
 6f0:	729d6820 	vfrintrz.d  	\$vr0, \$vr1
 6f4:	729d7420 	vfrintrne.s 	\$vr0, \$vr1
 6f8:	729d7820 	vfrintrne.d 	\$vr0, \$vr1
 6fc:	729de820 	vfcvtl.s.h  	\$vr0, \$vr1
 700:	729dec20 	vfcvth.s.h  	\$vr0, \$vr1
 704:	729df020 	vfcvtl.d.s  	\$vr0, \$vr1
 708:	729df420 	vfcvth.d.s  	\$vr0, \$vr1
 70c:	729e0020 	vffint.s.w  	\$vr0, \$vr1
 710:	729e0420 	vffint.s.wu 	\$vr0, \$vr1
 714:	729e0820 	vffint.d.l  	\$vr0, \$vr1
 718:	729e0c20 	vffint.d.lu 	\$vr0, \$vr1
 71c:	729e1020 	vffintl.d.w 	\$vr0, \$vr1
 720:	729e1420 	vffinth.d.w 	\$vr0, \$vr1
 724:	729e3020 	vftint.w.s  	\$vr0, \$vr1
 728:	729e3420 	vftint.l.d  	\$vr0, \$vr1
 72c:	729e3820 	vftintrm.w.s	\$vr0, \$vr1
 730:	729e3c20 	vftintrm.l.d	\$vr0, \$vr1
 734:	729e4020 	vftintrp.w.s	\$vr0, \$vr1
 738:	729e4420 	vftintrp.l.d	\$vr0, \$vr1
 73c:	729e4820 	vftintrz.w.s	\$vr0, \$vr1
 740:	729e4c20 	vftintrz.l.d	\$vr0, \$vr1
 744:	729e5020 	vftintrne.w.s	\$vr0, \$vr1
 748:	729e5420 	vftintrne.l.d	\$vr0, \$vr1
 74c:	729e5820 	vftint.wu.s 	\$vr0, \$vr1
 750:	729e5c20 	vftint.lu.d 	\$vr0, \$vr1
 754:	729e7020 	vftintrz.wu.s	\$vr0, \$vr1
 758:	729e7420 	vftintrz.lu.d	\$vr0, \$vr1
 75c:	729e8020 	vftintl.l.s 	\$vr0, \$vr1
 760:	729e8420 	vftinth.l.s 	\$vr0, \$vr1
 764:	729e8820 	vftintrml.l.s	\$vr0, \$vr1
 768:	729e8c20 	vftintrmh.l.s	\$vr0, \$vr1
 76c:	729e9020 	vftintrpl.l.s	\$vr0, \$vr1
 770:	729e9420 	vftintrph.l.s	\$vr0, \$vr1
 774:	729e9820 	vftintrzl.l.s	\$vr0, \$vr1
 778:	729e9c20 	vftintrzh.l.s	\$vr0, \$vr1
 77c:	729ea020 	vftintrnel.l.s	\$vr0, \$vr1
 780:	729ea420 	vftintrneh.l.s	\$vr0, \$vr1
 784:	729ee020 	vexth.h.b   	\$vr0, \$vr1
 788:	729ee420 	vexth.w.h   	\$vr0, \$vr1
 78c:	729ee820 	vexth.d.w   	\$vr0, \$vr1
 790:	729eec20 	vexth.q.d   	\$vr0, \$vr1
 794:	729ef020 	vexth.hu.bu 	\$vr0, \$vr1
 798:	729ef420 	vexth.wu.hu 	\$vr0, \$vr1
 79c:	729ef820 	vexth.du.wu 	\$vr0, \$vr1
 7a0:	729efc20 	vexth.qu.du 	\$vr0, \$vr1
 7a4:	729f0020 	vreplgr2vr.b	\$vr0, \$ra
 7a8:	729f0420 	vreplgr2vr.h	\$vr0, \$ra
 7ac:	729f0820 	vreplgr2vr.w	\$vr0, \$ra
 7b0:	729f0c20 	vreplgr2vr.d	\$vr0, \$ra
 7b4:	72a02420 	vrotri.b    	\$vr0, \$vr1, 0x1
 7b8:	72a04420 	vrotri.h    	\$vr0, \$vr1, 0x1
 7bc:	72a08420 	vrotri.w    	\$vr0, \$vr1, 0x1
 7c0:	72a10420 	vrotri.d    	\$vr0, \$vr1, 0x1
 7c4:	72a42420 	vsrlri.b    	\$vr0, \$vr1, 0x1
 7c8:	72a44420 	vsrlri.h    	\$vr0, \$vr1, 0x1
 7cc:	72a48420 	vsrlri.w    	\$vr0, \$vr1, 0x1
 7d0:	72a50420 	vsrlri.d    	\$vr0, \$vr1, 0x1
 7d4:	72a82420 	vsrari.b    	\$vr0, \$vr1, 0x1
 7d8:	72a84420 	vsrari.h    	\$vr0, \$vr1, 0x1
 7dc:	72a88420 	vsrari.w    	\$vr0, \$vr1, 0x1
 7e0:	72a90420 	vsrari.d    	\$vr0, \$vr1, 0x1
 7e4:	72eb8420 	vinsgr2vr.b 	\$vr0, \$ra, 0x1
 7e8:	72ebc420 	vinsgr2vr.h 	\$vr0, \$ra, 0x1
 7ec:	72ebe420 	vinsgr2vr.w 	\$vr0, \$ra, 0x1
 7f0:	72ebf420 	vinsgr2vr.d 	\$vr0, \$ra, 0x1
 7f4:	72ef8420 	vpickve2gr.b	\$zero, \$vr1, 0x1
 7f8:	72efc420 	vpickve2gr.h	\$zero, \$vr1, 0x1
 7fc:	72efe420 	vpickve2gr.w	\$zero, \$vr1, 0x1
 800:	72eff420 	vpickve2gr.d	\$zero, \$vr1, 0x1
 804:	72f38420 	vpickve2gr.bu	\$zero, \$vr1, 0x1
 808:	72f3c420 	vpickve2gr.hu	\$zero, \$vr1, 0x1
 80c:	72f3e420 	vpickve2gr.wu	\$zero, \$vr1, 0x1
 810:	72f3f420 	vpickve2gr.du	\$zero, \$vr1, 0x1
 814:	72f78420 	vreplvei.b  	\$vr0, \$vr1, 0x1
 818:	72f7c420 	vreplvei.h  	\$vr0, \$vr1, 0x1
 81c:	72f7e420 	vreplvei.w  	\$vr0, \$vr1, 0x1
 820:	72f7f420 	vreplvei.d  	\$vr0, \$vr1, 0x1
 824:	73082420 	vsllwil.h.b 	\$vr0, \$vr1, 0x1
 828:	73084420 	vsllwil.w.h 	\$vr0, \$vr1, 0x1
 82c:	73088420 	vsllwil.d.w 	\$vr0, \$vr1, 0x1
 830:	73090020 	vextl.q.d   	\$vr0, \$vr1
 834:	730c2420 	vsllwil.hu.bu	\$vr0, \$vr1, 0x1
 838:	730c4420 	vsllwil.wu.hu	\$vr0, \$vr1, 0x1
 83c:	730c8420 	vsllwil.du.wu	\$vr0, \$vr1, 0x1
 840:	730d0020 	vextl.qu.du 	\$vr0, \$vr1
 844:	73102420 	vbitclri.b  	\$vr0, \$vr1, 0x1
 848:	73104420 	vbitclri.h  	\$vr0, \$vr1, 0x1
 84c:	73108420 	vbitclri.w  	\$vr0, \$vr1, 0x1
 850:	73110420 	vbitclri.d  	\$vr0, \$vr1, 0x1
 854:	73142420 	vbitseti.b  	\$vr0, \$vr1, 0x1
 858:	73144420 	vbitseti.h  	\$vr0, \$vr1, 0x1
 85c:	73148420 	vbitseti.w  	\$vr0, \$vr1, 0x1
 860:	73150420 	vbitseti.d  	\$vr0, \$vr1, 0x1
 864:	73182420 	vbitrevi.b  	\$vr0, \$vr1, 0x1
 868:	73184420 	vbitrevi.h  	\$vr0, \$vr1, 0x1
 86c:	73188420 	vbitrevi.w  	\$vr0, \$vr1, 0x1
 870:	73190420 	vbitrevi.d  	\$vr0, \$vr1, 0x1
 874:	73242420 	vsat.b      	\$vr0, \$vr1, 0x1
 878:	73244420 	vsat.h      	\$vr0, \$vr1, 0x1
 87c:	73248420 	vsat.w      	\$vr0, \$vr1, 0x1
 880:	73250420 	vsat.d      	\$vr0, \$vr1, 0x1
 884:	73282420 	vsat.bu     	\$vr0, \$vr1, 0x1
 888:	73284420 	vsat.hu     	\$vr0, \$vr1, 0x1
 88c:	73288420 	vsat.wu     	\$vr0, \$vr1, 0x1
 890:	73290420 	vsat.du     	\$vr0, \$vr1, 0x1
 894:	732c2420 	vslli.b     	\$vr0, \$vr1, 0x1
 898:	732c4420 	vslli.h     	\$vr0, \$vr1, 0x1
 89c:	732c8420 	vslli.w     	\$vr0, \$vr1, 0x1
 8a0:	732d0420 	vslli.d     	\$vr0, \$vr1, 0x1
 8a4:	73302420 	vsrli.b     	\$vr0, \$vr1, 0x1
 8a8:	73304420 	vsrli.h     	\$vr0, \$vr1, 0x1
 8ac:	73308420 	vsrli.w     	\$vr0, \$vr1, 0x1
 8b0:	73310420 	vsrli.d     	\$vr0, \$vr1, 0x1
 8b4:	73342420 	vsrai.b     	\$vr0, \$vr1, 0x1
 8b8:	73344420 	vsrai.h     	\$vr0, \$vr1, 0x1
 8bc:	73348420 	vsrai.w     	\$vr0, \$vr1, 0x1
 8c0:	73350420 	vsrai.d     	\$vr0, \$vr1, 0x1
 8c4:	73404420 	vsrlni.b.h  	\$vr0, \$vr1, 0x1
 8c8:	73408420 	vsrlni.h.w  	\$vr0, \$vr1, 0x1
 8cc:	73410420 	vsrlni.w.d  	\$vr0, \$vr1, 0x1
 8d0:	73420420 	vsrlni.d.q  	\$vr0, \$vr1, 0x1
 8d4:	73484420 	vssrlni.b.h 	\$vr0, \$vr1, 0x1
 8d8:	73488420 	vssrlni.h.w 	\$vr0, \$vr1, 0x1
 8dc:	73490420 	vssrlni.w.d 	\$vr0, \$vr1, 0x1
 8e0:	734a0420 	vssrlni.d.q 	\$vr0, \$vr1, 0x1
 8e4:	73444420 	vsrlrni.b.h 	\$vr0, \$vr1, 0x1
 8e8:	73448420 	vsrlrni.h.w 	\$vr0, \$vr1, 0x1
 8ec:	73450420 	vsrlrni.w.d 	\$vr0, \$vr1, 0x1
 8f0:	73460420 	vsrlrni.d.q 	\$vr0, \$vr1, 0x1
 8f4:	734c4420 	vssrlni.bu.h	\$vr0, \$vr1, 0x1
 8f8:	734c8420 	vssrlni.hu.w	\$vr0, \$vr1, 0x1
 8fc:	734d0420 	vssrlni.wu.d	\$vr0, \$vr1, 0x1
 900:	734e0420 	vssrlni.du.q	\$vr0, \$vr1, 0x1
 904:	73504420 	vssrlrni.b.h	\$vr0, \$vr1, 0x1
 908:	73508420 	vssrlrni.h.w	\$vr0, \$vr1, 0x1
 90c:	73510420 	vssrlrni.w.d	\$vr0, \$vr1, 0x1
 910:	73520420 	vssrlrni.d.q	\$vr0, \$vr1, 0x1
 914:	73544420 	vssrlrni.bu.h	\$vr0, \$vr1, 0x1
 918:	73548420 	vssrlrni.hu.w	\$vr0, \$vr1, 0x1
 91c:	73550420 	vssrlrni.wu.d	\$vr0, \$vr1, 0x1
 920:	73560420 	vssrlrni.du.q	\$vr0, \$vr1, 0x1
 924:	73584420 	vsrani.b.h  	\$vr0, \$vr1, 0x1
 928:	73588420 	vsrani.h.w  	\$vr0, \$vr1, 0x1
 92c:	73590420 	vsrani.w.d  	\$vr0, \$vr1, 0x1
 930:	735a0420 	vsrani.d.q  	\$vr0, \$vr1, 0x1
 934:	735c4420 	vsrarni.b.h 	\$vr0, \$vr1, 0x1
 938:	735c8420 	vsrarni.h.w 	\$vr0, \$vr1, 0x1
 93c:	735d0420 	vsrarni.w.d 	\$vr0, \$vr1, 0x1
 940:	735e0420 	vsrarni.d.q 	\$vr0, \$vr1, 0x1
 944:	73604420 	vssrani.b.h 	\$vr0, \$vr1, 0x1
 948:	73608420 	vssrani.h.w 	\$vr0, \$vr1, 0x1
 94c:	73610420 	vssrani.w.d 	\$vr0, \$vr1, 0x1
 950:	73620420 	vssrani.d.q 	\$vr0, \$vr1, 0x1
 954:	73644420 	vssrani.bu.h	\$vr0, \$vr1, 0x1
 958:	73648420 	vssrani.hu.w	\$vr0, \$vr1, 0x1
 95c:	73650420 	vssrani.wu.d	\$vr0, \$vr1, 0x1
 960:	73660420 	vssrani.du.q	\$vr0, \$vr1, 0x1
 964:	73684420 	vssrarni.b.h	\$vr0, \$vr1, 0x1
 968:	73688420 	vssrarni.h.w	\$vr0, \$vr1, 0x1
 96c:	73690420 	vssrarni.w.d	\$vr0, \$vr1, 0x1
 970:	736a0420 	vssrarni.d.q	\$vr0, \$vr1, 0x1
 974:	736c4420 	vssrarni.bu.h	\$vr0, \$vr1, 0x1
 978:	736c8420 	vssrarni.hu.w	\$vr0, \$vr1, 0x1
 97c:	736d0420 	vssrarni.wu.d	\$vr0, \$vr1, 0x1
 980:	736e0420 	vssrarni.du.q	\$vr0, \$vr1, 0x1
 984:	73800420 	vextrins.d  	\$vr0, \$vr1, 0x1
 988:	73840420 	vextrins.w  	\$vr0, \$vr1, 0x1
 98c:	73880420 	vextrins.h  	\$vr0, \$vr1, 0x1
 990:	738c0420 	vextrins.b  	\$vr0, \$vr1, 0x1
 994:	73900420 	vshuf4i.b   	\$vr0, \$vr1, 0x1
 998:	73940420 	vshuf4i.h   	\$vr0, \$vr1, 0x1
 99c:	73980420 	vshuf4i.w   	\$vr0, \$vr1, 0x1
 9a0:	739c0420 	vshuf4i.d   	\$vr0, \$vr1, 0x1
 9a4:	73c40420 	vbitseli.b  	\$vr0, \$vr1, 0x1
 9a8:	73d00420 	vandi.b     	\$vr0, \$vr1, 0x1
 9ac:	73d40420 	vori.b      	\$vr0, \$vr1, 0x1
 9b0:	73d80420 	vxori.b     	\$vr0, \$vr1, 0x1
 9b4:	73dc0420 	vnori.b     	\$vr0, \$vr1, 0x1
 9b8:	73e00020 	vldi        	\$vr0, 1
 9bc:	701e0820 	vaddwev.h.b 	\$vr0, \$vr1, \$vr2
 9c0:	701e8820 	vaddwev.w.h 	\$vr0, \$vr1, \$vr2
 9c4:	701f0820 	vaddwev.d.w 	\$vr0, \$vr1, \$vr2
 9c8:	701f8820 	vaddwev.q.d 	\$vr0, \$vr1, \$vr2
 9cc:	702e0820 	vaddwev.h.bu	\$vr0, \$vr1, \$vr2
 9d0:	702e8820 	vaddwev.w.hu	\$vr0, \$vr1, \$vr2
 9d4:	702f0820 	vaddwev.d.wu	\$vr0, \$vr1, \$vr2
 9d8:	702f8820 	vaddwev.q.du	\$vr0, \$vr1, \$vr2
 9dc:	703e0820 	vaddwev.h.bu.b	\$vr0, \$vr1, \$vr2
 9e0:	703e8820 	vaddwev.w.hu.h	\$vr0, \$vr1, \$vr2
 9e4:	703f0820 	vaddwev.d.wu.w	\$vr0, \$vr1, \$vr2
 9e8:	703f8820 	vaddwev.q.du.d	\$vr0, \$vr1, \$vr2
 9ec:	70220820 	vaddwod.h.b 	\$vr0, \$vr1, \$vr2
 9f0:	70228820 	vaddwod.w.h 	\$vr0, \$vr1, \$vr2
 9f4:	70230820 	vaddwod.d.w 	\$vr0, \$vr1, \$vr2
 9f8:	70238820 	vaddwod.q.d 	\$vr0, \$vr1, \$vr2
 9fc:	70320820 	vaddwod.h.bu	\$vr0, \$vr1, \$vr2
 a00:	70328820 	vaddwod.w.hu	\$vr0, \$vr1, \$vr2
 a04:	70330820 	vaddwod.d.wu	\$vr0, \$vr1, \$vr2
 a08:	70338820 	vaddwod.q.du	\$vr0, \$vr1, \$vr2
 a0c:	70400820 	vaddwod.h.bu.b	\$vr0, \$vr1, \$vr2
 a10:	70408820 	vaddwod.w.hu.h	\$vr0, \$vr1, \$vr2
 a14:	70410820 	vaddwod.d.wu.w	\$vr0, \$vr1, \$vr2
 a18:	70418820 	vaddwod.q.du.d	\$vr0, \$vr1, \$vr2
 a1c:	70ac0820 	vmaddwev.h.b	\$vr0, \$vr1, \$vr2
 a20:	70ac8820 	vmaddwev.w.h	\$vr0, \$vr1, \$vr2
 a24:	70ad0820 	vmaddwev.d.w	\$vr0, \$vr1, \$vr2
 a28:	70ad8820 	vmaddwev.q.d	\$vr0, \$vr1, \$vr2
 a2c:	70b40820 	vmaddwev.h.bu	\$vr0, \$vr1, \$vr2
 a30:	70b48820 	vmaddwev.w.hu	\$vr0, \$vr1, \$vr2
 a34:	70b50820 	vmaddwev.d.wu	\$vr0, \$vr1, \$vr2
 a38:	70b58820 	vmaddwev.q.du	\$vr0, \$vr1, \$vr2
 a3c:	70bc0820 	vmaddwev.h.bu.b	\$vr0, \$vr1, \$vr2
 a40:	70bc8820 	vmaddwev.w.hu.h	\$vr0, \$vr1, \$vr2
 a44:	70bd0820 	vmaddwev.d.wu.w	\$vr0, \$vr1, \$vr2
 a48:	70bd8820 	vmaddwev.q.du.d	\$vr0, \$vr1, \$vr2
 a4c:	70ae0820 	vmaddwod.h.b	\$vr0, \$vr1, \$vr2
 a50:	70ae8820 	vmaddwod.w.h	\$vr0, \$vr1, \$vr2
 a54:	70af0820 	vmaddwod.d.w	\$vr0, \$vr1, \$vr2
 a58:	70af8820 	vmaddwod.q.d	\$vr0, \$vr1, \$vr2
 a5c:	70b60820 	vmaddwod.h.bu	\$vr0, \$vr1, \$vr2
 a60:	70b68820 	vmaddwod.w.hu	\$vr0, \$vr1, \$vr2
 a64:	70b70820 	vmaddwod.d.wu	\$vr0, \$vr1, \$vr2
 a68:	70b78820 	vmaddwod.q.du	\$vr0, \$vr1, \$vr2
 a6c:	70be0820 	vmaddwod.h.bu.b	\$vr0, \$vr1, \$vr2
 a70:	70be8820 	vmaddwod.w.hu.h	\$vr0, \$vr1, \$vr2
 a74:	70bf0820 	vmaddwod.d.wu.w	\$vr0, \$vr1, \$vr2
 a78:	70bf8820 	vmaddwod.q.du.d	\$vr0, \$vr1, \$vr2
 a7c:	70900820 	vmulwev.h.b 	\$vr0, \$vr1, \$vr2
 a80:	70908820 	vmulwev.w.h 	\$vr0, \$vr1, \$vr2
 a84:	70910820 	vmulwev.d.w 	\$vr0, \$vr1, \$vr2
 a88:	70918820 	vmulwev.q.d 	\$vr0, \$vr1, \$vr2
 a8c:	70980820 	vmulwev.h.bu	\$vr0, \$vr1, \$vr2
 a90:	70988820 	vmulwev.w.hu	\$vr0, \$vr1, \$vr2
 a94:	70990820 	vmulwev.d.wu	\$vr0, \$vr1, \$vr2
 a98:	70998820 	vmulwev.q.du	\$vr0, \$vr1, \$vr2
 a9c:	70a00820 	vmulwev.h.bu.b	\$vr0, \$vr1, \$vr2
 aa0:	70a08820 	vmulwev.w.hu.h	\$vr0, \$vr1, \$vr2
 aa4:	70a10820 	vmulwev.d.wu.w	\$vr0, \$vr1, \$vr2
 aa8:	70a18820 	vmulwev.q.du.d	\$vr0, \$vr1, \$vr2
 aac:	70920820 	vmulwod.h.b 	\$vr0, \$vr1, \$vr2
 ab0:	70928820 	vmulwod.w.h 	\$vr0, \$vr1, \$vr2
 ab4:	70930820 	vmulwod.d.w 	\$vr0, \$vr1, \$vr2
 ab8:	70938820 	vmulwod.q.d 	\$vr0, \$vr1, \$vr2
 abc:	709a0820 	vmulwod.h.bu	\$vr0, \$vr1, \$vr2
 ac0:	709a8820 	vmulwod.w.hu	\$vr0, \$vr1, \$vr2
 ac4:	709b0820 	vmulwod.d.wu	\$vr0, \$vr1, \$vr2
 ac8:	709b8820 	vmulwod.q.du	\$vr0, \$vr1, \$vr2
 acc:	70a20820 	vmulwod.h.bu.b	\$vr0, \$vr1, \$vr2
 ad0:	70a28820 	vmulwod.w.hu.h	\$vr0, \$vr1, \$vr2
 ad4:	70a30820 	vmulwod.d.wu.w	\$vr0, \$vr1, \$vr2
 ad8:	70a38820 	vmulwod.q.du.d	\$vr0, \$vr1, \$vr2
 adc:	70200820 	vsubwev.h.b 	\$vr0, \$vr1, \$vr2
 ae0:	70208820 	vsubwev.w.h 	\$vr0, \$vr1, \$vr2
 ae4:	70210820 	vsubwev.d.w 	\$vr0, \$vr1, \$vr2
 ae8:	70218820 	vsubwev.q.d 	\$vr0, \$vr1, \$vr2
 aec:	70300820 	vsubwev.h.bu	\$vr0, \$vr1, \$vr2
 af0:	70308820 	vsubwev.w.hu	\$vr0, \$vr1, \$vr2
 af4:	70310820 	vsubwev.d.wu	\$vr0, \$vr1, \$vr2
 af8:	70318820 	vsubwev.q.du	\$vr0, \$vr1, \$vr2
 afc:	70240820 	vsubwod.h.b 	\$vr0, \$vr1, \$vr2
 b00:	70248820 	vsubwod.w.h 	\$vr0, \$vr1, \$vr2
 b04:	70250820 	vsubwod.d.w 	\$vr0, \$vr1, \$vr2
 b08:	70258820 	vsubwod.q.d 	\$vr0, \$vr1, \$vr2
 b0c:	70340820 	vsubwod.h.bu	\$vr0, \$vr1, \$vr2
 b10:	70348820 	vsubwod.w.hu	\$vr0, \$vr1, \$vr2
 b14:	70350820 	vsubwod.d.wu	\$vr0, \$vr1, \$vr2
 b18:	70358820 	vsubwod.q.du	\$vr0, \$vr1, \$vr2
 b1c:	73e18020 	vldi        	\$vr0, 3073
 b20:	73e08020 	vldi        	\$vr0, 1025
 b24:	73e10020 	vldi        	\$vr0, 2049
 b28:	73e00020 	vldi        	\$vr0, 1
 b2c:	73e40420 	vpermi.w    	\$vr0, \$vr1, 0x1
 b30:	769f1020 	vext2xv.h.b 	\$xr0, \$xr1
 b34:	769f1420 	vext2xv.w.b 	\$xr0, \$xr1
 b38:	769f1820 	vext2xv.d.b 	\$xr0, \$xr1
 b3c:	769f1c20 	vext2xv.w.h 	\$xr0, \$xr1
 b40:	769f2020 	vext2xv.d.h 	\$xr0, \$xr1
 b44:	769f2420 	vext2xv.d.w 	\$xr0, \$xr1
 b48:	769f2820 	vext2xv.hu.bu	\$xr0, \$xr1
 b4c:	769f2c20 	vext2xv.wu.bu	\$xr0, \$xr1
 b50:	769f3020 	vext2xv.du.bu	\$xr0, \$xr1
 b54:	769f3420 	vext2xv.wu.hu	\$xr0, \$xr1
 b58:	769f3820 	vext2xv.du.hu	\$xr0, \$xr1
 b5c:	769f3c20 	vext2xv.du.wu	\$xr0, \$xr1
 b60:	729d1420 	vfrecipe.s  	\$vr0, \$vr1
 b64:	729d1820 	vfrecipe.d  	\$vr0, \$vr1
 b68:	729d2420 	vfrsqrte.s  	\$vr0, \$vr1
 b6c:	729d2820 	vfrsqrte.d  	\$vr0, \$vr1
