vfmadd.s	$vr0, $vr1, $vr2, $vr3
vfmsub.s	$vr0, $vr1, $vr2, $vr3
vfnmadd.s	$vr0, $vr1, $vr2, $vr3
vfnmsub.s	$vr0, $vr1, $vr2, $vr3
vfcmp.caf.s	$vr0, $vr1, $vr2
vfcmp.saf.s	$vr0, $vr1, $vr2
vfcmp.clt.s	$vr0, $vr1, $vr2
vfcmp.slt.s	$vr0, $vr1, $vr2
vfcmp.ceq.s	$vr0, $vr1, $vr2
vfcmp.seq.s	$vr0, $vr1, $vr2
vfcmp.cle.s	$vr0, $vr1, $vr2
vfcmp.sle.s	$vr0, $vr1, $vr2
vfcmp.cun.s	$vr0, $vr1, $vr2
vfcmp.sun.s	$vr0, $vr1, $vr2
vfcmp.cult.s	$vr0, $vr1, $vr2
vfcmp.sult.s	$vr0, $vr1, $vr2
vfcmp.cueq.s	$vr0, $vr1, $vr2
vfcmp.sueq.s	$vr0, $vr1, $vr2
vfcmp.cule.s	$vr0, $vr1, $vr2
vfcmp.sule.s	$vr0, $vr1, $vr2
vfcmp.cne.s	$vr0, $vr1, $vr2
vfcmp.sne.s	$vr0, $vr1, $vr2
vfcmp.cor.s	$vr0, $vr1, $vr2
vfcmp.sor.s	$vr0, $vr1, $vr2
vfcmp.cune.s	$vr0, $vr1, $vr2
vfcmp.sune.s	$vr0, $vr1, $vr2
vbitsel.v	$vr0, $vr1, $vr2, $vr3
vshuf.b	$vr0, $vr1, $vr2, $vr3
vfmadd.d	$vr0, $vr1, $vr2, $vr3
vfmsub.d	$vr0, $vr1, $vr2, $vr3
vfnmadd.d	$vr0, $vr1, $vr2, $vr3
vfnmsub.d	$vr0, $vr1, $vr2, $vr3
vfcmp.caf.d	$vr0, $vr1, $vr2
vfcmp.saf.d	$vr0, $vr1, $vr2
vfcmp.clt.d	$vr0, $vr1, $vr2
vfcmp.slt.d	$vr0, $vr1, $vr2
vfcmp.ceq.d	$vr0, $vr1, $vr2
vfcmp.seq.d	$vr0, $vr1, $vr2
vfcmp.cle.d	$vr0, $vr1, $vr2
vfcmp.sle.d	$vr0, $vr1, $vr2
vfcmp.cun.d	$vr0, $vr1, $vr2
vfcmp.sun.d	$vr0, $vr1, $vr2
vfcmp.cult.d	$vr0, $vr1, $vr2
vfcmp.sult.d	$vr0, $vr1, $vr2
vfcmp.cueq.d	$vr0, $vr1, $vr2
vfcmp.sueq.d	$vr0, $vr1, $vr2
vfcmp.cule.d	$vr0, $vr1, $vr2
vfcmp.sule.d	$vr0, $vr1, $vr2
vfcmp.cne.d	$vr0, $vr1, $vr2
vfcmp.sne.d	$vr0, $vr1, $vr2
vfcmp.cor.d	$vr0, $vr1, $vr2
vfcmp.sor.d	$vr0, $vr1, $vr2
vfcmp.cune.d	$vr0, $vr1, $vr2
vfcmp.sune.d	$vr0, $vr1, $vr2
vld	$vr0, $r1, 1
vst	$vr0, $r1, 1
vldx	$vr0, $r1, $r2
vstx	$vr0, $r1, $r2
vldrepl.d	$vr0, $r1, 1000
vldrepl.w	$vr0, $r1, 100
vldrepl.h	$vr0, $r1, 10
vldrepl.b	$vr0, $r1, 1
vstelm.d	$vr0, $r1, 1000, 1
vstelm.w	$vr0, $r1, 100, 1
vstelm.h	$vr0, $r1, 10, 1
vstelm.b	$vr0, $r1, 1, 1
vseq.b	$vr0, $vr1, $vr2
vseq.h	$vr0, $vr1, $vr2
vseq.w	$vr0, $vr1, $vr2
vseq.d	$vr0, $vr1, $vr2
vsle.b	$vr0, $vr1, $vr2
vsle.h	$vr0, $vr1, $vr2
vsle.w	$vr0, $vr1, $vr2
vsle.d	$vr0, $vr1, $vr2
vsle.bu	$vr0, $vr1, $vr2
vsle.hu	$vr0, $vr1, $vr2
vsle.wu	$vr0, $vr1, $vr2
vsle.du	$vr0, $vr1, $vr2
vslt.b	$vr0, $vr1, $vr2
vslt.h	$vr0, $vr1, $vr2
vslt.w	$vr0, $vr1, $vr2
vslt.d	$vr0, $vr1, $vr2
vslt.bu	$vr0, $vr1, $vr2
vslt.hu	$vr0, $vr1, $vr2
vslt.wu	$vr0, $vr1, $vr2
vslt.du	$vr0, $vr1, $vr2
vadd.b	$vr0, $vr1, $vr2
vadd.h	$vr0, $vr1, $vr2
vadd.w	$vr0, $vr1, $vr2
vadd.d	$vr0, $vr1, $vr2
vsub.b	$vr0, $vr1, $vr2
vsub.h	$vr0, $vr1, $vr2
vsub.w	$vr0, $vr1, $vr2
vsub.d	$vr0, $vr1, $vr2
vsadd.b	$vr0, $vr1, $vr2
vsadd.h	$vr0, $vr1, $vr2
vsadd.w	$vr0, $vr1, $vr2
vsadd.d	$vr0, $vr1, $vr2
vssub.b	$vr0, $vr1, $vr2
vssub.h	$vr0, $vr1, $vr2
vssub.w	$vr0, $vr1, $vr2
vssub.d	$vr0, $vr1, $vr2
vsadd.bu	$vr0, $vr1, $vr2
vsadd.hu	$vr0, $vr1, $vr2
vsadd.wu	$vr0, $vr1, $vr2
vsadd.du	$vr0, $vr1, $vr2
vssub.bu	$vr0, $vr1, $vr2
vssub.hu	$vr0, $vr1, $vr2
vssub.wu	$vr0, $vr1, $vr2
vssub.du	$vr0, $vr1, $vr2
vhaddw.h.b	$vr0, $vr1, $vr2
vhaddw.w.h	$vr0, $vr1, $vr2
vhaddw.d.w	$vr0, $vr1, $vr2
vhaddw.q.d	$vr0, $vr1, $vr2
vhsubw.h.b	$vr0, $vr1, $vr2
vhsubw.w.h	$vr0, $vr1, $vr2
vhsubw.d.w	$vr0, $vr1, $vr2
vhsubw.q.d	$vr0, $vr1, $vr2
vhaddw.hu.bu	$vr0, $vr1, $vr2
vhaddw.wu.hu	$vr0, $vr1, $vr2
vhaddw.du.wu	$vr0, $vr1, $vr2
vhaddw.qu.du	$vr0, $vr1, $vr2
vhsubw.hu.bu	$vr0, $vr1, $vr2
vhsubw.wu.hu	$vr0, $vr1, $vr2
vhsubw.du.wu	$vr0, $vr1, $vr2
vhsubw.qu.du	$vr0, $vr1, $vr2
vadda.b	$vr0, $vr1, $vr2
vadda.h	$vr0, $vr1, $vr2
vadda.w	$vr0, $vr1, $vr2
vadda.d	$vr0, $vr1, $vr2
vabsd.b	$vr0, $vr1, $vr2
vabsd.h	$vr0, $vr1, $vr2
vabsd.w	$vr0, $vr1, $vr2
vabsd.d	$vr0, $vr1, $vr2
vabsd.bu	$vr0, $vr1, $vr2
vabsd.hu	$vr0, $vr1, $vr2
vabsd.wu	$vr0, $vr1, $vr2
vabsd.du	$vr0, $vr1, $vr2
vavg.b	$vr0, $vr1, $vr2
vavg.h	$vr0, $vr1, $vr2
vavg.w	$vr0, $vr1, $vr2
vavg.d	$vr0, $vr1, $vr2
vavg.bu	$vr0, $vr1, $vr2
vavg.hu	$vr0, $vr1, $vr2
vavg.wu	$vr0, $vr1, $vr2
vavg.du	$vr0, $vr1, $vr2
vavgr.b	$vr0, $vr1, $vr2
vavgr.h	$vr0, $vr1, $vr2
vavgr.w	$vr0, $vr1, $vr2
vavgr.d	$vr0, $vr1, $vr2
vavgr.bu	$vr0, $vr1, $vr2
vavgr.hu	$vr0, $vr1, $vr2
vavgr.wu	$vr0, $vr1, $vr2
vavgr.du	$vr0, $vr1, $vr2
vmax.b	$vr0, $vr1, $vr2
vmax.h	$vr0, $vr1, $vr2
vmax.w	$vr0, $vr1, $vr2
vmax.d	$vr0, $vr1, $vr2
vmin.b	$vr0, $vr1, $vr2
vmin.h	$vr0, $vr1, $vr2
vmin.w	$vr0, $vr1, $vr2
vmin.d	$vr0, $vr1, $vr2
vmax.bu	$vr0, $vr1, $vr2
vmax.hu	$vr0, $vr1, $vr2
vmax.wu	$vr0, $vr1, $vr2
vmax.du	$vr0, $vr1, $vr2
vmin.bu	$vr0, $vr1, $vr2
vmin.hu	$vr0, $vr1, $vr2
vmin.wu	$vr0, $vr1, $vr2
vmin.du	$vr0, $vr1, $vr2
vmul.b	$vr0, $vr1, $vr2
vmul.h	$vr0, $vr1, $vr2
vmul.w	$vr0, $vr1, $vr2
vmul.d	$vr0, $vr1, $vr2
vmuh.b	$vr0, $vr1, $vr2
vmuh.h	$vr0, $vr1, $vr2
vmuh.w	$vr0, $vr1, $vr2
vmuh.d	$vr0, $vr1, $vr2
vmuh.bu	$vr0, $vr1, $vr2
vmuh.hu	$vr0, $vr1, $vr2
vmuh.wu	$vr0, $vr1, $vr2
vmuh.du	$vr0, $vr1, $vr2
vmadd.b	$vr0, $vr1, $vr2
vmadd.h	$vr0, $vr1, $vr2
vmadd.w	$vr0, $vr1, $vr2
vmadd.d	$vr0, $vr1, $vr2
vmsub.b	$vr0, $vr1, $vr2
vmsub.h	$vr0, $vr1, $vr2
vmsub.w	$vr0, $vr1, $vr2
vmsub.d	$vr0, $vr1, $vr2
vdiv.b	$vr0, $vr1, $vr2
vdiv.h	$vr0, $vr1, $vr2
vdiv.w	$vr0, $vr1, $vr2
vdiv.d	$vr0, $vr1, $vr2
vmod.b	$vr0, $vr1, $vr2
vmod.h	$vr0, $vr1, $vr2
vmod.w	$vr0, $vr1, $vr2
vmod.d	$vr0, $vr1, $vr2
vdiv.bu	$vr0, $vr1, $vr2
vdiv.hu	$vr0, $vr1, $vr2
vdiv.wu	$vr0, $vr1, $vr2
vdiv.du	$vr0, $vr1, $vr2
vmod.bu	$vr0, $vr1, $vr2
vmod.hu	$vr0, $vr1, $vr2
vmod.wu	$vr0, $vr1, $vr2
vmod.du	$vr0, $vr1, $vr2
vsll.b	$vr0, $vr1, $vr2
vsll.h	$vr0, $vr1, $vr2
vsll.w	$vr0, $vr1, $vr2
vsll.d	$vr0, $vr1, $vr2
vsrl.b	$vr0, $vr1, $vr2
vsrl.h	$vr0, $vr1, $vr2
vsrl.w	$vr0, $vr1, $vr2
vsrl.d	$vr0, $vr1, $vr2
vsra.b	$vr0, $vr1, $vr2
vsra.h	$vr0, $vr1, $vr2
vsra.w	$vr0, $vr1, $vr2
vsra.d	$vr0, $vr1, $vr2
vrotr.b	$vr0, $vr1, $vr2
vrotr.h	$vr0, $vr1, $vr2
vrotr.w	$vr0, $vr1, $vr2
vrotr.d	$vr0, $vr1, $vr2
vsrlr.b	$vr0, $vr1, $vr2
vsrlr.h	$vr0, $vr1, $vr2
vsrlr.w	$vr0, $vr1, $vr2
vsrlr.d	$vr0, $vr1, $vr2
vsrar.b	$vr0, $vr1, $vr2
vsrar.h	$vr0, $vr1, $vr2
vsrar.w	$vr0, $vr1, $vr2
vsrar.d	$vr0, $vr1, $vr2
vsrln.b.h	$vr0, $vr1, $vr2
vsrln.h.w	$vr0, $vr1, $vr2
vsrln.w.d	$vr0, $vr1, $vr2
vsran.b.h	$vr0, $vr1, $vr2
vsran.h.w	$vr0, $vr1, $vr2
vsran.w.d	$vr0, $vr1, $vr2
vsrlrn.b.h	$vr0, $vr1, $vr2
vsrlrn.h.w	$vr0, $vr1, $vr2
vsrlrn.w.d	$vr0, $vr1, $vr2
vsrarn.b.h	$vr0, $vr1, $vr2
vsrarn.h.w	$vr0, $vr1, $vr2
vsrarn.w.d	$vr0, $vr1, $vr2
vssrln.b.h	$vr0, $vr1, $vr2
vssrln.h.w	$vr0, $vr1, $vr2
vssrln.w.d	$vr0, $vr1, $vr2
vssran.b.h	$vr0, $vr1, $vr2
vssran.h.w	$vr0, $vr1, $vr2
vssran.w.d	$vr0, $vr1, $vr2
vssrlrn.b.h	$vr0, $vr1, $vr2
vssrlrn.h.w	$vr0, $vr1, $vr2
vssrlrn.w.d	$vr0, $vr1, $vr2
vssrarn.b.h	$vr0, $vr1, $vr2
vssrarn.h.w	$vr0, $vr1, $vr2
vssrarn.w.d	$vr0, $vr1, $vr2
vssrln.bu.h	$vr0, $vr1, $vr2
vssrln.hu.w	$vr0, $vr1, $vr2
vssrln.wu.d	$vr0, $vr1, $vr2
vssran.bu.h	$vr0, $vr1, $vr2
vssran.hu.w	$vr0, $vr1, $vr2
vssran.wu.d	$vr0, $vr1, $vr2
vssrlrn.bu.h	$vr0, $vr1, $vr2
vssrlrn.hu.w	$vr0, $vr1, $vr2
vssrlrn.wu.d	$vr0, $vr1, $vr2
vssrarn.bu.h	$vr0, $vr1, $vr2
vssrarn.hu.w	$vr0, $vr1, $vr2
vssrarn.wu.d	$vr0, $vr1, $vr2
vbitclr.b	$vr0, $vr1, $vr2
vbitclr.h	$vr0, $vr1, $vr2
vbitclr.w	$vr0, $vr1, $vr2
vbitclr.d	$vr0, $vr1, $vr2
vbitset.b	$vr0, $vr1, $vr2
vbitset.h	$vr0, $vr1, $vr2
vbitset.w	$vr0, $vr1, $vr2
vbitset.d	$vr0, $vr1, $vr2
vbitrev.b	$vr0, $vr1, $vr2
vbitrev.h	$vr0, $vr1, $vr2
vbitrev.w	$vr0, $vr1, $vr2
vbitrev.d	$vr0, $vr1, $vr2
vpackev.b	$vr0, $vr1, $vr2
vpackev.h	$vr0, $vr1, $vr2
vpackev.w	$vr0, $vr1, $vr2
vpackev.d	$vr0, $vr1, $vr2
vpackod.b	$vr0, $vr1, $vr2
vpackod.h	$vr0, $vr1, $vr2
vpackod.w	$vr0, $vr1, $vr2
vpackod.d	$vr0, $vr1, $vr2
vilvl.b	$vr0, $vr1, $vr2
vilvl.h	$vr0, $vr1, $vr2
vilvl.w	$vr0, $vr1, $vr2
vilvl.d	$vr0, $vr1, $vr2
vilvh.b	$vr0, $vr1, $vr2
vilvh.h	$vr0, $vr1, $vr2
vilvh.w	$vr0, $vr1, $vr2
vilvh.d	$vr0, $vr1, $vr2
vpickev.b	$vr0, $vr1, $vr2
vpickev.h	$vr0, $vr1, $vr2
vpickev.w	$vr0, $vr1, $vr2
vpickev.d	$vr0, $vr1, $vr2
vpickod.b	$vr0, $vr1, $vr2
vpickod.h	$vr0, $vr1, $vr2
vpickod.w	$vr0, $vr1, $vr2
vpickod.d	$vr0, $vr1, $vr2
vreplve.b	$vr0, $vr1, $r2
vreplve.h	$vr0, $vr1, $r2
vreplve.w	$vr0, $vr1, $r2
vreplve.d	$vr0, $vr1, $r2
vand.v	$vr0, $vr1, $vr2
vor.v	$vr0, $vr1, $vr2
vxor.v	$vr0, $vr1, $vr2
vnor.v	$vr0, $vr1, $vr2
vandn.v	$vr0, $vr1, $vr2
vorn.v	$vr0, $vr1, $vr2
vfrstp.b	$vr0, $vr1, $vr2
vfrstp.h	$vr0, $vr1, $vr2
vadd.q	$vr0, $vr1, $vr2
vsub.q	$vr0, $vr1, $vr2
vsigncov.b	$vr0, $vr1, $vr2
vsigncov.h	$vr0, $vr1, $vr2
vsigncov.w	$vr0, $vr1, $vr2
vsigncov.d	$vr0, $vr1, $vr2
vfadd.s	$vr0, $vr1, $vr2
vfadd.d	$vr0, $vr1, $vr2
vfsub.s	$vr0, $vr1, $vr2
vfsub.d	$vr0, $vr1, $vr2
vfmul.s	$vr0, $vr1, $vr2
vfmul.d	$vr0, $vr1, $vr2
vfdiv.s	$vr0, $vr1, $vr2
vfdiv.d	$vr0, $vr1, $vr2
vfmax.s	$vr0, $vr1, $vr2
vfmax.d	$vr0, $vr1, $vr2
vfmin.s	$vr0, $vr1, $vr2
vfmin.d	$vr0, $vr1, $vr2
vfmaxa.s	$vr0, $vr1, $vr2
vfmaxa.d	$vr0, $vr1, $vr2
vfmina.s	$vr0, $vr1, $vr2
vfmina.d	$vr0, $vr1, $vr2
vfcvt.h.s	$vr0, $vr1, $vr2
vfcvt.s.d	$vr0, $vr1, $vr2
vffint.s.l	$vr0, $vr1, $vr2
vftint.w.d	$vr0, $vr1, $vr2
vftintrm.w.d	$vr0, $vr1, $vr2
vftintrp.w.d	$vr0, $vr1, $vr2
vftintrz.w.d	$vr0, $vr1, $vr2
vftintrne.w.d	$vr0, $vr1, $vr2
vshuf.h	$vr0, $vr1, $vr2
vshuf.w	$vr0, $vr1, $vr2
vshuf.d	$vr0, $vr1, $vr2
vseqi.b	$vr0, $vr1, 1
vseqi.h	$vr0, $vr1, 1
vseqi.w	$vr0, $vr1, 1
vseqi.d	$vr0, $vr1, 1
vslei.b	$vr0, $vr1, 1
vslei.h	$vr0, $vr1, 1
vslei.w	$vr0, $vr1, 1
vslei.d	$vr0, $vr1, 1
vslei.bu	$vr0, $vr1, 1
vslei.hu	$vr0, $vr1, 1
vslei.wu	$vr0, $vr1, 1
vslei.du	$vr0, $vr1, 1
vslti.b	$vr0, $vr1, 1
vslti.h	$vr0, $vr1, 1
vslti.w	$vr0, $vr1, 1
vslti.d	$vr0, $vr1, 1
vslti.bu	$vr0, $vr1, 1
vslti.hu	$vr0, $vr1, 1
vslti.wu	$vr0, $vr1, 1
vslti.du	$vr0, $vr1, 1
vaddi.bu	$vr0, $vr1, 1
vaddi.hu	$vr0, $vr1, 1
vaddi.wu	$vr0, $vr1, 1
vaddi.du	$vr0, $vr1, 1
vsubi.bu	$vr0, $vr1, 1
vsubi.hu	$vr0, $vr1, 1
vsubi.wu	$vr0, $vr1, 1
vsubi.du	$vr0, $vr1, 1
vbsll.v	$vr0, $vr1, 1
vbsrl.v	$vr0, $vr1, 1
vmaxi.b	$vr0, $vr1, 1
vmaxi.h	$vr0, $vr1, 1
vmaxi.w	$vr0, $vr1, 1
vmaxi.d	$vr0, $vr1, 1
vmini.b	$vr0, $vr1, 1
vmini.h	$vr0, $vr1, 1
vmini.w	$vr0, $vr1, 1
vmini.d	$vr0, $vr1, 1
vmaxi.bu	$vr0, $vr1, 1
vmaxi.hu	$vr0, $vr1, 1
vmaxi.wu	$vr0, $vr1, 1
vmaxi.du	$vr0, $vr1, 1
vmini.bu	$vr0, $vr1, 1
vmini.hu	$vr0, $vr1, 1
vmini.wu	$vr0, $vr1, 1
vmini.du	$vr0, $vr1, 1
vfrstpi.b	$vr0, $vr1, 1
vfrstpi.h	$vr0, $vr1, 1
vclo.b	$vr0, $vr1
vclo.h	$vr0, $vr1
vclo.w	$vr0, $vr1
vclo.d	$vr0, $vr1
vclz.b	$vr0, $vr1
vclz.h	$vr0, $vr1
vclz.w	$vr0, $vr1
vclz.d	$vr0, $vr1
vpcnt.b	$vr0, $vr1
vpcnt.h	$vr0, $vr1
vpcnt.w	$vr0, $vr1
vpcnt.d	$vr0, $vr1
vneg.b	$vr0, $vr1
vneg.h	$vr0, $vr1
vneg.w	$vr0, $vr1
vneg.d	$vr0, $vr1
vmskltz.b	$vr0, $vr1
vmskltz.h	$vr0, $vr1
vmskltz.w	$vr0, $vr1
vmskltz.d	$vr0, $vr1
vmskgez.b	$vr0, $vr1
vmsknz.b	$vr0, $vr1
vseteqz.v	$fcc0, $vr1
vsetnez.v	$fcc0, $vr1
vsetanyeqz.b	$fcc0, $vr1
vsetanyeqz.h	$fcc0, $vr1
vsetanyeqz.w	$fcc0, $vr1
vsetanyeqz.d	$fcc0, $vr1
vsetallnez.b	$fcc0, $vr1
vsetallnez.h	$fcc0, $vr1
vsetallnez.w	$fcc0, $vr1
vsetallnez.d	$fcc0, $vr1
vflogb.s	$vr0, $vr1
vflogb.d	$vr0, $vr1
vfclass.s	$vr0, $vr1
vfclass.d	$vr0, $vr1
vfsqrt.s	$vr0, $vr1
vfsqrt.d	$vr0, $vr1
vfrecip.s	$vr0, $vr1
vfrecip.d	$vr0, $vr1
vfrsqrt.s	$vr0, $vr1
vfrsqrt.d	$vr0, $vr1
vfrint.s	$vr0, $vr1
vfrint.d	$vr0, $vr1
vfrintrm.s	$vr0, $vr1
vfrintrm.d	$vr0, $vr1
vfrintrp.s	$vr0, $vr1
vfrintrp.d	$vr0, $vr1
vfrintrz.s	$vr0, $vr1
vfrintrz.d	$vr0, $vr1
vfrintrne.s	$vr0, $vr1
vfrintrne.d	$vr0, $vr1
vfcvtl.s.h	$vr0, $vr1
vfcvth.s.h	$vr0, $vr1
vfcvtl.d.s	$vr0, $vr1
vfcvth.d.s	$vr0, $vr1
vffint.s.w	$vr0, $vr1
vffint.s.wu	$vr0, $vr1
vffint.d.l	$vr0, $vr1
vffint.d.lu	$vr0, $vr1
vffintl.d.w	$vr0, $vr1
vffinth.d.w	$vr0, $vr1
vftint.w.s	$vr0, $vr1
vftint.l.d	$vr0, $vr1
vftintrm.w.s	$vr0, $vr1
vftintrm.l.d	$vr0, $vr1
vftintrp.w.s	$vr0, $vr1
vftintrp.l.d	$vr0, $vr1
vftintrz.w.s	$vr0, $vr1
vftintrz.l.d	$vr0, $vr1
vftintrne.w.s	$vr0, $vr1
vftintrne.l.d	$vr0, $vr1
vftint.wu.s	$vr0, $vr1
vftint.lu.d	$vr0, $vr1
vftintrz.wu.s	$vr0, $vr1
vftintrz.lu.d	$vr0, $vr1
vftintl.l.s	$vr0, $vr1
vftinth.l.s	$vr0, $vr1
vftintrml.l.s	$vr0, $vr1
vftintrmh.l.s	$vr0, $vr1
vftintrpl.l.s	$vr0, $vr1
vftintrph.l.s	$vr0, $vr1
vftintrzl.l.s	$vr0, $vr1
vftintrzh.l.s	$vr0, $vr1
vftintrnel.l.s	$vr0, $vr1
vftintrneh.l.s	$vr0, $vr1
vexth.h.b	$vr0, $vr1
vexth.w.h	$vr0, $vr1
vexth.d.w	$vr0, $vr1
vexth.q.d	$vr0, $vr1
vexth.hu.bu	$vr0, $vr1
vexth.wu.hu	$vr0, $vr1
vexth.du.wu	$vr0, $vr1
vexth.qu.du	$vr0, $vr1
vreplgr2vr.b	$vr0, $r1
vreplgr2vr.h	$vr0, $r1
vreplgr2vr.w	$vr0, $r1
vreplgr2vr.d	$vr0, $r1
vrotri.b	$vr0, $vr1, 1
vrotri.h	$vr0, $vr1, 1
vrotri.w	$vr0, $vr1, 1
vrotri.d	$vr0, $vr1, 1
vsrlri.b	$vr0, $vr1, 1
vsrlri.h	$vr0, $vr1, 1
vsrlri.w	$vr0, $vr1, 1
vsrlri.d	$vr0, $vr1, 1
vsrari.b	$vr0, $vr1, 1
vsrari.h	$vr0, $vr1, 1
vsrari.w	$vr0, $vr1, 1
vsrari.d	$vr0, $vr1, 1
vinsgr2vr.b	$vr0, $r1, 1
vinsgr2vr.h	$vr0, $r1, 1
vinsgr2vr.w	$vr0, $r1, 1
vinsgr2vr.d	$vr0, $r1, 1
vpickve2gr.b	$r0, $vr1, 1
vpickve2gr.h	$r0, $vr1, 1
vpickve2gr.w	$r0, $vr1, 1
vpickve2gr.d	$r0, $vr1, 1
vpickve2gr.bu	$r0, $vr1, 1
vpickve2gr.hu	$r0, $vr1, 1
vpickve2gr.wu	$r0, $vr1, 1
vpickve2gr.du	$r0, $vr1, 1
vreplvei.b	$vr0, $vr1, 1
vreplvei.h	$vr0, $vr1, 1
vreplvei.w	$vr0, $vr1, 1
vreplvei.d	$vr0, $vr1, 1
vsllwil.h.b	$vr0, $vr1, 1
vsllwil.w.h	$vr0, $vr1, 1
vsllwil.d.w	$vr0, $vr1, 1
vextl.q.d	$vr0, $vr1
vsllwil.hu.bu	$vr0, $vr1, 1
vsllwil.wu.hu	$vr0, $vr1, 1
vsllwil.du.wu	$vr0, $vr1, 1
vextl.qu.du	$vr0, $vr1
vbitclri.b	$vr0, $vr1, 1
vbitclri.h	$vr0, $vr1, 1
vbitclri.w	$vr0, $vr1, 1
vbitclri.d	$vr0, $vr1, 1
vbitseti.b	$vr0, $vr1, 1
vbitseti.h	$vr0, $vr1, 1
vbitseti.w	$vr0, $vr1, 1
vbitseti.d	$vr0, $vr1, 1
vbitrevi.b	$vr0, $vr1, 1
vbitrevi.h	$vr0, $vr1, 1
vbitrevi.w	$vr0, $vr1, 1
vbitrevi.d	$vr0, $vr1, 1
vsat.b	$vr0, $vr1, 1
vsat.h	$vr0, $vr1, 1
vsat.w	$vr0, $vr1, 1
vsat.d	$vr0, $vr1, 1
vsat.bu	$vr0, $vr1, 1
vsat.hu	$vr0, $vr1, 1
vsat.wu	$vr0, $vr1, 1
vsat.du	$vr0, $vr1, 1
vslli.b	$vr0, $vr1, 1
vslli.h	$vr0, $vr1, 1
vslli.w	$vr0, $vr1, 1
vslli.d	$vr0, $vr1, 1
vsrli.b	$vr0, $vr1, 1
vsrli.h	$vr0, $vr1, 1
vsrli.w	$vr0, $vr1, 1
vsrli.d	$vr0, $vr1, 1
vsrai.b	$vr0, $vr1, 1
vsrai.h	$vr0, $vr1, 1
vsrai.w	$vr0, $vr1, 1
vsrai.d	$vr0, $vr1, 1
vsrlni.b.h	$vr0, $vr1, 1
vsrlni.h.w	$vr0, $vr1, 1
vsrlni.w.d	$vr0, $vr1, 1
vsrlni.d.q	$vr0, $vr1, 1
vssrlni.b.h	$vr0, $vr1, 1
vssrlni.h.w	$vr0, $vr1, 1
vssrlni.w.d	$vr0, $vr1, 1
vssrlni.d.q	$vr0, $vr1, 1
vsrlrni.b.h	$vr0, $vr1, 1
vsrlrni.h.w	$vr0, $vr1, 1
vsrlrni.w.d	$vr0, $vr1, 1
vsrlrni.d.q	$vr0, $vr1, 1
vssrlni.bu.h	$vr0, $vr1, 1
vssrlni.hu.w	$vr0, $vr1, 1
vssrlni.wu.d	$vr0, $vr1, 1
vssrlni.du.q	$vr0, $vr1, 1
vssrlrni.b.h	$vr0, $vr1, 1
vssrlrni.h.w	$vr0, $vr1, 1
vssrlrni.w.d	$vr0, $vr1, 1
vssrlrni.d.q	$vr0, $vr1, 1
vssrlrni.bu.h	$vr0, $vr1, 1
vssrlrni.hu.w	$vr0, $vr1, 1
vssrlrni.wu.d	$vr0, $vr1, 1
vssrlrni.du.q	$vr0, $vr1, 1
vsrani.b.h	$vr0, $vr1, 1
vsrani.h.w	$vr0, $vr1, 1
vsrani.w.d	$vr0, $vr1, 1
vsrani.d.q	$vr0, $vr1, 1
vsrarni.b.h	$vr0, $vr1, 1
vsrarni.h.w	$vr0, $vr1, 1
vsrarni.w.d	$vr0, $vr1, 1
vsrarni.d.q	$vr0, $vr1, 1
vssrani.b.h	$vr0, $vr1, 1
vssrani.h.w	$vr0, $vr1, 1
vssrani.w.d	$vr0, $vr1, 1
vssrani.d.q	$vr0, $vr1, 1
vssrani.bu.h	$vr0, $vr1, 1
vssrani.hu.w	$vr0, $vr1, 1
vssrani.wu.d	$vr0, $vr1, 1
vssrani.du.q	$vr0, $vr1, 1
vssrarni.b.h	$vr0, $vr1, 1
vssrarni.h.w	$vr0, $vr1, 1
vssrarni.w.d	$vr0, $vr1, 1
vssrarni.d.q	$vr0, $vr1, 1
vssrarni.bu.h	$vr0, $vr1, 1
vssrarni.hu.w	$vr0, $vr1, 1
vssrarni.wu.d	$vr0, $vr1, 1
vssrarni.du.q	$vr0, $vr1, 1
vextrins.d	$vr0, $vr1, 1
vextrins.w	$vr0, $vr1, 1
vextrins.h	$vr0, $vr1, 1
vextrins.b	$vr0, $vr1, 1
vshuf4i.b	$vr0, $vr1, 1
vshuf4i.h	$vr0, $vr1, 1
vshuf4i.w	$vr0, $vr1, 1
vshuf4i.d	$vr0, $vr1, 1
vbitseli.b	$vr0, $vr1, 1
vandi.b	$vr0, $vr1, 1
vori.b	$vr0, $vr1, 1
vxori.b	$vr0, $vr1, 1
vnori.b	$vr0, $vr1, 1
vrepli.b	$vr0, 1
vaddwev.h.b	$vr0, $vr1, $vr2
vaddwev.w.h	$vr0, $vr1, $vr2
vaddwev.d.w	$vr0, $vr1, $vr2
vaddwev.q.d	$vr0, $vr1, $vr2
vaddwev.h.bu	$vr0, $vr1, $vr2
vaddwev.w.hu	$vr0, $vr1, $vr2
vaddwev.d.wu	$vr0, $vr1, $vr2
vaddwev.q.du	$vr0, $vr1, $vr2
vaddwev.h.bu.b	$vr0, $vr1, $vr2
vaddwev.w.hu.h	$vr0, $vr1, $vr2
vaddwev.d.wu.w	$vr0, $vr1, $vr2
vaddwev.q.du.d	$vr0, $vr1, $vr2
vaddwod.h.b	$vr0, $vr1, $vr2
vaddwod.w.h	$vr0, $vr1, $vr2
vaddwod.d.w	$vr0, $vr1, $vr2
vaddwod.q.d	$vr0, $vr1, $vr2
vaddwod.h.bu	$vr0, $vr1, $vr2
vaddwod.w.hu	$vr0, $vr1, $vr2
vaddwod.d.wu	$vr0, $vr1, $vr2
vaddwod.q.du	$vr0, $vr1, $vr2
vaddwod.h.bu.b	$vr0, $vr1, $vr2
vaddwod.w.hu.h	$vr0, $vr1, $vr2
vaddwod.d.wu.w	$vr0, $vr1, $vr2
vaddwod.q.du.d	$vr0, $vr1, $vr2
vmaddwev.h.b	$vr0, $vr1, $vr2
vmaddwev.w.h	$vr0, $vr1, $vr2
vmaddwev.d.w	$vr0, $vr1, $vr2
vmaddwev.q.d	$vr0, $vr1, $vr2
vmaddwev.h.bu	$vr0, $vr1, $vr2
vmaddwev.w.hu	$vr0, $vr1, $vr2
vmaddwev.d.wu	$vr0, $vr1, $vr2
vmaddwev.q.du	$vr0, $vr1, $vr2
vmaddwev.h.bu.b	$vr0, $vr1, $vr2
vmaddwev.w.hu.h	$vr0, $vr1, $vr2
vmaddwev.d.wu.w	$vr0, $vr1, $vr2
vmaddwev.q.du.d	$vr0, $vr1, $vr2
vmaddwod.h.b	$vr0, $vr1, $vr2
vmaddwod.w.h	$vr0, $vr1, $vr2
vmaddwod.d.w	$vr0, $vr1, $vr2
vmaddwod.q.d	$vr0, $vr1, $vr2
vmaddwod.h.bu	$vr0, $vr1, $vr2
vmaddwod.w.hu	$vr0, $vr1, $vr2
vmaddwod.d.wu	$vr0, $vr1, $vr2
vmaddwod.q.du	$vr0, $vr1, $vr2
vmaddwod.h.bu.b	$vr0, $vr1, $vr2
vmaddwod.w.hu.h	$vr0, $vr1, $vr2
vmaddwod.d.wu.w	$vr0, $vr1, $vr2
vmaddwod.q.du.d	$vr0, $vr1, $vr2
vmulwev.h.b	$vr0, $vr1, $vr2
vmulwev.w.h	$vr0, $vr1, $vr2
vmulwev.d.w	$vr0, $vr1, $vr2
vmulwev.q.d	$vr0, $vr1, $vr2
vmulwev.h.bu	$vr0, $vr1, $vr2
vmulwev.w.hu	$vr0, $vr1, $vr2
vmulwev.d.wu	$vr0, $vr1, $vr2
vmulwev.q.du	$vr0, $vr1, $vr2
vmulwev.h.bu.b	$vr0, $vr1, $vr2
vmulwev.w.hu.h	$vr0, $vr1, $vr2
vmulwev.d.wu.w	$vr0, $vr1, $vr2
vmulwev.q.du.d	$vr0, $vr1, $vr2
vmulwod.h.b	$vr0, $vr1, $vr2
vmulwod.w.h	$vr0, $vr1, $vr2
vmulwod.d.w	$vr0, $vr1, $vr2
vmulwod.q.d	$vr0, $vr1, $vr2
vmulwod.h.bu	$vr0, $vr1, $vr2
vmulwod.w.hu	$vr0, $vr1, $vr2
vmulwod.d.wu	$vr0, $vr1, $vr2
vmulwod.q.du	$vr0, $vr1, $vr2
vmulwod.h.bu.b	$vr0, $vr1, $vr2
vmulwod.w.hu.h	$vr0, $vr1, $vr2
vmulwod.d.wu.w	$vr0, $vr1, $vr2
vmulwod.q.du.d	$vr0, $vr1, $vr2
vsubwev.h.b	$vr0, $vr1, $vr2
vsubwev.w.h	$vr0, $vr1, $vr2
vsubwev.d.w	$vr0, $vr1, $vr2
vsubwev.q.d	$vr0, $vr1, $vr2
vsubwev.h.bu	$vr0, $vr1, $vr2
vsubwev.w.hu	$vr0, $vr1, $vr2
vsubwev.d.wu	$vr0, $vr1, $vr2
vsubwev.q.du	$vr0, $vr1, $vr2
vsubwod.h.b	$vr0, $vr1, $vr2
vsubwod.w.h	$vr0, $vr1, $vr2
vsubwod.d.w	$vr0, $vr1, $vr2
vsubwod.q.d	$vr0, $vr1, $vr2
vsubwod.h.bu	$vr0, $vr1, $vr2
vsubwod.w.hu	$vr0, $vr1, $vr2
vsubwod.d.wu	$vr0, $vr1, $vr2
vsubwod.q.du	$vr0, $vr1, $vr2
vrepli.d	$vr0, 1
vrepli.h	$vr0, 1
vrepli.w	$vr0, 1
vldi	$vr0, 1
vpermi.w	$vr0, $vr1, 1
vext2xv.h.b	$xr0, $xr1
vext2xv.w.b	$xr0, $xr1
vext2xv.d.b	$xr0, $xr1
vext2xv.w.h	$xr0, $xr1
vext2xv.d.h	$xr0, $xr1
vext2xv.d.w	$xr0, $xr1
vext2xv.hu.bu	$xr0, $xr1
vext2xv.wu.bu	$xr0, $xr1
vext2xv.du.bu	$xr0, $xr1
vext2xv.wu.hu	$xr0, $xr1
vext2xv.du.hu	$xr0, $xr1
vext2xv.du.wu	$xr0, $xr1
vfrecipe.s	$vr0, $vr1
vfrecipe.d	$vr0, $vr1
vfrsqrte.s	$vr0, $vr1
vfrsqrte.d	$vr0, $vr1
