# Multi-and positive: two mid-function `and $-32, %rsp` notes with the alias
# save AFTER the second and and all uses after it. Save and uses share post-
# and#2 numbering (same dynamic slack), so no crossing — legal. Contrasts with
# sarcasm-reject-alias-cross-and2-att (save between and#1/and#2, use after
# and#2 — crosses and#2's slack, rejected).
	.text
	.globl	andafter2_test
	.type	andafter2_test, @function
andafter2_test:                  ;! long(long,long)
	pushq	%rbx
	subq	$64, %rsp
	jmp	.Lafter1
.Lafter1:
	and	$-32, %rsp
	jmp	.Lafter2
.Lafter2:
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
	.size	andafter2_test, .-andafter2_test
	.section	.note.GNU-stack,"",@progbits
