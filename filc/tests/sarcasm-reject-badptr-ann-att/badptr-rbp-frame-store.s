/* The store twin of badptr-rbp-frame-load.s: a `;! store ptr` through an
   rbp-relative frame slot (frame-pointer function) is rejected — the slot is
   virtualized, leaving no heap capability to store through. */
	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	%rdi, %rax
	movq	%rax, -8(%rbp)      ;! store ptr
	leave
	ret
	.size	f, .-f
