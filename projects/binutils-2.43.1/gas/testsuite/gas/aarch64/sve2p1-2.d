#name: Test of SVE2.1 dupq instructions.
#as: -march=armv9.4-a
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	05212400 	dupq	z0.b, z0.b\[0\]
.*:	0521241f 	dupq	z31.b, z0.b\[0\]
.*:	052127e0 	dupq	z0.b, z31.b\[0\]
.*:	053f2400 	dupq	z0.b, z0.b\[15\]
.*:	053f27ff 	dupq	z31.b, z31.b\[15\]
.*:	052925e7 	dupq	z7.b, z15.b\[4\]
.*:	05222400 	dupq	z0.h, z0.h\[0\]
.*:	0522241f 	dupq	z31.h, z0.h\[0\]
.*:	052227e0 	dupq	z0.h, z31.h\[0\]
.*:	053e2400 	dupq	z0.h, z0.h\[7\]
.*:	053e27ff 	dupq	z31.h, z31.h\[7\]
.*:	053225e7 	dupq	z7.h, z15.h\[4\]
.*:	05242400 	dupq	z0.s, z0.s\[0\]
.*:	0524241f 	dupq	z31.s, z0.s\[0\]
.*:	052427e0 	dupq	z0.s, z31.s\[0\]
.*:	053c2400 	dupq	z0.s, z0.s\[3\]
.*:	053c27ff 	dupq	z31.s, z31.s\[3\]
.*:	053425e7 	dupq	z7.s, z15.s\[2\]
.*:	05282400 	dupq	z0.d, z0.d\[0\]
.*:	0528241f 	dupq	z31.d, z0.d\[0\]
.*:	052827e0 	dupq	z0.d, z31.d\[0\]
.*:	05382400 	dupq	z0.d, z0.d\[1\]
.*:	053827ff 	dupq	z31.d, z31.d\[1\]
.*:	052825e7 	dupq	z7.d, z15.d\[0\]
