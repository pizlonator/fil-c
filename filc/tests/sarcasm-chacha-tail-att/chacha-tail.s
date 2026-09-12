	.file	"chacha-tail.c"
	.text
	# Runtime proof for the SARCASM-only byte tails in
	# projects/openssl-3.6.4/crypto/chacha/asm/chacha-x86_64.pl
	# (chacha_tail_sarcasm): a 64-byte keystream block is staged on the
	# frame with the REAL store mix the tails see — twelve 4-byte GPR
	# stores over bytes 0..32 and 48..64 (exactly the ctr32 `.Ltail`
	# `mov @x[i],4*N(%rsp)` shape: a GPR-only virtualized range, where
	# each 4-byte window load lands inside a single stored range) plus
	# one ALIGNED 16-byte vector store over bytes 32..48 (exactly the
	# ctr32 `.Ltail` `movdqa %xmm1,4*8(%rsp)` / ssse3 `movdqa` shape: a
	# materialized range, proving 4-byte loads inside 16-byte vector
	# stores) — then consumed as sixteen static 4-byte windows while the
	# inp/out pointers are bumped. The vector store uses the ALIGNED form
	# (`movdqa`, like the real tails — not the synthetic `movdqu` the
	# test used before): the frame slot is aligned by construction
	# (`subq $72` from an entry rsp with rsp%16==8 leaves rsp%16==0, so
	# 32(%rsp) is 16-aligned). 8-byte GPR (movq) edges are DELIBERATELY
	# not used even though they would also be "a mix": verified 2026-09
	# that movq stores + 4-byte window loads miscompile in the model
	# (a 4-byte load at offset 4 of an 8-byte store at offset 0 reads a
	# half-stale word — GPR ranges virtualize per-offset, so only a
	# same-offset 4-byte store feeds a 4-byte load; see the .pl's
	# "Windows are 4 bytes wide" comment). The movl edges below ARE the
	# real GPR half of the mix. chacha_tail_ecx uses the %ecx scratch of the ssse3/4x/4xop/
	# 8x/avx512/avx512vl/16x/8xvl tails (whose pristine tails already
	# clobber %ecx, so no 64-bit value in %rcx is live there);
	# chacha_tail_edx uses the %edx scratch of the ChaCha20_ctr32 tail
	# (whose pristine tail already clobbers %edx). The driver checks
	# every tail length 0..64 for both.
	.globl	chacha_tail_ecx
	.type	chacha_tail_ecx, @function
