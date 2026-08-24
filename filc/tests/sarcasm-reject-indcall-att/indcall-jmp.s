/* An indirect jump through a raw register (computed goto / a switch jump table)
   has no provable target and is rejected. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	jmp	*%r9
	popq	%rbp
	ret
	.size	f, .-f

