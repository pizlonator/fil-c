/* A :got: load reads a global's address out of the GOT: the address behind it
   cannot be capability-proven, so it is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	adrp	x1, :got:sym
	ldr	x1, [x1, #:got_lo12:sym]
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

