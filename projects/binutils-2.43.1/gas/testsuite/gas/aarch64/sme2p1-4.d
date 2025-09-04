#name: Test of SME2.1 MOVAZ (tile to vector, single) instructions.
#as: -march=armv9.4-a+sme2p1
#objdump: -dr

[^:]+:     file format .*


[^:]+:

[^:]+:
.*:	c0020200 	movaz	z0.b, za0h.b \[w12, 0\]
.*:	c002021f 	movaz	z31.b, za0h.b \[w12, 0\]
.*:	c0028200 	movaz	z0.b, za0v.b \[w12, 0\]
.*:	c0026200 	movaz	z0.b, za0h.b \[w15, 0\]
.*:	c00203e0 	movaz	z0.b, za0h.b \[w12, 15\]
.*:	c002e3ff 	movaz	z31.b, za0v.b \[w15, 15\]
.*:	c002226f 	movaz	z15.b, za0h.b \[w13, 3\]
.*:	c0024227 	movaz	z7.b, za0h.b \[w14, 1\]
.*:	c0420200 	movaz	z0.h, za0h.h \[w12, 0\]
.*:	c042021f 	movaz	z31.h, za0h.h \[w12, 0\]
.*:	c0420300 	movaz	z0.h, za1h.h \[w12, 0\]
.*:	c0428200 	movaz	z0.h, za0v.h \[w12, 0\]
.*:	c0426200 	movaz	z0.h, za0h.h \[w15, 0\]
.*:	c04202e0 	movaz	z0.h, za0h.h \[w12, 7\]
.*:	c042e3ff 	movaz	z31.h, za1v.h \[w15, 7\]
.*:	c042226f 	movaz	z15.h, za0h.h \[w13, 3\]
.*:	c0424227 	movaz	z7.h, za0h.h \[w14, 1\]
.*:	c0820200 	movaz	z0.s, za0h.s \[w12, 0\]
.*:	c082021f 	movaz	z31.s, za0h.s \[w12, 0\]
.*:	c0820380 	movaz	z0.s, za3h.s \[w12, 0\]
.*:	c0828200 	movaz	z0.s, za0v.s \[w12, 0\]
.*:	c0826200 	movaz	z0.s, za0h.s \[w15, 0\]
.*:	c0820260 	movaz	z0.s, za0h.s \[w12, 3\]
.*:	c082e3ff 	movaz	z31.s, za3v.s \[w15, 3\]
.*:	c08222cf 	movaz	z15.s, za1h.s \[w13, 2\]
.*:	c0824327 	movaz	z7.s, za2h.s \[w14, 1\]
.*:	c0c20200 	movaz	z0.d, za0h.d \[w12, 0\]
.*:	c0c2021f 	movaz	z31.d, za0h.d \[w12, 0\]
.*:	c0c203c0 	movaz	z0.d, za7h.d \[w12, 0\]
.*:	c0c28200 	movaz	z0.d, za0v.d \[w12, 0\]
.*:	c0c26200 	movaz	z0.d, za0h.d \[w15, 0\]
.*:	c0c20220 	movaz	z0.d, za0h.d \[w12, 1\]
.*:	c0c2e3ff 	movaz	z31.d, za7v.d \[w15, 1\]
.*:	c0c222cf 	movaz	z15.d, za3h.d \[w13, 0\]
.*:	c0c24327 	movaz	z7.d, za4h.d \[w14, 1\]
.*:	c0c30200 	movaz	z0.q, za0h.q \[w12, 0\]
.*:	c0c3021f 	movaz	z31.q, za0h.q \[w12, 0\]
.*:	c0c303e0 	movaz	z0.q, za15h.q \[w12, 0\]
.*:	c0c38200 	movaz	z0.q, za0v.q \[w12, 0\]
.*:	c0c36200 	movaz	z0.q, za0h.q \[w15, 0\]
.*:	c0c3e3ff 	movaz	z31.q, za15v.q \[w15, 0\]
.*:	c0c322ef 	movaz	z15.q, za7h.q \[w13, 0\]
.*:	c0c342c7 	movaz	z7.q, za6h.q \[w14, 0\]
