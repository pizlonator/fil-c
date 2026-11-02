	.text
# Runtime proof for the dec8x offload ping-pong annotations
# (projects/sarcastic-openssl/crypto/aes/asm/aesni-mb-x86_64.pl,
# aesni_multi_cbc_decrypt_avx): the ($offload) accesses carry
# `#! stack buffer (offd, %rsp + 192, %rsp + 448)` while $offload flips
# between the IV half (%rsp+192) and the ciphertext half (%rsp+320) via
# `xor $0x80` -- the exact .pl shape, including the frame (sub/and/subs
# + saved-rsp carrier recovery), the base lea, both the plain
# `vmovdqu ($offload)` and the `vpxor ($offload),xmm,xmm` tail forms, and
# the three xor flips (stage-A, stage-B, drain-A, drain-B). The flip
# needs sarcasm's absolute-alignment identity for computed buffer bases
# (output-frame dynamic alignment); see sarcasm-mb-toggle-xor-att.
	.globl	mboffd_run
	.type	mboffd_run, @function
mboffd_run:                     ;! void(ptr, ptr, ptr)
	# %rdi = src (256B), %rsi = dst (256B), %rdx = key (16B).
	movq	%rsp, %rax
	subq	$256, %rsp
	andq	$-256, %rsp
	subq	$192, %rsp
	movq	%rax, 16(%rsp)			# saved-rsp carrier
	movdqu	(%rdx), %xmm7			# key
	leaq	192+128(%rsp), %rbx		# offload base (ciphertext half @ +320)
	# stage ciphertext half from src[0..128)
	movdqu	0(%rdi), %xmm0
	vmovdqu	%xmm0, 0x00(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	16(%rdi), %xmm0
	vmovdqu	%xmm0, 0x10(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	32(%rdi), %xmm0
	vmovdqu	%xmm0, 0x20(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	48(%rdi), %xmm0
	vmovdqu	%xmm0, 0x30(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	64(%rdi), %xmm0
	vmovdqu	%xmm0, 0x40(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	80(%rdi), %xmm0
	vmovdqu	%xmm0, 0x50(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	96(%rdi), %xmm0
	vmovdqu	%xmm0, 0x60(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	112(%rdi), %xmm0
	vmovdqu	%xmm0, 0x70(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	xorq	$0x80, %rbx			# -> IV half @ +192
	# stage IV half from src[128..256)
	movdqu	128(%rdi), %xmm0
	vmovdqu	%xmm0, 0x00(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	144(%rdi), %xmm0
	vmovdqu	%xmm0, 0x10(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	160(%rdi), %xmm0
	vmovdqu	%xmm0, 0x20(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	176(%rdi), %xmm0
	vmovdqu	%xmm0, 0x30(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	192(%rdi), %xmm0
	vmovdqu	%xmm0, 0x40(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	208(%rdi), %xmm0
	vmovdqu	%xmm0, 0x50(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	224(%rdi), %xmm0
	vmovdqu	%xmm0, 0x60(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	240(%rdi), %xmm0
	vmovdqu	%xmm0, 0x70(%rbx)	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	xorq	$0x80, %rbx			# -> ciphertext half @ +320
	# drain ciphertext half to dst[128..256) verbatim (the offload shape)
	vmovdqu	0x00(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 128(%rsi)
	vmovdqu	0x10(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 144(%rsi)
	vmovdqu	0x20(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 160(%rsi)
	vmovdqu	0x30(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 176(%rsi)
	vmovdqu	0x40(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 192(%rsi)
	vmovdqu	0x50(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 208(%rsi)
	vmovdqu	0x60(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 224(%rsi)
	vmovdqu	0x70(%rbx), %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 240(%rsi)
	xorq	$0x80, %rbx			# -> IV half @ +192
	# drain IV half to dst[0..128) xored with the key (the tail shape)
	movdqa	%xmm7, %xmm0
	vpxor	0x00(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 0(%rsi)
	movdqa	%xmm7, %xmm0
	vpxor	0x10(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 16(%rsi)
	movdqa	%xmm7, %xmm0
	vpxor	0x20(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 32(%rsi)
	movdqa	%xmm7, %xmm0
	vpxor	0x30(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 48(%rsi)
	movdqa	%xmm7, %xmm0
	vpxor	0x40(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 64(%rsi)
	movdqa	%xmm7, %xmm0
	vpxor	0x50(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 80(%rsi)
	movdqa	%xmm7, %xmm0
	vpxor	0x60(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 96(%rsi)
	movdqa	%xmm7, %xmm0
	vpxor	0x70(%rbx), %xmm0, %xmm0	#! stack buffer (offd, %rsp + 192, %rsp + 448)
	movdqu	%xmm0, 112(%rsi)
	movq	16(%rsp), %rax			# carrier recovery
	leaq	(%rax), %rsp
	ret
	.size	mboffd_run, .-mboffd_run
	.section	.note.GNU-stack,"",@progbits
