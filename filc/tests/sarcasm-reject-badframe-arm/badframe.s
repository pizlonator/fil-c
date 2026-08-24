/* A store past the end of the (16-byte) frame lands in the caller's argument
   area: sp-relative accesses must stay inside the frame sarcasm modeled. */
	.arch armv8-a
	.text
	.global	badstore
	.type	badstore, %function
badstore:                       ;! void(int)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	str	x0, [sp, 64]
	ldp	x29, x30, [sp], 16
	ret
	.size	badstore, .-badstore
