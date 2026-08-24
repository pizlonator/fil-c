/* svc enters the kernel with RAW arguments, bypassing Fil-C's syscall
   argument validation — a genuine memory-safety hole — and has unmodeled
   control and clobber effects: rejected. (brk/udf stay allowed: pure
   traps.) */
	.text
	.global	f
	.type	f, %function
f:                              ;! long(long)
	mov	x8, #64
	svc	#0
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
