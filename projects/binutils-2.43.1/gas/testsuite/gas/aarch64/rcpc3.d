#name: RCPC3 GPR load/store
#source: rcpc3.s
#as: -march=armv8.2-a+rcpc3
#objdump: -dr

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
[^:]+:	d9411860 	ldiapp	x0, x1, \[x3\]
[^:]+:	99411860 	ldiapp	w0, w1, \[x3\]
[^:]+:	d9410860 	ldiapp	x0, x1, \[x3\], #16
[^:]+:	99410860 	ldiapp	w0, w1, \[x3\], #8
[^:]+:	d9410bc0 	ldiapp	x0, x1, \[x30\], #16
[^:]+:	d95e081d 	ldiapp	x29, x30, \[x0\], #16
[^:]+:	d941087f 	ldiapp	xzr, x1, \[x3\], #16
[^:]+:	9941087f 	ldiapp	wzr, w1, \[x3\], #8
[^:]+:	d9410be0 	ldiapp	x0, x1, \[sp\], #16
[^:]+:	99410be0 	ldiapp	w0, w1, \[sp\], #8
[^:]+:	d9411800 	ldiapp	x0, x1, \[x0\]
[^:]+:	d9411820 	ldiapp	x0, x1, \[x1\]
[^:]+:	99411800 	ldiapp	w0, w1, \[x0\]
[^:]+:	99411820 	ldiapp	w0, w1, \[x1\]
[^:]+:	d9011860 	stilp	x0, x1, \[x3\]
[^:]+:	99011860 	stilp	w0, w1, \[x3\]
[^:]+:	d9010860 	stilp	x0, x1, \[x3, #-16\]!
[^:]+:	99010860 	stilp	w0, w1, \[x3, #-8\]!
[^:]+:	d9011820 	stilp	x0, x1, \[x1\]
[^:]+:	d9011800 	stilp	x0, x1, \[x0\]
[^:]+:	99011820 	stilp	w0, w1, \[x1\]
[^:]+:	99011800 	stilp	w0, w1, \[x0\]
[^:]+:	b8bfc020 	ldapr	w0, \[x1\]
[^:]+:	b8bfc020 	ldapr	w0, \[x1\]
[^:]+:	f8bfc020 	ldapr	x0, \[x1\]
[^:]+:	f8bfc020 	ldapr	x0, \[x1\]
[^:]+:	99c00841 	ldapr	w1, \[x2\], #4
[^:]+:	d9c00841 	ldapr	x1, \[x2\], #8
[^:]+:	d9c0081e 	ldapr	x30, \[x0\], #8
[^:]+:	d9c00bc0 	ldapr	x0, \[x30\], #8
[^:]+:	99c0083f 	ldapr	wzr, \[x1\], #4
[^:]+:	d9c0083f 	ldapr	xzr, \[x1\], #8
[^:]+:	99c00be0 	ldapr	w0, \[sp\], #4
[^:]+:	d9c00be0 	ldapr	x0, \[sp\], #8
[^:]+:	889ffc20 	stlr	w0, \[x1\]
[^:]+:	889ffc20 	stlr	w0, \[x1\]
[^:]+:	c89ffc20 	stlr	x0, \[x1\]
[^:]+:	c89ffc20 	stlr	x0, \[x1\]
[^:]+:	99800841 	stlr	w1, \[x2, #-4\]!
[^:]+:	d9800841 	stlr	x1, \[x2, #-8\]!
[^:]+:	d980081e 	stlr	x30, \[x0, #-8\]!
[^:]+:	d9800bc0 	stlr	x0, \[x30, #-8\]!
[^:]+:	9980083f 	stlr	wzr, \[x1, #-4\]!
[^:]+:	d980083f 	stlr	xzr, \[x1, #-8\]!
[^:]+:	99800be0 	stlr	w0, \[sp, #-4\]!
[^:]+:	d9800be0 	stlr	x0, \[sp, #-8\]!
