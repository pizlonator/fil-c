/* Storing x30 with sp writeback looks like a prologue but is not the
   callee-saved stp x29, x30 pair the frame model recognizes: rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	str	x30, [sp, #-16]!
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

