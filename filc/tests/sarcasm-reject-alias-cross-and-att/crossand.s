# An alias-carrier access crossing a mid-function `and $-N, %rsp` is a static
# error: pre- and post-and traffic share one numbering, but the and shifts rsp
# by a dynamic slack, so the same slot key names different addresses on the
# two sides. Here %rax parks the pre-and rsp (the unconditional jump ends the
# prologue prefix, so the and is a mid-function note) and is used after it.
	.text
	.globl	crossand
	.type	crossand, @function
crossand:                       ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	movq	%rsp, %rax
	movq	%rdi, 0(%rax)
	jmp	.Lafter
.Lafter:
	and	$-32, %rsp
	movq	%rsi, 8(%rax)
	movq	0(%rax), %rcx
	addq	8(%rax), %rcx
	movq	%rcx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	crossand, .-crossand
	.section	.note.GNU-stack,"",@progbits
