#name: Test of SVE2.1 extq instructions.
#as: -march=armv9.4-a
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	05602400 	extq	z0.b, z0.b, z0.b, #0
.*:	0560241f 	extq	z31.b, z31.b, z0.b, #0
.*:	056027e0 	extq	z0.b, z0.b, z31.b, #0
.*:	056f2400 	extq	z0.b, z0.b, z0.b, #15
.*:	056f27ff 	extq	z31.b, z31.b, z31.b, #15
.*:	056727ef 	extq	z15.b, z15.b, z31.b, #7
.*:	0420bca3 	movprfx	z3, z5
.*:	05602403 	extq	z3.b, z3.b, z0.b, #0
.*:	0420bd5f 	movprfx	z31, z10
.*:	056f241f 	extq	z31.b, z31.b, z0.b, #15
