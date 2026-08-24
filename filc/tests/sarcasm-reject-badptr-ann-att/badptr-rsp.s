/* An rsp-relative access is a frame slot (virtualized by the frame pass), not
   an annotated pointer access: rejected. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	movq	8(%rsp), %rax       ;! load ptr
	popq	%rbp
	ret
	.size	f, .-f

