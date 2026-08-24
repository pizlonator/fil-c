/* A raw call without a ;! callsite signature cannot be marshalled (sarcasm would
   not know which argument registers hold pointers), so it is rejected. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	call	callee
	popq	%rbp
	ret
	.size	f, .-f

