	.text
	# Single-step indexed lea off %rsp into the plain fixed frame. This used
	# to behave identically to the two-step idiom (the D9 fixed-frame escape
	# promotion turned the frame into a GC region and the redirect resolved
	# both spellings into it); that promotion was removed, so sarcasm now
	# REJECTS taking the address of the stack frame at compile time and this
	# file fails with "taking address of stack frame is not supported
	# (cannot prove safety)" on the first escaping lea.
	.globl	leaidx_single
	.type	leaidx_single, @function
leaidx_single:                  ;! long(long)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp
	movq	%rdi, %r12           # index survives the call in %r12
	leaq	16(%rsp), %rdi      # frame+16 escapes: rejected at compile time
	call	fill32 ;! void(ptr)
	leaq	16(%rsp,%r12,8), %rax
	movq	(%rax), %rax
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	leaidx_single, .-leaidx_single
	.globl	leaidx_two
	.type	leaidx_two, @function
leaidx_two:                     ;! long(long)
	pushq	%rbx
	pushq	%r12
	subq	$64, %rsp
	movq	%rdi, %r12
	leaq	16(%rsp), %rbx
	movq	%rbx, %rdi
	call	fill32 ;! void(ptr)
	leaq	(%rbx,%r12,8), %rax
	movq	(%rax), %rax
	addq	$64, %rsp
	popq	%r12
	popq	%rbx
	ret
	.size	leaidx_two, .-leaidx_two
	.globl	fill32
	.type	fill32, @function
fill32:                         ;! void(ptr)
	endbr64
	movq	$100, (%rdi)
	movq	$101, 8(%rdi)
	movq	$102, 16(%rdi)
	movq	$103, 24(%rdi)
	ret
	.size	fill32, .-fill32
	.section	.note.GNU-stack,"",@progbits
