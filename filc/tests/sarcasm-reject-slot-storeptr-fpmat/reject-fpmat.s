	.text
	.globl	f
	.type	f, @function
f:                              ;! void(ptr)
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$64, %rsp
	# The vector store below taints [16, 32) (normalized offsets), so the
	# 8-byte slot access at offset 24 MATERIALIZES into real frame memory --
	# where the invisicap sidecar bytes have no home. Move such a pointer
	# through a virtualized slot (outside the FP-tainted ranges) instead.
	vmovdqu	%xmm0, -48(%rbp)
	movq	%rdi, -40(%rbp)    ;! store ptr
	movq	-40(%rbp), %rax    ;! load ptr
	movq	(%rax), %rax
	leave
	ret
	.size	f, .-f
	.section	.note.GNU-stack,"",@progbits
