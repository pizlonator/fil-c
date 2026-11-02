	.text
# Runtime proof for the enc8x offload window annotations
# (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl,
# aesni_multi_cbc_encrypt_avx): the ($offload) accesses carry
# `#! stack buffer (offe, %rsp + 128, %rsp + 192)`, a 64-byte window the
# base (`lea 128(%rsp)`) addresses directly with no ping-pong. The enc8x
# annotation placement needed no fix, but the window it declares is
# load-bearing for the SARCASM path, so it gets the same round-trip
# proof as the dec8x window: vector staging plus the GPR/vector drain
# mix, verified both halves' worth twice over with different blocks.
	.globl	mboffe_run
	.type	mboffe_run, @function
mboffe_run:                     ;! void(ptr, ptr)
	# %rdi = src (64B), %rsi = dst (64B).
	movq	%rsp, %rax
	subq	$192, %rsp
	andq	$-128, %rsp
	movq	%rax, 16(%rsp)			# saved-rsp carrier
	pushq	%rbx
	leaq	128(%rsp), %rbx			# offload area @ +128
	movdqu	0(%rdi), %xmm0
	vmovdqu	%xmm0, 0x00(%rbx)	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movdqu	16(%rdi), %xmm0
	vmovdqu	%xmm0, 0x10(%rbx)	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movdqu	32(%rdi), %xmm0
	vmovdqu	%xmm0, 0x20(%rbx)	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movdqu	48(%rdi), %xmm0
	vmovdqu	%xmm0, 0x30(%rbx)	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	vmovdqu	0x00(%rbx), %xmm0	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movdqu	%xmm0, 0(%rsi)
	vmovdqu	0x10(%rbx), %xmm1	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movdqu	%xmm1, 16(%rsi)
	movl	0x20(%rbx), %eax	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movl	%eax, 32(%rsi)
	movl	0x24(%rbx), %eax	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movl	%eax, 36(%rsi)
	movq	0x28(%rbx), %rax	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movq	%rax, 40(%rsi)
	movdqu	0x30(%rbx), %xmm0	#! stack buffer (offe, %rsp + 128, %rsp + 192)
	movdqu	%xmm0, 48(%rsi)
	popq	%rbx
	movq	16(%rsp), %rax			# carrier recovery
	leaq	(%rax), %rsp
	ret
	.size	mboffe_run, .-mboffe_run
	.section	.note.GNU-stack,"",@progbits
