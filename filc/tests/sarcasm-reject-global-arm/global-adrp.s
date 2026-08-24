/* adrp+ldr materializes the address of a global, but a global cannot be given a
   capability automatically, so the raw address forge is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	adrp	x1, sym
	ldr	x1, [x1, :lo12:sym]
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

