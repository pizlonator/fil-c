	/* sve-fp8-fail.s Test file for error-checking AArch64 SVE 8-bit
	floating-point vector instructions.  */

	.macro cvt_pat1, op
	/* Check element width qualifier for destination register.  */
	\op	z0.b, z1.b
	\op	z0.h, z1.b /* Valid.  */
	\op	z0.s, z1.b
	\op	z0.d, z1.b
	/* Check element width qualifier for source register.  */
	\op	z0.h, z1.b /* Valid.  */
	\op	z0.h, z1.h
	\op	z0.h, z1.s
	\op	z0.h, z1.d
	/* Ensure predicate register is not allowed.  */
	\op	z0.h, p0, z1.d
	\op	z0.h, p0/z, z1.d
	.endm

	.macro cvt_pat2, op, width
	/* Check element width qualifier for destination register.  */
	\op	z0.b, { z0.\width - z1.\width } /* Valid.  */
	\op	z1.h, { z0.\width - z1.\width }
	\op	z0.s, { z0.\width - z1.\width }
	\op	z7.d, { z0.\width - z1.\width }
	/* Check whether source register range starts at even register.  */
	\op	z0.b, { z1.\width - z2.\width }
	.endm

	cvt_pat1 bf1cvt
	cvt_pat1 bf2cvt
	cvt_pat1 bf1cvtlt
	cvt_pat1 bf2cvtlt
	cvt_pat1 f1cvt
	cvt_pat1 f2cvt
	cvt_pat1 f1cvtlt
	cvt_pat1 f2cvtlt

	cvt_pat2 bfcvtn, h
	cvt_pat2 fcvtn, h
	cvt_pat2 fcvtnb, s
	cvt_pat2 fcvtnt, s
