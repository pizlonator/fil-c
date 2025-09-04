#source: armv9_5-a-sysregs.s
#as: -march=armv9.5-a -I$srcdir/$subdir
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
.*:	d51ec120 	msr	vdisr_el3, x0
.*:	d53ec120 	mrs	x0, vdisr_el3
.*:	d51e5260 	msr	vsesr_el3, x0
.*:	d53e5260 	mrs	x0, vsesr_el3
.*:	d5139c80 	msr	spmzr_el0, x0
.*:	d5339c80 	mrs	x0, spmzr_el0
.*:	d5100540 	msr	mdstepop_el1, x0
.*:	d5300540 	mrs	x0, mdstepop_el1
