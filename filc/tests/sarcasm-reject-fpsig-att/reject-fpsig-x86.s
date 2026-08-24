/* Floating-point types in ;! signatures are rejected on x86_64 too: the generic
   thunk marshals every argument word through GPRs, so FP args (which the real
   fast CC passes in xmm registers) would be corrupted. */
	.text
	.globl	dmul
	.type	dmul, @function
dmul:                           ;! double(double, double)
	mulsd	%xmm1, %xmm0
	ret
	.size	dmul, .-dmul
