#name: Test that fpmr register is gated and available via the fp8 feature
#source: fpmr.s
#as: -march=armv9.2-a+fp8
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
   0:	d53b4440 	mrs	x0, fpmr
   4:	d51b4440 	msr	fpmr, x0
