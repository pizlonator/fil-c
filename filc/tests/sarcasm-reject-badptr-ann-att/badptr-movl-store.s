/* A 32-bit store cannot store a capability: the stored value must be a 64-bit
   GPR. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movl	%eax, (%rdi)        ;! store ptr
	popq	%rbp
	ret
	.size	f, .-f

