#name: Test of SVE2.1 orqv instruction.
#as: -march=armv9.4-a
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	041c2000 	orqv	v0.16b, p0, z0.b
.*:	041c201f 	orqv	v31.16b, p0, z0.b
.*:	04dc2000 	orqv	v0.2d, p0, z0.d
.*:	041c3c00 	orqv	v0.16b, p7, z0.b
.*:	041c23e0 	orqv	v0.16b, p0, z31.b
.*:	04dc3fff 	orqv	v31.2d, p7, z31.d
.*:	045c35ef 	orqv	v15.8h, p5, z15.h
.*:	049c2e8a 	orqv	v10.4s, p3, z20.s
