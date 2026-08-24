/* In a function that establishes rbp as a frame pointer (movq %rsp, %rbp), an
   rbp-relative access is a frame slot: the frame pass virtualizes it, so a
   `;! load ptr` on it has no memory operand (and no heap capability) to load
   through. Rejected with a clean error — arm64 rejects the same shape for
   sp/x29 bases. (rbp as an ordinary GPR WITHOUT a frame pointer stays legal;
   see the sarcasm-rbpgpr tests.) */
	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax      ;! load ptr
	leave
	ret
	.size	f, .-f
