#source: sysreg-3.s
#as: -march=armv8.3-a -I$srcdir/$subdir
#objdump: -dr

.*:     file .*

Disassembly of section \.text:

0+ <.*>:
.*:	d5182100 	msr	apiakeylo_el1, x0
.*:	d5382100 	mrs	x0, apiakeylo_el1
.*:	d5182120 	msr	apiakeyhi_el1, x0
.*:	d5382120 	mrs	x0, apiakeyhi_el1
.*:	d5182140 	msr	apibkeylo_el1, x0
.*:	d5382140 	mrs	x0, apibkeylo_el1
.*:	d5182160 	msr	apibkeyhi_el1, x0
.*:	d5382160 	mrs	x0, apibkeyhi_el1
.*:	d5182200 	msr	apdakeylo_el1, x0
.*:	d5382200 	mrs	x0, apdakeylo_el1
.*:	d5182220 	msr	apdakeyhi_el1, x0
.*:	d5382220 	mrs	x0, apdakeyhi_el1
.*:	d5182240 	msr	apdbkeylo_el1, x0
.*:	d5382240 	mrs	x0, apdbkeylo_el1
.*:	d5182260 	msr	apdbkeyhi_el1, x0
.*:	d5382260 	mrs	x0, apdbkeyhi_el1
.*:	d5182300 	msr	apgakeylo_el1, x0
.*:	d5382300 	mrs	x0, apgakeylo_el1
.*:	d5182320 	msr	apgakeyhi_el1, x0
.*:	d5382320 	mrs	x0, apgakeyhi_el1
