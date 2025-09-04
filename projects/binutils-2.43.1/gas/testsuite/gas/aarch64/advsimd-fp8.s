	/* advsimd-fp8.s Test file for AArch64 8-bit floating-point vector
	instructions.  */

	/* Instructions convert the elements from the lower half of the source
	vector while scaling the values by 2^-UInt(FPMR.LSCALE{2}[3:0]).  */

	.macro cvrt_lowerhalf, op
	\op 	v0.8h, v0.8b
	\op 	v31.8h, v0.8b
	\op 	v0.8h, v31.8b
	\op 	v31.8h, v31.8b
	.endm

	cvrt_lowerhalf	bf1cvtl
	cvrt_lowerhalf	bf2cvtl
	cvrt_lowerhalf	f1cvtl
	cvrt_lowerhalf	f2cvtl

	/* Instructions convert the elements from the upper half of the source
	vector while scaling the values by 2^-UInt(FPMR.LSCALE{2}[3:0]).  */

	.macro cvrt_upperhalf, op
	\op 	v0.8h, v0.16b
	\op 	v31.8h, v0.16b
	\op 	v0.8h, v31.16b
	\op 	v31.8h, v31.16b
	.endm

	cvrt_upperhalf	bf1cvtl2
	cvrt_upperhalf	bf2cvtl2
	cvrt_upperhalf	f1cvtl2
	cvrt_upperhalf	f2cvtl2

	/* Floating-point adjust exponent by vector.  */

	.macro fscale_gen, op_var
	fscale	v0.\op_var, v0.\op_var, v0.\op_var
	fscale	v31.\op_var, v0.\op_var, v0.\op_var
	fscale	v0.\op_var, v31.\op_var, v0.\op_var
	fscale	v0.\op_var, v0.\op_var, v31.\op_var
	fscale	v31.\op_var, v31.\op_var, v0.\op_var
	fscale	v0.\op_var, v31.\op_var, v31.\op_var
	fscale	v31.\op_var, v31.\op_var, v0.\op_var
	fscale	v31.\op_var, v31.\op_var, v31.\op_var
	.endm

	/* Half-precision variant.  */
	fscale_gen	4h
	fscale_gen	8h
	/* Single-precision variant.  */
	fscale_gen	2s
	fscale_gen	4s
	fscale_gen	2d

	/* Half and single-precision to FP8 convert and narrow.  */

	.macro fcvtn_to_fp8, op, sd, ss
	\op	v0.\sd, v0.\ss, v0.\ss
	\op	v31.\sd, v0.\ss, v0.\ss
	\op	v0.\sd, v31.\ss, v0.\ss
	\op	v0.\sd, v0.\ss, v31.\ss
	\op	v31.\sd, v31.\ss, v0.\ss
	\op	v0.\sd, v31.\ss, v31.\ss
	\op	v31.\sd, v31.\ss, v31.\ss
	.endm

	/* Half-precision variant.  */
	fcvtn_to_fp8 fcvtn 8b, 4h
	fcvtn_to_fp8 fcvtn 16b, 8h

	/* Single-precision variant.  */
	fcvtn_to_fp8 fcvtn, 8b, 4s
	fcvtn_to_fp8 fcvtn2, 16b, 4s
