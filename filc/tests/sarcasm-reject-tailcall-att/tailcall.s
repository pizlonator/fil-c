/* A jump to a non-local label is a tail call: even with a ;! signature it is
   rejected (tail calls are not yet supported). Jumps to local labels are fine. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	jmp	somefunc            ;! int(ptr)
	popq	%rbp
	ret
	.size	f, .-f

