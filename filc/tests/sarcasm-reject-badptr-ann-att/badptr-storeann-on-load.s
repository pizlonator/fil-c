/* ';! store ptr' on a load is a contradiction: the memory operand must be the
   destination of a store. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	(%rdi), %rax        ;! store ptr
	popq	%rbp
	ret
	.size	f, .-f

