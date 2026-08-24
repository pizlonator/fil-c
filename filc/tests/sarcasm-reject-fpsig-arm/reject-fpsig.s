/* Floating-point types in ;! signatures are rejected: FP arguments and returns
   travel in FP/SIMD registers, which sarcasm does not marshal. */
	.text
	.global	dmul
	.type	dmul, %function
dmul:                           ;! double(double, double)
	fmul	d0, d0, d1
	ret
	.size	dmul, .-dmul
