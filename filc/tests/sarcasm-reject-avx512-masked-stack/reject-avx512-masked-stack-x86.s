	.text
	.globl	f
	.type	f, @function
f:                              ;! long(ptr)
	# AVX512 {k}-masked memory accesses whose lane structure sarcasm cannot
	# lower for a stack slot (expand/compress, truncating stores, ALU memory
	# sources) are still rejected: the frame rewrite virtualizes/materializes
	# the operand, and only the masked vector-move family has an exact
	# load / register-masked-move / store lowering (see the masked-stack
	# lowering). This pins the stack-slot rejection for the non-lowerable
	# forms — the heap twins of these accept the same forms with the
	# mask-aware bounds check.
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$64, %rsp
	vcompressq	%zmm0, -64(%rbp){%k1}
	movq	%rdi, %rax
	leave
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
