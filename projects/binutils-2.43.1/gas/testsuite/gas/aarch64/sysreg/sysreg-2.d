#source: sysreg-2.s
#as: -march=armv8.2-a+profile -I$srcdir/$subdir
#objdump: -dr

.*:     file .*


Disassembly of section .text:

0+ <.*>:
.*:	d5380720 	mrs	x0, id_aa64mmfr1_el1
.*:	d5380740 	mrs	x0, id_aa64mmfr2_el1
.*:	d5380760 	mrs	x0, id_aa64mmfr3_el1
.*:	d5380780 	mrs	x0, id_aa64mmfr4_el1
.*:	d5385300 	mrs	x0, erridr_el1
.*:	d5185320 	msr	errselr_el1, x0
.*:	d5385320 	mrs	x0, errselr_el1
.*:	d5385400 	mrs	x0, erxfr_el1
.*:	d5185420 	msr	erxctlr_el1, x0
.*:	d5385420 	mrs	x0, erxctlr_el1
.*:	d5185440 	msr	erxstatus_el1, x0
.*:	d5385440 	mrs	x0, erxstatus_el1
.*:	d5185460 	msr	erxaddr_el1, x0
.*:	d5385460 	mrs	x0, erxaddr_el1
.*:	d5185500 	msr	erxmisc0_el1, x0
.*:	d5385500 	mrs	x0, erxmisc0_el1
.*:	d5185520 	msr	erxmisc1_el1, x0
.*:	d5385520 	mrs	x0, erxmisc1_el1
.*:	d53c5260 	mrs	x0, vsesr_el2
.*:	d518c120 	msr	disr_el1, x0
.*:	d538c120 	mrs	x0, disr_el1
.*:	d53cc120 	mrs	x0, vdisr_el2
.*:	d50b7a20 	dc	cvac, x0
.*:	d50b7b21 	dc	cvau, x1
.*:	d50b7c22 	dc	cvap, x2
.*:	d5087900 	at	s1e1rp, x0
.*:	d5087921 	at	s1e1wp, x1
.*:	d5189a00 	msr	pmblimitr_el1, x0
.*:	d5389a00 	mrs	x0, pmblimitr_el1
.*:	d5189a20 	msr	pmbptr_el1, x0
.*:	d5389a20 	mrs	x0, pmbptr_el1
.*:	d5189a60 	msr	pmbsr_el1, x0
.*:	d5389a60 	mrs	x0, pmbsr_el1
.*:	d5189900 	msr	pmscr_el1, x0
.*:	d5389900 	mrs	x0, pmscr_el1
.*:	d5189940 	msr	pmsicr_el1, x0
.*:	d5389940 	mrs	x0, pmsicr_el1
.*:	d5189960 	msr	pmsirr_el1, x0
.*:	d5389960 	mrs	x0, pmsirr_el1
.*:	d5189980 	msr	pmsfcr_el1, x0
.*:	d5389980 	mrs	x0, pmsfcr_el1
.*:	d51899a0 	msr	pmsevfr_el1, x0
.*:	d53899a0 	mrs	x0, pmsevfr_el1
.*:	d51899c0 	msr	pmslatfr_el1, x0
.*:	d53899c0 	mrs	x0, pmslatfr_el1
.*:	d51c9900 	msr	pmscr_el2, x0
.*:	d53c9900 	mrs	x0, pmscr_el2
.*:	d51d9900 	msr	pmscr_el12, x0
.*:	d53d9900 	mrs	x0, pmscr_el12
.*:	d5389ae0 	mrs	x0, pmbidr_el1
.*:	d53899e0 	mrs	x0, pmsidr_el1