chacha_tail_ecx:                ;! void(ptr, ptr, ptr, size_t)
	# %rdi=dst %rsi=src %rdx=blk %rcx=n. %r8 counts down like the
	# pristine `dec $len`; %r10d is the repurposed index (re-inited per
	# window); %rcx is dead after this move, exactly like the key
	# pointer it stands in for.
	testq	%rcx, %rcx
	jz	.Lecx_ret
	movq	%rcx, %r8
	subq	$72, %rsp
	movl	0(%rdx), %eax
	movl	%eax, 0(%rsp)
	movl	4(%rdx), %eax
	movl	%eax, 4(%rsp)
	movl	8(%rdx), %eax
	movl	%eax, 8(%rsp)
	movl	12(%rdx), %eax
	movl	%eax, 12(%rsp)
	movl	16(%rdx), %eax
	movl	%eax, 16(%rsp)
	movl	20(%rdx), %eax
	movl	%eax, 20(%rsp)
	movl	24(%rdx), %eax
	movl	%eax, 24(%rsp)
	movl	28(%rdx), %eax
	movl	%eax, 28(%rsp)
	movdqu	32(%rdx), %xmm0
	movdqa	%xmm0, 32(%rsp)
	movl	48(%rdx), %eax
	movl	%eax, 48(%rsp)
	movl	52(%rdx), %eax
	movl	%eax, 52(%rsp)
	movl	56(%rdx), %eax
	movl	%eax, 56(%rsp)
	movl	60(%rdx), %eax
	movl	%eax, 60(%rsp)
	movl	0(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_0:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_0
	movl	4(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_4:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_4
	movl	8(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_8:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_8
	movl	12(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_12:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_12
	movl	16(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_16:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_16
	movl	20(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_20:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_20
	movl	24(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_24:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_24
	movl	28(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_28:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_28
	movl	32(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_32:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_32
	movl	36(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_36:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_36
	movl	40(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_40:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_40
	movl	44(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_44:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_44
	movl	48(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_48:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_48
	movl	52(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_52:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_52
	movl	56(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_56:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_56
	movl	60(%rsp),%ecx
	movl	$4,%r10d
.Lw_ecx_60:
	movzbl	(%rsi),%eax
	xorb	%cl,%al
	movb	%al,(%rdi)
	shrq	$8,%rcx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_ecx_done
	decl	%r10d
	jnz	.Lw_ecx_60
.Lwin_ecx_done:
	addq	$72, %rsp
.Lecx_ret:
	ret
	.size	chacha_tail_ecx, .-chacha_tail_ecx
	.globl	chacha_tail_edx
	.type	chacha_tail_edx, @function
chacha_tail_edx:                ;! void(ptr, ptr, ptr, size_t)
	# Same shape with the %edx scratch of the ChaCha20_ctr32 tail.
	testq	%rcx, %rcx
	jz	.Ledx_ret
	movq	%rcx, %r8
	subq	$72, %rsp
	movl	0(%rdx), %eax
	movl	%eax, 0(%rsp)
	movl	4(%rdx), %eax
	movl	%eax, 4(%rsp)
	movl	8(%rdx), %eax
	movl	%eax, 8(%rsp)
	movl	12(%rdx), %eax
	movl	%eax, 12(%rsp)
	movl	16(%rdx), %eax
	movl	%eax, 16(%rsp)
	movl	20(%rdx), %eax
	movl	%eax, 20(%rsp)
	movl	24(%rdx), %eax
	movl	%eax, 24(%rsp)
	movl	28(%rdx), %eax
	movl	%eax, 28(%rsp)
	movdqu	32(%rdx), %xmm0
	movdqa	%xmm0, 32(%rsp)
	movl	48(%rdx), %eax
	movl	%eax, 48(%rsp)
	movl	52(%rdx), %eax
	movl	%eax, 52(%rsp)
	movl	56(%rdx), %eax
	movl	%eax, 56(%rsp)
	movl	60(%rdx), %eax
	movl	%eax, 60(%rsp)
	movl	0(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_0:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_0
	movl	4(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_4:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_4
	movl	8(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_8:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_8
	movl	12(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_12:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_12
	movl	16(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_16:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_16
	movl	20(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_20:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_20
	movl	24(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_24:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_24
	movl	28(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_28:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_28
	movl	32(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_32:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_32
	movl	36(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_36:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_36
	movl	40(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_40:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_40
	movl	44(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_44:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_44
	movl	48(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_48:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_48
	movl	52(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_52:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_52
	movl	56(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_56:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_56
	movl	60(%rsp),%edx
	movl	$4,%r10d
.Lw_edx_60:
	movzbl	(%rsi),%eax
	xorb	%dl,%al
	movb	%al,(%rdi)
	shrq	$8,%rdx
	leaq	1(%rsi),%rsi
	leaq	1(%rdi),%rdi
	decq	%r8
	jz	.Lwin_edx_done
	decl	%r10d
	jnz	.Lw_edx_60
.Lwin_edx_done:
	addq	$72, %rsp
.Ledx_ret:
	ret
	.size	chacha_tail_edx, .-chacha_tail_edx

# Upper-32 clobber proof for the tail scratch model (item 8): the sequence
# below is shaped exactly like the generated tails — a `movl` from a frame
# slot (which zeroes the scratch register's upper 32 in hardware) followed
# by the tail's `shrq $8` byte loop — but BOTH scratch registers enter
# holding a LIVE 64-bit sentinel with a nonzero upper half, and four more
# copies of the sentinel are kept live across the sequence in other
# registers (register pressure, so the live values must ride the
# spill/restore path the full-kill model relies on).
#
# What this proves, and what is proven by audit instead. A 32-bit GPR
# destination is a FULL 64-bit web kill in sarcasm's model
# (x86_64_isa.partialRegWrite is width<32 only, so `movl` is a pure def of
# its web — matching the hardware zero-extension), hence the allocator
# spills (rather than assumes preserved) any 64-bit value live across the
# scratch. That kill direction is proven by code audit: no black-box
# runtime test can distinguish "kill full 64" from "kill low 32, keep
# upper" for the clobbered register itself (both render the identical
# `movl`, and the hardware zeroes the upper either way), so the audit cite
# above IS the proof of the kill. This test pins the end-to-end behavior
# with live uppers present — the shape that exposes any regression that
# ever trusts upper-half preservation: the scratch results must show
# zeroed uppers (0x11223344>>8 = 0x00112233, not 0x9E3779XX_00112233) and
# every sentinel copy kept live across the scratch must come back intact.
# The real-site half ("no 64-bit value is live at any actual tail") is
# proven in projects/openssl-3.6.4/crypto/chacha/asm/chacha-x86_64.pl's
# chacha_tail_sarcasm comment: the scratch is exactly the register the
# pristine tail already clobbers (movzb to a 32-bit dest zeroes the upper
# 32 in both versions — a full 64-bit kill in both), %rcx's key pointer is
# fully consumed once the block is stored, and ctr32's %rdx length is
# consumed (the remainder lives in %rbp).
	.globl	chacha_tail_upper32
	.type	chacha_tail_upper32, @function
chacha_tail_upper32:            ;! long(long, ptr)
	# %rdi = sentinel S (nonzero upper 32), %rsi = 64-byte block.
	movq	%rdi, %rcx
	movq	%rdi, %rdx
	movq	%rdi, %r10
	movq	%rdi, %r11
	movq	%rdi, %r8
	movq	%rdi, %r9
	subq	$64, %rsp
	movl	0(%rsi), %eax
	movl	%eax, 0(%rsp)
	movl	4(%rsi), %eax
	movl	%eax, 4(%rsp)
	movl	0(%rsp), %ecx
	shrq	$8, %rcx
	movl	4(%rsp), %edx
	shrq	$8, %rdx
	movq	%rcx, %rax
	addq	%rdx, %rax
	addq	%r10, %rax
	addq	%r11, %rax
	addq	%r8, %rax
	addq	%r9, %rax
	addq	$64, %rsp
	ret
	.size	chacha_tail_upper32, .-chacha_tail_upper32
	.section	.note.GNU-stack,"",@progbits
