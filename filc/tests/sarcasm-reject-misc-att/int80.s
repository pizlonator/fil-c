/* int $0x80 enters the kernel with RAW arguments, bypassing Fil-C's syscall
   argument validation (the same hole as syscall): rejected. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movq	%rdi, %rax
	int	$0x80
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
