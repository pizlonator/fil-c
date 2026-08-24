/* ';! load ptr' on a store is a contradiction: the memory operand must be the
   source of a load. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, (%rax)        ;! load ptr
	popq	%rbp
	ret
	.size	f, .-f

