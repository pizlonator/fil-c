/* A signature-shaped annotation only makes sense on a call: on any other
   instruction it is rejected. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	addq	%rsi, %rdi          ;! int(ptr)
	popq	%rbp
	ret
	.size	f, .-f

