#source: sysreg128.s
#as: -I$srcdir/$subdir
#objdump: -dr

.*


Disassembly of section \.text:

0+ <\.text>:
.*:	d5587402 	msrr	par_el1, x2, x3
.*:	d5787402 	mrrs	x2, x3, par_el1
.*:	d558d0c2 	msrr	rcwmask_el1, x2, x3
.*:	d578d0c2 	mrrs	x2, x3, rcwmask_el1
.*:	d558d062 	msrr	rcwsmask_el1, x2, x3
.*:	d578d062 	mrrs	x2, x3, rcwsmask_el1
.*:	d5582002 	msrr	ttbr0_el1, x2, x3
.*:	d5782002 	mrrs	x2, x3, ttbr0_el1
.*:	d55d2002 	msrr	ttbr0_el12, x2, x3
.*:	d57d2002 	mrrs	x2, x3, ttbr0_el12
.*:	d55c2002 	msrr	ttbr0_el2, x2, x3
.*:	d57c2002 	mrrs	x2, x3, ttbr0_el2
.*:	d5582022 	msrr	ttbr1_el1, x2, x3
.*:	d5782022 	mrrs	x2, x3, ttbr1_el1
.*:	d55d2022 	msrr	ttbr1_el12, x2, x3
.*:	d57d2022 	mrrs	x2, x3, ttbr1_el12
.*:	d55c2022 	msrr	ttbr1_el2, x2, x3
.*:	d57c2022 	mrrs	x2, x3, ttbr1_el2
.*:	d55c2102 	msrr	vttbr_el2, x2, x3
.*:	d57c2102 	mrrs	x2, x3, vttbr_el2
