/* ';! frobnicate' is not a recognized annotation form: rejected (the expected
   forms are 'load ptr', 'store ptr', 'alloca ...' or a callsite signature). */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	%rdi, %rax          ;! frobnicate
	popq	%rbp
	ret
	.size	f, .-f

