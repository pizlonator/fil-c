	/* sve-fp8.s Test file for AArch64 SVE 8-bit floating-point vector
	instructions.  */

	.macro cvt_pat1, op
	\op	z1.h, z0.b
	\op	z0.h, z1.b
	\op	z30.h, z31.b
	.endm

	.macro cvt_pat2, op, width
	\op	z0.b, { z0.\width - z1.\width }
	\op	z4.b, { z0.\width - z1.\width }
	\op	z0.b, { z2.\width - z3.\width }
	\op	z27.b, { z28.\width - z29.\width }
	.endm

	/* 8-bit floating-point convert to BFloat16 (top/bottom) with scaling by
	2^-UInt(FPMR.LSCALE{2}[5:0]).  */

	cvt_pat1 bf1cvt
	cvt_pat1 bf2cvt
	cvt_pat1 bf1cvtlt
	cvt_pat1 bf2cvtlt

	/* 8-bit floating-point convert to half-precision (top/bottom) with
	scaling by 2^-UInt(FPMR.LSCALE{2}[3:0]).  */

	cvt_pat1 f1cvt
	cvt_pat1 f2cvt
	cvt_pat1 f1cvtlt
	cvt_pat1 f2cvtlt

	/* BFloat16 convert, narrow and interleave to 8-bit floating-point
	with scaling by 2^SInt(FPMR.NSCALE).  */

	cvt_pat2 bfcvtn, h

	/* Half-precision convert, narrow and interleave to 8-bit floating-point
	with scaling by 2^SInt(FPMR.NSCALE[4:0]).  */

	cvt_pat2 fcvtn, h

	/* Single-precision convert, narrow and interleave to 8-bit
	floating-point (top/bottom) 2^SInt(FPMR.NSCALE).  */

	cvt_pat2 fcvtnb, s
	cvt_pat2 fcvtnt, s
