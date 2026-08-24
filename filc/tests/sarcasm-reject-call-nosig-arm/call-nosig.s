/* A raw bl without a ;! callsite signature cannot be marshalled (sarcasm would
   not know which argument registers hold pointers), so it is rejected. */
	.text
	.global	f
	.type	f, %function
f:                              ;! void(ptr)
	stp	x29, x30, [sp, -16]!
	mov	x29, sp
	bl	callee
	ldp	x29, x30, [sp], 16
	ret
	.size	f, .-f

