# Save-after-and/use-after-and through a mid-function `and $-32, %rsp`: both
# the save (`movq %rsp, %rax`) and all uses (`0(%rax)`/`8(%rax)`) sit after
# the and, so they share post-and numbering (same dynamic slack) and stay
# legal. Only the crossing shape — save before, use after, or saves on mixed
# sides reaching one use — is rejected (see sarcasm-reject-alias-cross-and-att).
# The unconditional jump ends the prologue prefix, so the and is a mid-function
# note (a prologue and would never trigger the crossing check).
	.text
	.globl	andafter_test
	.type	andafter_test, @function
andafter_test:                  ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	jmp	.Lafter
.Lafter:
	and	$-32, %rsp
	movq	%rsp, %rax
	movq	%rdi, 0(%rax)
	movq	%rsi, 8(%rax)
	movq	0(%rax), %rcx
	addq	8(%rax), %rcx
	movq	%rcx, %rax
	addq	$64, %rsp
	popq	%rbx
	ret
	.size	andafter_test, .-andafter_test
	.section	.note.GNU-stack,"",@progbits
