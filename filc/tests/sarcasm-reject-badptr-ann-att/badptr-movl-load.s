/* A 32-bit load cannot load a capability: the destination must be a 64-bit
   GPR. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movl	(%rdi), %eax        ;! load ptr
	popq	%rbp
	ret
	.size	f, .-f

