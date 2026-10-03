	.text
# A `stack buffer` annotation on a HEAP access: rejected (it is only
# meaningful on a stack-relative access or a rep string op, and an annotated
# access must address the stack).
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	subq	$64, %rsp
	movl	(%rdi), %eax #! stack buffer (x, %rsp, %rsp + 32)
	addq	$64, %rsp
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
