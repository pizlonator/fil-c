	.text
# Runtime proof for the aesni-mb 8x constant-address re-sink
# (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl, enc8x/dec8x):
# output positions live in frame slots as real capabilities
# (`;! store ptr` / `;! load ptr`), input pointers advance with plain
# `lea 16(inptr),inptr`, and each iteration re-sinks through branches --
# `jl` over `leaq sink,inptr` (input live while counter>1) and `jle` over
# `leaq sink,outcap` (output live while counter>=1) -- so a sunk input
# always reads [base+16,base+32) and a sunk output always writes
# [base,base+16), no matter how far the pointers advanced while live.
# The per-iteration re-sink (not the advance) dominates: the bumped sunk
# value is never dereferenced. The chained xor stands in for the AES
# rounds; counters clamp at 0 like the .pl's vpcmpgtd/vpaddd decrement.
	.globl	mb8x_run
	.type	mb8x_run, @function
mb8x_run:                       ;! void(ptr)
	# %rdi = struct mb8x_job *: in[2]@0, out[2]@16, n[2]@32, iv[2][16]@40.
	pushq	%rbx
	pushq	%r8
	pushq	%r9
	subq	$32, %rsp			# slots: cnt0@0 cnt1@4 outcap0@8 outcap1@16
	xorl	%edx, %edx			# num (max) = 0
	leaq	mb8x_sink(%rip), %rbx
	# stream 0 prologue (note: the count in %eax is consumed before %rax
	# is reused for the output capability)
	movl	32(%rdi), %eax
	movq	0(%rdi), %r8			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	testl	%eax, %eax
	movl	%eax, 0(%rsp)
	cmovle	%rbx, %r8
	movq	16(%rdi), %rax			;! load ptr
	movq	%rax, 8(%rsp)			;! store ptr
	movdqu	40(%rdi), %xmm2
	movdqu	(%r8), %xmm0
	pxor	%xmm0, %xmm2
	# stream 1 prologue
	movl	36(%rdi), %eax
	movq	8(%rdi), %r9			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	testl	%eax, %eax
	movl	%eax, 4(%rsp)
	cmovle	%rbx, %r9
	movq	24(%rdi), %rax			;! load ptr
	movq	%rax, 16(%rsp)			;! store ptr
	movdqu	56(%rdi), %xmm3
	movdqu	(%r9), %xmm0
	pxor	%xmm0, %xmm3
	testl	%edx, %edx
	jz	.Lmb8x_done
	movl	$1, %ecx
	.align	16
.Lmb8x_loop:
	# stream 0
	movq	8(%rsp), %rax			;! load ptr
	cmpl	0(%rsp), %ecx
	jl	.Lmb8x_cin0
	leaq	mb8x_sink(%rip), %r8
.Lmb8x_cin0:
	jle	.Lmb8x_cout0
	leaq	mb8x_sink(%rip), %rax
.Lmb8x_cout0:
	movdqu	16(%r8), %xmm0
	movq	%rax, 8(%rsp)			;! store ptr
	movups	%xmm2, (%rax)
	pxor	%xmm0, %xmm2
	leaq	16(%rax), %rax
	movq	%rax, 8(%rsp)			;! store ptr
	leaq	16(%r8), %r8
	# stream 1
	movq	16(%rsp), %rax			;! load ptr
	cmpl	4(%rsp), %ecx
	jl	.Lmb8x_cin1
	leaq	mb8x_sink(%rip), %r9
.Lmb8x_cin1:
	jle	.Lmb8x_cout1
	leaq	mb8x_sink(%rip), %rax
.Lmb8x_cout1:
	movdqu	16(%r9), %xmm0
	movq	%rax, 16(%rsp)			;! store ptr
	movups	%xmm3, (%rax)
	pxor	%xmm0, %xmm3
	leaq	16(%rax), %rax
	movq	%rax, 16(%rsp)			;! store ptr
	leaq	16(%r9), %r9
	cmpl	$0, 0(%rsp)
	jle	.Lmb8x_n0
	decl	0(%rsp)
.Lmb8x_n0:
	cmpl	$0, 4(%rsp)
	jle	.Lmb8x_n1
	decl	4(%rsp)
.Lmb8x_n1:
	decl	%edx
	jnz	.Lmb8x_loop
.Lmb8x_done:
	addq	$32, %rsp
	popq	%r9
	popq	%r8
	popq	%rbx
	ret
	.size	mb8x_run, .-mb8x_run
	.comm	mb8x_sink,32,16
	.section	.note.GNU-stack,"",@progbits
