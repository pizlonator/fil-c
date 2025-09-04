	/* advsimd-fp8-fail.s Test file for error-checking AArch64 8-bit
	floating-point vector instructions.  */

	/* Instructions convert the elements from the lower half of the source
	vector while scaling the values by 2^-UInt(FPMR.LSCALE{2}[3:0]).  */

	.macro cvrt_lowerhalf, op
	/* Check the number and width of destination vector elements.  */
	\op	v0.16b, v1.8b
	\op	v0.8b, v1.8b
	\op	v0.4h, v1.8b
	\op	v0.8h, v1.8b /* Valid.  */
	\op 	v0.2s, v1.8b
	\op 	v0.4s, v1.8b
	\op 	v0.2d, v1.8b
	\op 	v0, v1.8b
	/* Check the number and width of source vector elements.  */
	\op 	v0.8h, v1.4h
	\op 	v0.8h, v1.2s
	\op 	v0.8h, v1.16b
	\op 	v0.8h, v1.8h
	\op 	v0.8h, v1.4s
	\op 	v0.8h, v1.2d
	\op 	v0.8h, v0
	.endm

	cvrt_lowerhalf	bf1cvtl
	cvrt_lowerhalf	bf2cvtl
	cvrt_lowerhalf	f1cvtl
	cvrt_lowerhalf	f2cvtl

	/* Instructions convert the elements from the upper half of the source
	vector while scaling the values by 2^-UInt(FPMR.LSCALE{2}[3:0]).  */

	.macro cvrt_upperhalf, op
	/* Check the number and width of destination vector elements.  */
	\op	v0.16b, v1.16b
	\op	v0.8b, v1.16b
	\op	v0.4h, v1.16b
	\op	v0.8h, v1.16b /* Valid.  */
	\op 	v0.2s, v1.16b
	\op 	v0.4s, v1.16b
	\op 	v0.2d, v1.16b
	\op 	v0, v1.16b
	/* Check the number and width of source vector elements.  */
	\op 	v0.8h, v1.8b
	\op 	v0.8h, v1.4h
	\op 	v0.8h, v1.2s
	\op 	v0.8h, v1.16b /* Valid.  */
	\op 	v0.8h, v1.8h
	\op 	v0.8h, v1.4s
	\op 	v0.8h, v1.2d
	\op 	v0.8h, v1
	.endm

	cvrt_upperhalf	bf1cvtl2
	cvrt_upperhalf	bf2cvtl2
	cvrt_upperhalf	f1cvtl2
	cvrt_upperhalf	f2cvtl2

	/* Floating-point adjust exponent by vector.  */

	/* Check invalid vector element number and width combinations.  */
	fscale	v0.8b, v0.8b, v0.8b
	fscale	v0.16b, v0.16b, v0.16b

	/* Half and single-precision to FP8 convert and narrow.  */

	/* Half-precision variant.  */
	/* Check the number and width of destination vector elements.  */
	fcvtn	v0.8b, v1.4h, v2.4h /* Valid.  */
	fcvtn	v0.4h, v1.4h, v2.4h
	fcvtn	v0.2s, v1.4h, v2.4h
	fcvtn	v0.16b, v1.4h, v2.4h
	fcvtn	v0.8h, v1.4h, v2.4h
	fcvtn	v0.4s, v1.4h, v2.4h
	fcvtn	v0.2d, v1.4h, v2.4h

	fcvtn	v0.8b, v1.8h, v2.8h
	fcvtn	v0.4h, v1.8h, v2.8h
	fcvtn	v0.2s, v1.8h, v2.8h
	fcvtn	v0.16b, v1.8h, v2.8h /* Valid.  */
	fcvtn	v0.8h, v1.8h, v2.8h
	fcvtn	v0.4s, v1.8h, v2.8h
	fcvtn	v0.2d, v1.8h, v2.8h

	/* Check the number and width of source vector elements.  */
	fcvtn	v0.8b, v1.8b, v2.8b
	fcvtn	v0.8b, v1.4h, v2.4h /* Valid.  */
	fcvtn	v0.8b, v1.2s, v2.2s
	fcvtn	v0.8b, v1.16b, v2.16b
	fcvtn	v0.8b, v1.8h, v2.8h
	fcvtn	v0.8b, v1.4s, v2.4s /* Valid.  */
	fcvtn	v0.8b, v1.2d, v2.2d

	fcvtn	v0.16b, v1.8b, v2.8b
	fcvtn	v0.16b, v1.4h, v2.4h
	fcvtn	v0.16b, v1.2s, v2.2s
	fcvtn	v0.16b, v1.16b, v2.16b
	fcvtn	v0.16b, v1.8h, v2.8h  /* Valid.  */
	fcvtn	v0.16b, v1.4s, v2.4s
	fcvtn	v0.16b, v1.2d, v2.2d

	/* Single-precision variant.  */
	/* Check the number and width of destination vector elements.  */
	fcvtn	v0.8b, v1.4s, v2.4s /* Valid.  */
	fcvtn	v0.4h, v1.4s, v2.4s
	fcvtn	v0.2s, v1.4s, v2.4s
	fcvtn	v0.16b, v1.4s, v2.4s
	fcvtn	v0.8h, v1.4s, v2.4s
	fcvtn	v0.4s, v1.4s, v2.4s
	fcvtn	v0.2d, v1.4s, v2.4s

	/* Check the number and width of source vector elements.  */
	fcvtn2	v0.16b, v1.8b, v2.8b
	fcvtn2	v0.16b, v1.4h, v2.4h
	fcvtn2	v0.16b, v1.2d, v2.2d
	fcvtn2	v0.16b, v1.16b, v2.16b
	fcvtn2	v0.16b, v1.8h, v2.8h
	fcvtn2	v0.16b, v1.4s, v2.4s  /* Valid.  */
	fcvtn2	v0.16b, v1.2d, v2.2d
