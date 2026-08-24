/* A register-indexed frame access has a dynamic offset the frame model cannot
   bound, so it is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x0, [sp, x1, lsl #3]
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

