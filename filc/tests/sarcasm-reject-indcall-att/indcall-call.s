/* An indirect call through a raw register has no provable target: only calls
   through function-pointer values (which carry capabilities) can be made safe. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	call	*%r9
	popq	%rbp
	ret
	.size	f, .-f

