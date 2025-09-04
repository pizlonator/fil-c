#source: sysreg.s
#as: -I$srcdir/$subdir
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
.*:	d51b9c60 	msr	pmovsclr_el0, x0
.*:	d53b9c60 	mrs	x0, pmovsclr_el0
.*:	d51b9e60 	msr	pmovsset_el0, x0
.*:	d53b9e60 	mrs	x0, pmovsset_el0
.*:	d5380580 	mrs	x0, id_aa64afr0_el1
.*:	d53805a0 	mrs	x0, id_aa64afr1_el1
.*:	d5380500 	mrs	x0, id_aa64dfr0_el1
.*:	d5380520 	mrs	x0, id_aa64dfr1_el1
.*:	d5380540 	mrs	x0, id_aa64dfr2_el1
.*:	d5380140 	mrs	x0, id_dfr0_el1
.*:	d5380100 	mrs	x0, id_pfr0_el1
.*:	d5380120 	mrs	x0, id_pfr1_el1
.*:	d5380160 	mrs	x0, id_afr0_el1
.*:	d5380180 	mrs	x0, id_mmfr0_el1
.*:	d53801a0 	mrs	x0, id_mmfr1_el1
.*:	d53801c0 	mrs	x0, id_mmfr2_el1
.*:	d53801e0 	mrs	x0, id_mmfr3_el1
.*:	d53802c0 	mrs	x0, id_mmfr4_el1
.*:	d5380200 	mrs	x0, id_isar0_el1
.*:	d5380220 	mrs	x0, id_isar1_el1
.*:	d5380240 	mrs	x0, id_isar2_el1
.*:	d5380260 	mrs	x0, id_isar3_el1
.*:	d5380280 	mrs	x0, id_isar4_el1
.*:	d53802a0 	mrs	x0, id_isar5_el1
.*:	d5380640 	mrs	x0, id_aa64isar2_el1
.*:	d538065e 	mrs	x30, id_aa64isar2_el1
.*:	d5380660 	mrs	x0, id_aa64isar3_el1
.*:	d538067e 	mrs	x30, id_aa64isar3_el1
.*:	d538cf00 	mrs	x0, s3_0_c12_c15_0
.*:	d5184b00 	msr	s3_0_c4_c11_0, x0
.*:	d5384b00 	mrs	x0, s3_0_c4_c11_0
.*:	d5110300 	msr	trcstatr, x0
.*:	d5310300 	mrs	x0, trcstatr
