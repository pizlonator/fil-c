# Feature 2 reject twin: a rsp-relative access that executes on BOTH sides of
# the mid-function `and` (the loop wraps around it) names one slot key two
# real addresses a dynamic slack apart — one execution is pre-and (true
# coordinates), the other post-and (the and's slack=0 convention). The
# rewrite keeps the historical rejection for the both-sides shape; only the
# strictly-pre-and spelling (sarcasm-preand-rsp-att) is newly legal.
	.text
	.globl	rej
	.type	rej, @function
rej:                            ;! long(long)
	jmp	.Lenter
.Lenter:
	subq	$128, %rsp
	movl	%edi, %r8d
.Lloop:
	movq	%rdi, 8(%rsp)		# executes pre-and (first pass) AND post-and
	movq	8(%rsp), %rax		# (the loop wraps) -> one key, two addresses
	andq	$-32, %rsp
	decl	%r8d
	jnz	.Lloop
	addq	$128, %rsp
	ret
	.size	rej, .-rej
	.section	.note.GNU-stack,"",@progbits
