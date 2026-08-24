/* gcc-style register aliases: fp = x29 (frame pointer), lr = x30 (link
   register).  This is exactly how gcc -O2 emits a leaf frame, and it must be
   accepted just like the x29/x30 spellings. */
	.text
	.global	addnine
	.type	addnine, %function
addnine:                        ;! long(long)
	stp	fp, lr, [sp, -16]!
	mov	fp, sp
	add	x0, x0, #9
	ldp	fp, lr, [sp], 16
	ret
	.size	addnine, .-addnine
	.section	.note.GNU-stack,"",@progbits
