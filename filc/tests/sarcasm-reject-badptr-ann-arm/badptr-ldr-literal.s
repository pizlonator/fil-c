/* A literal-pool load has no memory operand to bounds-check (and literal pools
   cannot be made memory-safe): rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	ldr	x0, .Llit           ;! load ptr
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

