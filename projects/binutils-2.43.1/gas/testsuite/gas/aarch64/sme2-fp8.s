	/* sme2-fp8.s Test file for AArch64 SME 8-bit floating-point
	vector instructions.  */

	.macro cvt_pat1, op
	\op	{z0.h-z1.h}, z3.b
	\op	{z0.h-z1.h}, z4.b
	\op	{z2.h-z3.h}, z4.b
	\op	{z28.h-z29.h}, z31.b
	.endm

	/* Multi-vector floating-point convert from 8-bit floating-point.  */

	/* To BFloat16 (in-order) 2^-UInt(FPMR.LSCALE{2}[5:0]).  */
	cvt_pat1 bf1cvt
	cvt_pat1 bf2cvt

	/* To deinterleaved BFloat16 (in-order) 2^-UInt(FPMR.LSCALE{2}[5:0]).  */
	cvt_pat1 bf1cvtl
	cvt_pat1 bf2cvtl

	/* To half-precision (in-order) 2^-UInt(FPMR.LSCALE{2}[3:0]).  */
	cvt_pat1 f1cvt
	cvt_pat1 f2cvt

	/* To deinterleaved half-precision 2^-UInt(FPMR.LSCALE{2}[3:0]).  */
	cvt_pat1 f1cvtl
	cvt_pat1 f2cvtl

	.macro cvt_pat2, op
	\op	z3.b, {z0.h-z1.h}
	\op	z4.b, {z0.h-z1.h}
	\op	z4.b, {z2.h-z3.h}
	\op	z31.b, {z28.h-z29.h}
	.endm

	/* Multi-vector floating-point convert to packed 8-bit floating-point
	format.  */

	/* From BFloat16 with 2^SInt(FPMR.NSCALE) scaling.  */
	cvt_pat2 bfcvt

	/* From half-precision with 2^SInt(FPMR.NSCALE[4:0]) scaling.  */
	cvt_pat2 fcvt

	.macro cvt_pat3, op
	\op	z7.b, {z0.s-z3.s}
	\op	z8.b, {z0.s-z3.s}
	\op	z1.b, {z4.s-z7.s}
	\op	z2.b, {z4.s-z7.s}
	\op	z31.b, {z24.s-z27.s}
	.endm

	/* Multi-vector floating-point convert from single-precision to
	{interleaved|packed} 8-bit floating-point format with
	2^SInt(FPMR.NSCALE) scaling.  */

	cvt_pat3 fcvtn /* interleaved.  */
	cvt_pat3 fcvt  /* packed.  */

	.macro fscale_single, w
	/* pair.  */
	fscale { z0.\w - z1.\w }, { z0.\w - z1.\w }, z2.\w
	fscale { z2.\w - z3.\w }, { z2.\w - z3.\w }, z2.\w
	fscale { z0.\w - z1.\w }, { z0.\w - z1.\w }, z4.\w
	fscale { z2.\w - z3.\w }, { z2.\w - z3.\w }, z4.\w
	fscale { z30.\w-z31.\w }, { z30.\w-z31.\w }, z15.\w
	/* quad.  */
	fscale { z0.\w-z3.\w }, { z0.\w-z3.\w }, z4.\w
	fscale { z4.\w-z7.\w }, { z4.\w-z7.\w }, z4.\w
	fscale { z0.\w-z3.\w }, { z0.\w-z3.\w }, z8.\w
	fscale { z4.\w-z7.\w }, { z4.\w-z7.\w }, z8.\w
	fscale { z28.\w-z31.\w }, { z28.\w-z31.\w }, z15.\w
	.endm

	/* Multi-vector floating-point adjust exponent by vector.

	Multiply floating-point elements of the 2 or 4 first source vecs by
	2.0^SInt(vals in the corresponding elements of 2nd source vector)
	and destructively place results in corresponding elements of 2 or 4
	first source vectors.  */

	fscale_single h
	fscale_single s
	fscale_single d

	.macro fscale_multi, w
	/* pair.  */
	fscale { z0.\w - z1.\w }, { z0.\w - z1.\w }, { z2.\w-z3.\w }
	fscale { z2.\w - z3.\w }, { z2.\w - z3.\w }, { z2.\w-z3.\w }
	fscale { z0.\w - z1.\w }, { z0.\w - z1.\w }, { z4.\w-z5.\w }
	fscale { z2.\w - z3.\w }, { z2.\w - z3.\w }, { z4.\w-z5.\w }
	fscale { z30.\w-z31.\w }, { z30.\w-z31.\w }, { z30.\w-z31.\w }
	/* quad.  */
	fscale { z0.\w-z3.\w }, { z0.\w-z3.\w }, { z4.\w-z7.\w }
	fscale { z4.\w-z7.\w }, { z4.\w-z7.\w }, { z4.\w-z7.\w }
	fscale { z0.\w-z3.\w }, { z0.\w-z3.\w }, { z8.\w-z11.\w }
	fscale { z4.\w-z7.\w }, { z4.\w-z7.\w }, { z8.\w-z11.\w }
	fscale { z28.\w-z31.\w }, { z28.\w-z31.\w }, { z24.\w-z27.\w }
	.endm

	/* Multi-vector floating-point adjust exponent.

	Multiply floating-point elements of the 2 or 4 first source vecs by
	2.0^SInt(vals in the corresponding elements of the 2 or 4 2nd source
	vectors) and destructively place results in corresponding elements
	of 2 or 4 first source vectors.  */

	fscale_multi h
	fscale_multi s
	fscale_multi d
