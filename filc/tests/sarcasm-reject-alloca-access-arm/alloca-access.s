/* An alloca region is a capability, so raw sp-relative accesses into it bypass
   the bounds check and are rejected (address it via the alloca result instead). */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	sub	sp, sp, #400
	mov	x4, sp              ;! alloca result size=400
	str	x0, [sp, 8]
	add	sp, sp, #400
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f
