#name: Test of SVE2.1 pmov instruction.
#as: -march=armv9.4-a
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	052a3800 	pmov	p0.b, z0
.*:	052a380f 	pmov	p15.b, z0
.*:	052a3be0 	pmov	p0.b, z31
.*:	052a3bef 	pmov	p15.b, z31
.*:	052a3bef 	pmov	p15.b, z31
.*:	052a39e7 	pmov	p7.b, z15
.*:	052c3800 	pmov	p0.h, z0
.*:	052c380f 	pmov	p15.h, z0
.*:	052c3be0 	pmov	p0.h, z31
.*:	052e3800 	pmov	p0.h, z0\[1\]
.*:	052e3bef 	pmov	p15.h, z31\[1\]
.*:	052c39e7 	pmov	p7.h, z15
.*:	052c3903 	pmov	p3.h, z8
.*:	05683800 	pmov	p0.s, z0
.*:	0568380f 	pmov	p15.s, z0
.*:	05683be0 	pmov	p0.s, z31
.*:	056e3800 	pmov	p0.s, z0\[3\]
.*:	056e3bef 	pmov	p15.s, z31\[3\]
.*:	056a39e7 	pmov	p7.s, z15\[1\]
.*:	05683903 	pmov	p3.s, z8
.*:	05a83800 	pmov	p0.d, z0
.*:	05a8380f 	pmov	p15.d, z0
.*:	05a83be0 	pmov	p0.d, z31
.*:	05ee3800 	pmov	p0.d, z0\[7\]
.*:	05ee3bef 	pmov	p15.d, z31\[7\]
.*:	05ae39e7 	pmov	p7.d, z15\[3\]
.*:	05a83903 	pmov	p3.d, z8
.*:	052b3800 	pmov	z0, p0.b
.*:	052b381f 	pmov	z31, p0.b
.*:	052b39e0 	pmov	z0, p15.b
.*:	052b39ff 	pmov	z31, p15.b
.*:	052b38e0 	pmov	z0, p7.b
.*:	052b38ef 	pmov	z15, p7.b
.*:	052d3800 	pmov	z0, p0.h
.*:	052d381f 	pmov	z31, p0.h
.*:	052f3800 	pmov	z0\[1\], p0.h
.*:	052d39e0 	pmov	z0, p15.h
.*:	052f39ff 	pmov	z31\[1\], p15.h
.*:	052d39ef 	pmov	z15, p15.h
.*:	052d38e8 	pmov	z8, p7.h
.*:	05693800 	pmov	z0, p0.s
.*:	0569381f 	pmov	z31, p0.s
.*:	056f3800 	pmov	z0\[3\], p0.s
.*:	056939e0 	pmov	z0, p15.s
.*:	056f39ff 	pmov	z31\[3\], p15.s
.*:	056d39ef 	pmov	z15\[2\], p15.s
.*:	056938e8 	pmov	z8, p7.s
.*:	05a93800 	pmov	z0, p0.d
.*:	05a9381f 	pmov	z31, p0.d
.*:	05ef3800 	pmov	z0\[7\], p0.d
.*:	05a939e0 	pmov	z0, p15.d
.*:	05ef39ff 	pmov	z31\[7\], p15.d
.*:	05af39ef 	pmov	z15\[3\], p15.d
.*:	05a938e8 	pmov	z8, p7.d
