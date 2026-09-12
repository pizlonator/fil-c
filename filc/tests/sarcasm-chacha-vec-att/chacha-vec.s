	.file	"chacha-vec.c"
	.text
	# Self-contained vector+scalar mix proof (fully self-contained): a 64-byte
	# keystream block is staged on the frame with the store mix a
	# vectorized tail sees — four 8-byte GPR stores over bytes 0..32,
	# one ALIGNED 16-byte vector store over bytes 32..48, and two 8-byte
	# GPR stores over bytes 48..64 — then consumed as eight static 8-byte
	# windows while the inp/out pointers are bumped. The vector store
	# uses the ALIGNED form (`movdqa`): the frame slot is aligned by
	# construction (`subq $72` from an entry rsp with rsp%16==8 leaves
	# rsp%16==0, so 32(%rsp) is 16-aligned). 8-byte GPR (movq) stores are
	# consumed with same-width 8-byte window loads (each window word is
	# then drained a byte at a time with `shrq $8`, exactly like the
	# 4-byte windows of sarcasm-chacha-tail-att drain with `shrq $8`):
	# same-offset, same-width loads are what the GPR virtualization
	# model feeds exactly. The driver checks every length 0..64 against
	# a plain-C ref (src ^ blk) with the overstep check folded in
	# (bytes past the length must stay at the sentinel).
	.globl	chacha_vec_xor
	.type	chacha_vec_xor, @function
chacha_vec_xor:                 ;! void(ptr, ptr, ptr, size_t)
	# %rdi=dst %rsi=src %rdx=blk %rcx=n. %r8 counts down; %r10d is the
	# repurposed per-window index; %rcx is dead after this move, exactly
	# like a consumed key pointer.
	testq	%rcx, %rcx
	jz	.Lvec_ret
	movq	%rcx, %r8
	subq	$72, %rsp
	movq	0(%rdx), %rax
	movq	%rax, 0(%rsp)
	movq	8(%rdx), %rax
	movq	%rax, 8(%rsp)
	movq	16(%rdx), %rax
	movq	%rax, 16(%rsp)
	movq	24(%rdx), %rax
	movq	%rax, 24(%rsp)
	movdqu	32(%rdx), %xmm0
	movdqa	%xmm0, 32(%rsp)
	movq	48(%rdx), %rax
	movq	%rax, 48(%rsp)
	movq	56(%rdx), %rax
	movq	%rax, 56(%rsp)
	movq	0(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_0:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_0
	movq	8(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_8:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_8
	movq	16(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_16:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_16
	movq	24(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_24:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_24
	movq	32(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_32:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_32
	movq	40(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_40:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_40
	movq	48(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_48:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_48
	movq	56(%rsp), %rcx
	movl	$8, %r10d
.Lw_vec_56:
	movzbl	(%rsi), %eax
	xorb	%cl, %al
	movb	%al, (%rdi)
	shrq	$8, %rcx
	leaq	1(%rsi), %rsi
	leaq	1(%rdi), %rdi
	decq	%r8
	jz	.Lwin_vec_done
	decl	%r10d
	jnz	.Lw_vec_56
.Lwin_vec_done:
	addq	$72, %rsp
.Lvec_ret:
	ret
	.size	chacha_vec_xor, .-chacha_vec_xor
	.section	.note.GNU-stack,"",@progbits
