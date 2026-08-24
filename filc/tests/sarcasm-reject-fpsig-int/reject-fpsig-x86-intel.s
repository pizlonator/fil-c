/* Floating-point types in ;! signatures are rejected on x86_64 (Intel syntax). */
	.intel_syntax noprefix
	.text
	.globl	g
	.type	g, @function
g:                              ;! float(float, ptr)
	ret
	.size	g, .-g
