# Check 64bit instructions with optimized encoding

	.text
_start:
	andq	$foo, %rax
	andq	$((1<<31) - 1), %rax
	andq	$((1<<31) - 1), %rbx
	andq	$((1<<31) - 1), %r14
	andq	$-((1<<31)), %rax
	andq	$-((1<<31)), %rbx
	andq	$-((1<<31)), %r14
	andq	$((1<<7) - 1), %rax
	andq	$((1<<7) - 1), %rbx
	andq	$((1<<7) - 1), %r14
	andq	$-((1<<7)), %rax
	andq	$-((1<<7)), %rbx
	andq	$-((1<<7)), %r14
	testq	$((1<<31) - 1), %rax
	testq	$((1<<31) - 1), %rbx
	testq	$((1<<31) - 1), %r14
	testq	$-((1<<31)), %rax
	testq	$-((1<<31)), %rbx
	testq	$-((1<<31)), %r14
	xorq	(%rsi), %rax
	xorq	%rax, %rax
	xorq	%rbx, %rbx
	xorq	%r14, %r14
	xorq	%rdx, %rax
	xorq	%rdx, %rbx
	xorq	%rdx, %r14
	subq	%rax, %rax
	subq	%rbx, %rbx
	subq	%r14, %r14
	subq	%rdx, %rax
	subq	%rdx, %rbx
	subq	%rdx, %r14
	andq	$((1<<31) - 1), (%rax)
	andq	$-((1<<31)), (%rax)
	testq	$((1<<31) - 1), (%rax)
	testq	$-((1<<31)), (%rax)
	mov	$((1<<31) - 1),%rax
	movq	$((1<<31) - 1),%rax
	mov	$((1<<31) - 1),%r8
	movq	$((1<<31) - 1),%r8
	mov	$0xffffffff,%rax
	movq	$0xffffffff,%rax
	mov	$0xffffffff,%r8
	movq	$0xffffffff,%r8
	mov	$1023,%rax
	movq	$1023,%rax
	mov	$0x100000000,%rax
	movq	$0x100000000,%rax
	clrq	%rax
	clrq	%r14
	bt	$15, %ax
	bt	$16, %ax
	bt	$15, %r8w
	bt	$16, %r8w
	bt	$31, %rax
	bt	$32, %rax
	bt	$31, %r8
	btc	$15, %ax
	btc	$31, %rax
	btr	$15, %ax
	btr	$31, %rax
	bts	$15, %ax
	bts	$31, %rax
	xor	%rcx, %rcx, %rcx
	xor	%rdx, %rcx, %rcx
	xor	%rdx, %rdx, %rcx
	sub	%rcx, %rcx, %rcx
	sub	%rdx, %rcx, %rcx
	sub	%rdx, %rdx, %rcx
	xor	%r17, %r17, %r17
	xor	%r18, %r17, %r17
	xor	%r18, %r18, %r17
	sub	%r17, %r17, %r17
	sub	%r18, %r17, %r17
	sub	%r18, %r18, %r17
	xor	%cx, %cx, %cx
	xor	%dx, %cx, %cx
	xor	%dx, %dx, %cx
	sub	%cx, %cx, %cx
	sub	%dx, %cx, %cx
	sub	%dx, %dx, %cx
	xor	%r17b, %r17b, %r17b
	xor	%r18b, %r17b, %r17b
	xor	%r18b, %r18b, %r17b
	sub	%r17b, %r17b, %r17b
	sub	%r18b, %r17b, %r17b
	sub	%r18b, %r18b, %r17b

	shl	$1, %dl
	shl	$1, %dx
	shl	$1, %edx
	shl	$1, %rdx

	shl	%dl
	shl	%dx
	shl	%edx
	shl	%rdx

	shl	$1, %dl, %al
	shl	$1, %dx, %ax
	shl	$1, %edx, %eax
	shl	$1, %rdx, %rax

	sal	$1, %dl
	sal	$1, %dx
	sal	$1, %edx
	sal	$1, %rdx

	sal	%dl
	sal	%dx
	sal	%edx
	sal	%rdx

	sal	$1, %dl, %al
	sal	$1, %dx, %ax
	sal	$1, %edx, %eax
	sal	$1, %rdx, %rax
