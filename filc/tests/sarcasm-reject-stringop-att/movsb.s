/* Even BARE movsb (no rep prefix) implicitly read-modify-writes rsi and rdi —
   effects the model cannot represent (the instruction has no register
   operands, so the unknown fallback models zero effects): rejected, so that
   neither the prefixed nor the bare form can slip through. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr, long)
	movq	%rdi, %rsi
	movsb
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
