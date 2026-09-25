	.text
	# Fail-closed indexed lea: a single-step indexed lea whose dynamic index
	# lands outside the region used to trap at the access (the D9 fixed-frame
	# escape promotion made the frame a GC region and the runtime bounds check
	# caught the out-of-region index). That promotion was removed: sarcasm now
	# REJECTS taking the address of the stack frame at compile time, so this
	# file fails with "taking address of stack frame is not supported
	# (cannot prove safety)" on the escaping lea — before any code runs.
	.globl	leaidx_oob
	.type	leaidx_oob, @function
leaidx_oob:                     ;! long(long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rdi, %rbx           # index survives the call in %rbx
	leaq	16(%rsp), %rdi      # frame+16 escapes: rejected at compile time
	call	fill32 ;! void(ptr)
	leaq	16(%rsp,%rbx,8), %rax
	movq	(%rax), %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	leaidx_oob, .-leaidx_oob
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
