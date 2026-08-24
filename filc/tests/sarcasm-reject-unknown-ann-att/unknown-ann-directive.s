/* A ;! annotation on a directive has no meaning and is rejected (annotations go
   on instructions, the function-entry label, or a callsite). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	.p2align	3              ;! load ptr
	popq	%rbp
	ret
	.size	f, .-f

