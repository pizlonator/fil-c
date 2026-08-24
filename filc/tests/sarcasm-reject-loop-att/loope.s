/* `loope` additionally reads ZF and uses rcx implicitly — neither is modeled —
   so it is rejected like `loop` (use an explicit dec/cmp + conditional branch). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(long)
	movl	$0, %eax
.Lx:	addq	$2, %rax
	loope	.Lx
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
