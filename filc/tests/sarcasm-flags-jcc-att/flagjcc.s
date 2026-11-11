	.text
	.globl	flagjcc
	.type	flagjcc, @function
flagjcc:                        ;! long(ptr, long)
	cmpq	$5, %rsi
	movq	(%rdi), %rax
	jg	.Lbig
	movl	$0, %eax
	ret
.Lbig:
	movl	$1, %eax
	ret
	.size	flagjcc, .-flagjcc

	.globl	flagbits
	.type	flagbits, @function
# Snapshot every arithmetic flag across a checked load, with an accumulator
# originally in rax live across the check. The load's value is observed too.
flagbits:                       ;! long(ptr, long, long, long)
	movq	%rsi, %rax
	testq	%rcx, %rcx
	jnz	.Lbits_sub
	addb	%dl, %al
	jmp	.Lbits_load
.Lbits_sub:
	subb	%dl, %al
.Lbits_load:
	movq	(%rdi), %r8
	seto	%r9b
	lahf
	movzbl	%r9b, %r9d
	shrq	$8, %rax
	andq	$213, %rax
	shlq	$8, %r9
	orq	%r9, %rax
	xorq	%r8, %rax
	ret
	.size	flagbits, .-flagbits

	.globl	flagadx
	.type	flagadx, @function
# Keep independent carry and overflow chains live through checked memory
# operands. The explicit flags word also exercises the full-width fallback.
flagadx:                        ;! long(ptr, long)
	pushq	%rsi
	popfq
	movq	$-1, %rax
	movq	$0, %r8
	adcxq	(%rdi), %rax
	adoxq	8(%rdi), %r8
	adcxq	16(%rdi), %rax
	adoxq	24(%rdi), %r8
	seto	%r9b
	movzbl	%r9b, %r9d
	shlq	$16, %r9
	shlq	$32, %rax
	orq	%r8, %rax
	orq	%r9, %rax
	ret
	.size	flagadx, .-flagadx
	.section	.note.GNU-stack,"",@progbits
