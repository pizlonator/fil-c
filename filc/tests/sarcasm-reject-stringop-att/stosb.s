/* Bare stosb implicitly uses al and read-modify-writes rdi — unmodeled
   (no register operands -> zero-effect unknown fallback): rejected like
   movsb. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr, long)
	movl	%esi, %eax
	stosb
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
