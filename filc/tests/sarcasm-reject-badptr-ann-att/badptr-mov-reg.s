/* A register-to-register mov has no memory operand to bounds-check: ';! load
   ptr' requires a 64-bit mov between a GPR and memory. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, %rax          ;! load ptr
	popq	%rbp
	ret
	.size	f, .-f

