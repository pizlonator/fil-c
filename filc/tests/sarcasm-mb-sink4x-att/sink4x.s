	.text
# Runtime proof for the fixed aesni-mb 4x sink slide
# (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl, enc4x/dec4x):
#
#   leaq sink+16(%rip),sink-reg; sub offset,sink-reg     (loop head)
#   cmpl counter,one; cmovge sink,inptr; cmovg sink,outptr (per stream)
#   load  (inptr,offset)          (next-block input)
#   store cur,-16(outptr,offset)  (current-block output)
#
# so every sunk load lands at sink+16 and every sunk store at sink (via the
# -16 store bias): constant addressing for an unbounded block count with
# disjoint 16-byte windows ([base+16,base+32) loads, [base,base+16)
# stores) inside one shared 32-byte object -- the same 16B separation the
# pristine +16(input)/+0(output) stack slots provide. The chained xor
# (cur ^= next) stands in for the AES rounds; the addressing, the cancels,
# and the slide are the exact .pl shape, including the prologue
# `test; cmovle` empty-stream cancel (whose placeholder value is dead: the
# loop-head cmovs re-sink before any dereference) and the scalar-clamped
# counters (the .pl's pcmpgtd/paddd decrement, clamped at 0).
	.globl	mb4x_run
	.type	mb4x_run, @function
mb4x_run:                       ;! void(ptr)
	# %rdi = struct mb4x_job *: in[4]@0, out[4]@32, n[4]@64, iv[4][16]@80.
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	subq	$64, %rsp
	xorl	%edx, %edx			# num (max) = 0
	leaq	mbsink4(%rip), %rbp		# sink base for prologue cancels
	# stream 0
	movl	64(%rdi), %eax
	movq	0(%rdi), %r8			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	movq	32(%rdi), %r12			;! load ptr
	testl	%eax, %eax
	movl	%eax, 0(%rsp)
	cmovle	%rbp, %r8
	movdqu	80(%rdi), %xmm2
	movdqu	(%r8), %xmm0
	pxor	%xmm0, %xmm2
	# stream 1
	movl	68(%rdi), %eax
	movq	8(%rdi), %r9			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	movq	40(%rdi), %r13			;! load ptr
	testl	%eax, %eax
	movl	%eax, 4(%rsp)
	cmovle	%rbp, %r9
	movdqu	96(%rdi), %xmm3
	movdqu	(%r9), %xmm0
	pxor	%xmm0, %xmm3
	# stream 2
	movl	72(%rdi), %eax
	movq	16(%rdi), %r10			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	movq	48(%rdi), %r14			;! load ptr
	testl	%eax, %eax
	movl	%eax, 8(%rsp)
	cmovle	%rbp, %r10
	movdqu	112(%rdi), %xmm4
	movdqu	(%r10), %xmm0
	pxor	%xmm0, %xmm4
	# stream 3
	movl	76(%rdi), %eax
	movq	24(%rdi), %r11			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	movq	56(%rdi), %r15			;! load ptr
	testl	%eax, %eax
	movl	%eax, 12(%rsp)
	cmovle	%rbp, %r11
	movdqu	128(%rdi), %xmm5
	movdqu	(%r11), %xmm0
	pxor	%xmm0, %xmm5
	testl	%edx, %edx
	jz	.Lmb4x_done
	xorq	%rbx, %rbx			# offset = 0
	jmp	.Lmb4x_chk
	.align	32
.Lmb4x_loop:
	addq	$16, %rbx
	movl	$1, %ecx
	leaq	mbsink4+16(%rip), %rbp
	subq	%rbx, %rbp
	# stream 0
	cmpl	0(%rsp), %ecx
	cmovge	%rbp, %r8
	cmovg	%rbp, %r12
	movdqu	(%r8,%rbx), %xmm0
	movups	%xmm2, -16(%r12,%rbx)
	pxor	%xmm0, %xmm2
	# stream 1
	cmpl	4(%rsp), %ecx
	cmovge	%rbp, %r9
	cmovg	%rbp, %r13
	movdqu	(%r9,%rbx), %xmm0
	movups	%xmm3, -16(%r13,%rbx)
	pxor	%xmm0, %xmm3
	# stream 2
	cmpl	8(%rsp), %ecx
	cmovge	%rbp, %r10
	cmovg	%rbp, %r14
	movdqu	(%r10,%rbx), %xmm0
	movups	%xmm4, -16(%r14,%rbx)
	pxor	%xmm0, %xmm4
	# stream 3
	cmpl	12(%rsp), %ecx
	cmovge	%rbp, %r11
	cmovg	%rbp, %r15
	movdqu	(%r11,%rbx), %xmm0
	movups	%xmm5, -16(%r15,%rbx)
	pxor	%xmm0, %xmm5
	# clamp counters at 0 (the .pl's pcmpgtd/paddd decrement)
	cmpl	$0, 0(%rsp)
	jle	.Lmb4x_n0
	decl	0(%rsp)
.Lmb4x_n0:
	cmpl	$0, 4(%rsp)
	jle	.Lmb4x_n1
	decl	4(%rsp)
.Lmb4x_n1:
	cmpl	$0, 8(%rsp)
	jle	.Lmb4x_n2
	decl	8(%rsp)
.Lmb4x_n2:
	cmpl	$0, 12(%rsp)
	jle	.Lmb4x_n3
	decl	12(%rsp)
.Lmb4x_n3:
	decl	%edx
.Lmb4x_chk:
	testl	%edx, %edx
	jnz	.Lmb4x_loop
.Lmb4x_done:
	addq	$64, %rsp
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret
	.size	mb4x_run, .-mb4x_run
	.comm	mbsink4,32,16
	.section	.note.GNU-stack,"",@progbits
