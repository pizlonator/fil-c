/* FP/SIMD registers in frame-relative memory ops (a NEON spill) cannot be made
   memory-safe yet, so they are rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	stp	d8, d9, [sp, #16]
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

