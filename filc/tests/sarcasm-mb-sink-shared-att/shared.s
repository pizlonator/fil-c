	.text
# Sharing/disjointness proof for the single aesni-mb sink object
# (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl): two streams
# share one 32-byte `.comm` while one of them is fully sunk. The loop head
# rebuilds sink=base+16-offset every iteration, so sunk loads always read
# [base+16,base+32) and sunk stores always write [base,base+16) -- the
# windows are disjoint, hence dummy stores can never disturb dummy loads
# (and a hypothetical single-16B-sink "fix" with no +16 bias would fail
# the load-window assertions below). GPR traffic here complements the
# vector traffic of sarcasm-mb-sink4x-att: the property is about
# addresses, not widths. The stateless xor stands in for the AES rounds.
	.globl	mbsh_run
	.type	mbsh_run, @function
mbsh_run:                       ;! void(ptr)
	# %rdi = struct mbsh_job *: in[2]@0, out[2]@16, n[2]@32.
	pushq	%rbx
	pushq	%rbp
	pushq	%r10
	pushq	%r11
	subq	$16, %rsp			# counter slots (must not clobber saves)
	xorl	%edx, %edx			# num (max) = 0
	leaq	mbsh_sink(%rip), %rbp
	movl	32(%rdi), %eax
	movq	0(%rdi), %r8			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	movq	16(%rdi), %r10			;! load ptr
	testl	%eax, %eax
	movl	%eax, 0(%rsp)
	cmovle	%rbp, %r8
	movl	36(%rdi), %eax
	movq	8(%rdi), %r9			;! load ptr
	cmpl	%edx, %eax
	cmovg	%eax, %edx
	movq	24(%rdi), %r11			;! load ptr
	testl	%eax, %eax
	movl	%eax, 8(%rsp)
	cmovle	%rbp, %r9
	movabsq	$0x0F0E0D0C0B0A0908, %rsi	# K0
	movabsq	$0x8070605040302010, %rdi	# K1 (job dead past this point)
	testl	%edx, %edx
	jz	.Lmbsh_done
	xorq	%rbx, %rbx
	.align	16
.Lmbsh_loop:
	addq	$16, %rbx
	movl	$1, %ecx
	leaq	mbsh_sink+16(%rip), %rbp
	subq	%rbx, %rbp
	# stream 0
	cmpl	0(%rsp), %ecx
	cmovge	%rbp, %r8
	cmovg	%rbp, %r10
	movq	(%r8,%rbx), %rax
	xorq	%rsi, %rax
	movq	%rax, -16(%r10,%rbx)
	movq	8(%r8,%rbx), %rax
	xorq	%rdi, %rax
	movq	%rax, -8(%r10,%rbx)
	# stream 1
	cmpl	8(%rsp), %ecx
	cmovge	%rbp, %r9
	cmovg	%rbp, %r11
	movq	(%r9,%rbx), %rax
	xorq	%rsi, %rax
	movq	%rax, -16(%r11,%rbx)
	movq	8(%r9,%rbx), %rax
	xorq	%rdi, %rax
	movq	%rax, -8(%r11,%rbx)
	cmpl	$0, 0(%rsp)
	jle	.Lmbsh_n0
	decl	0(%rsp)
.Lmbsh_n0:
	cmpl	$0, 8(%rsp)
	jle	.Lmbsh_n1
	decl	8(%rsp)
.Lmbsh_n1:
	decl	%edx
	jnz	.Lmbsh_loop
.Lmbsh_done:
	addq	$16, %rsp
	popq	%r11
	popq	%r10
	popq	%rbp
	popq	%rbx
	ret
	.size	mbsh_run, .-mbsh_run
	.comm	mbsh_sink,32,16
	.section	.note.GNU-stack,"",@progbits
