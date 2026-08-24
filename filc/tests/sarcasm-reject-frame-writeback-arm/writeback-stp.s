/* A data store with sp writeback changes sp in ways the frame model cannot
   track: only callee-saved stp/ldp prologue/epilogue pairs may use writeback. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	stp	x0, x1, [sp, #-16]!
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

