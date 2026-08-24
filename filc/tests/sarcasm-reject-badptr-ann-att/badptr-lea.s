/* lea computes an address without accessing memory: ';! load ptr' is only
   valid on a real 64-bit mov load. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	leaq	(%rdi), %rax        ;! load ptr
	popq	%rbp
	ret
	.size	f, .-f

