#name: Test of SVE2.1 instructions
#as: -march=armv9.4-a
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	04052000 	addqv	v0.16b, p0, z0.b
.*:	0405201f 	addqv	v31.16b, p0, z0.b
.*:	04c52000 	addqv	v0.2d, p0, z0.d
.*:	04053c00 	addqv	v0.16b, p7, z0.b
.*:	040523e0 	addqv	v0.16b, p0, z31.b
.*:	04c53fff 	addqv	v31.2d, p7, z31.d
.*:	04852e8a 	addqv	v10.4s, p3, z20.s
.*:	041e2000 	andqv	v0.16b, p0, z0.b
.*:	041e201f 	andqv	v31.16b, p0, z0.b
.*:	04de2000 	andqv	v0.2d, p0, z0.d
.*:	041e3c00 	andqv	v0.16b, p7, z0.b
.*:	041e23e0 	andqv	v0.16b, p0, z31.b
.*:	04de3fff 	andqv	v31.2d, p7, z31.d
.*:	049e2e8a 	andqv	v10.4s, p3, z20.s
.*:	040c2000 	smaxqv	v0.16b, p0, z0.b
.*:	040c201f 	smaxqv	v31.16b, p0, z0.b
.*:	04cc2000 	smaxqv	v0.2d, p0, z0.d
.*:	040c3c00 	smaxqv	v0.16b, p7, z0.b
.*:	040c23e0 	smaxqv	v0.16b, p0, z31.b
.*:	04cc3fff 	smaxqv	v31.2d, p7, z31.d
.*:	048c2e8a 	smaxqv	v10.4s, p3, z20.s
.*:	040d2000 	umaxqv	v0.16b, p0, z0.b
.*:	040d201f 	umaxqv	v31.16b, p0, z0.b
.*:	04cd2000 	umaxqv	v0.2d, p0, z0.d
.*:	040d3c00 	umaxqv	v0.16b, p7, z0.b
.*:	040d23e0 	umaxqv	v0.16b, p0, z31.b
.*:	04cd3fff 	umaxqv	v31.2d, p7, z31.d
.*:	048d2e8a 	umaxqv	v10.4s, p3, z20.s
.*:	040e2000 	sminqv	v0.16b, p0, z0.b
.*:	040e201f 	sminqv	v31.16b, p0, z0.b
.*:	04ce2000 	sminqv	v0.2d, p0, z0.d
.*:	040e3c00 	sminqv	v0.16b, p7, z0.b
.*:	040e23e0 	sminqv	v0.16b, p0, z31.b
.*:	04ce3fff 	sminqv	v31.2d, p7, z31.d
.*:	048e2e8a 	sminqv	v10.4s, p3, z20.s
.*:	0521268a 	dupq	z10.b, z20.b\[0\]
.*:	053f268a 	dupq	z10.b, z20.b\[15\]
.*:	0522268a 	dupq	z10.h, z20.h\[0\]
.*:	053e268a 	dupq	z10.h, z20.h\[7\]
.*:	0524268a 	dupq	z10.s, z20.s\[0\]
.*:	053c268a 	dupq	z10.s, z20.s\[3\]
.*:	0528268a 	dupq	z10.d, z20.d\[0\]
.*:	0538268a 	dupq	z10.d, z20.d\[1\]
.*:	040f2000 	uminqv	v0.16b, p0, z0.b
.*:	040f201f 	uminqv	v31.16b, p0, z0.b
.*:	04cf2000 	uminqv	v0.2d, p0, z0.d
.*:	040f3c00 	uminqv	v0.16b, p7, z0.b
.*:	040f23e0 	uminqv	v0.16b, p0, z31.b
.*:	04cf3fff 	uminqv	v31.2d, p7, z31.d
.*:	048f2e8a 	uminqv	v10.4s, p3, z20.s
.*:	05602400 	extq	z0.b, z0.b, z0.b, #0
.*:	0560241f 	extq	z31.b, z31.b, z0.b, #0
.*:	056027e0 	extq	z0.b, z0.b, z31.b, #0
.*:	056f2400 	extq	z0.b, z0.b, z0.b, #15
.*:	056f27ff 	extq	z31.b, z31.b, z31.b, #15
.*:	056727ef 	extq	z15.b, z15.b, z31.b, #7
.*:	041d2000 	eorqv	v0.16b, p0, z0.b
.*:	041d201f 	eorqv	v31.16b, p0, z0.b
.*:	04dd2000 	eorqv	v0.2d, p0, z0.d
.*:	041d3c00 	eorqv	v0.16b, p7, z0.b
.*:	041d23e0 	eorqv	v0.16b, p0, z31.b
.*:	04dd3fff 	eorqv	v31.2d, p7, z31.d
.*:	049d2e8a 	eorqv	v10.4s, p3, z20.s
.*:	6450a000 	faddqv	v0.8h, p0, z0.h
.*:	6450a01f 	faddqv	v31.8h, p0, z0.h
.*:	64d0a000 	faddqv	v0.2d, p0, z0.d
.*:	6450bc00 	faddqv	v0.8h, p7, z0.h
.*:	6450a3e0 	faddqv	v0.8h, p0, z31.h
.*:	64d0bfff 	faddqv	v31.2d, p7, z31.d
.*:	6490ae8a 	faddqv	v10.4s, p3, z20.s
.*:	6454a000 	fmaxnmqv	v0.8h, p0, z0.h
.*:	6454a01f 	fmaxnmqv	v31.8h, p0, z0.h
.*:	64d4a000 	fmaxnmqv	v0.2d, p0, z0.d
.*:	6454bc00 	fmaxnmqv	v0.8h, p7, z0.h
.*:	6454a3e0 	fmaxnmqv	v0.8h, p0, z31.h
.*:	64d4bfff 	fmaxnmqv	v31.2d, p7, z31.d
.*:	6494ae8a 	fmaxnmqv	v10.4s, p3, z20.s
.*:	6456a000 	fmaxqv	v0.8h, p0, z0.h
.*:	6456a01f 	fmaxqv	v31.8h, p0, z0.h
.*:	64d6a000 	fmaxqv	v0.2d, p0, z0.d
.*:	6456bc00 	fmaxqv	v0.8h, p7, z0.h
.*:	6456a3e0 	fmaxqv	v0.8h, p0, z31.h
.*:	64d6bfff 	fmaxqv	v31.2d, p7, z31.d
.*:	6496ae8a 	fmaxqv	v10.4s, p3, z20.s
.*:	6455a000 	fminnmqv	v0.8h, p0, z0.h
.*:	6455a01f 	fminnmqv	v31.8h, p0, z0.h
.*:	64d5a000 	fminnmqv	v0.2d, p0, z0.d
.*:	6455bc00 	fminnmqv	v0.8h, p7, z0.h
.*:	6455a3e0 	fminnmqv	v0.8h, p0, z31.h
.*:	64d5bfff 	fminnmqv	v31.2d, p7, z31.d
.*:	6495ae8a 	fminnmqv	v10.4s, p3, z20.s
.*:	6457a000 	fminqv	v0.8h, p0, z0.h
.*:	6457a01f 	fminqv	v31.8h, p0, z0.h
.*:	64d7a000 	fminqv	v0.2d, p0, z0.d
.*:	6457bc00 	fminqv	v0.8h, p7, z0.h
.*:	6457a3e0 	fminqv	v0.8h, p0, z31.h
.*:	64d7bfff 	fminqv	v31.2d, p7, z31.d
.*:	6497ae8a 	fminqv	v10.4s, p3, z20.s
