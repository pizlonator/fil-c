/* hlt is a privileged halt (unmodeled control effect; not a pure trap like
   brk/udf): rejected (privileged instructions cannot be made memory-safe). */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(long)
	mov	x0, x1
	hlt	#0
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
