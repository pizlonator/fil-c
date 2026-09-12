# GC pressure with NESTED local calls: the caller loops calling subouter while
# the pointer array (%r13), the count (%r15), the index (%r12) and the running
# sum (%rbx) stay live. subouter loads one element pointer and loops calling
# subinner with that pointer (%r11) live across the inner call; subinner
# performs a `.alloca` (a real GC allocation, with its pollcheck) on every
# call and chases the pointer with `;! load ptr`. If any clone's roots,
# spills, or the in-clone allocations corrupt a live pointer, the sum
# mismatches. Inner trip count is 3, so each outer element triggers 3
# allocations.
	.text
	.globl	lc_gc2_sum
	.type	lc_gc2_sum, @function
lc_gc2_sum:                     ;! long(ptr,long)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r15
	movq	%rdi, %r13
	movq	%rsi, %r15
	xorq	%r12, %r12
	xorq	%rbx, %rbx
.Lgc2_loop:
	cmpq	%r15, %r12
	jge	.Lgc2_done
	call	subouter
	incq	%r12
	jmp	.Lgc2_loop
.Lgc2_done:
	movq	%rbx, %rax
	popq	%r15
	popq	%r13
	popq	%r12
	popq	%rbx
	ret
	.size	lc_gc2_sum, .-lc_gc2_sum
	.type	subouter, @function
subouter:
	movq	(%r13,%r12,8), %r11  ;! load ptr
	xorq	%r10, %r10
.Linner_loop:
	cmpq	$3, %r10
	jge	.Linner_done
	call	subinner
	incq	%r10
	jmp	.Linner_loop
.Linner_done:
	ret
	.size	subouter, .-subouter
	.type	subinner, @function
subinner:
	.alloca	$64, $16, %fil_buf
	movzbq	(%r11), %rax
	movzbq	1(%r11), %r8
	addq	%r8, %rax
	movzbq	2(%r11), %r8
	addq	%r8, %rax
	movzbq	3(%r11), %r8
	addq	%r8, %rax
	movq	%rax, 0(%fil_buf)
	movq	0(%fil_buf), %rax
	addq	%rax, %rbx
	ret
	.size	subinner, .-subinner
	.section	.note.GNU-stack,"",@progbits
