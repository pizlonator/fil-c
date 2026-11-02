	.text
# Bound-spelling proof for the bsaes wipe loops
# (projects/sarcastic-openssl/crypto/aes/asm/bsaes-x86_64.pl): the gas form
# compares the wipe pointer against the frame-top register
# (`cmp %rax,%rbp; jb`), while the SARCASM form compares against the
# recomputed end=base+size (`cmp %rax,%r11; ja`). With the .pl's
# end=base+size the `ja` loop wipes exactly [base, end) -- the documented
# key-schedule range (see sarcasm-bsaes-wipebound-att). The gas spelling
# is upstream's, kept byte-identical by the patch: with the same
# end-pointer value it wipes exactly the first 32-byte chunk (the `jb`
# exits once rax passes base+32), and agrees with the `ja` spelling only
# when the whole range is one chunk (rounds=1, size 32). This test pins
# both behaviors so the relationship stays explicit.
	.globl	bsaes_wipe_r11
	.type	bsaes_wipe_r11, @function
bsaes_wipe_r11:                 ;! void(ptr, long)
	# %rdi = base, %rsi = rounds. SARCASM spelling (bound in %r11).
	movq	%rdi, %rax
	pxor	%xmm0, %xmm0
	movl	%esi, %r11d
	shlq	$7, %r11
	subq	$96, %r11
	movq	%rdi, %r10
	addq	%r10, %r11			# key schedule end (== %rbp, sans frame-base read)
.Lwipe_r11_loop:
	movdqa	%xmm0, 0x00(%rax)
	movdqa	%xmm0, 0x10(%rax)
	leaq	0x20(%rax), %rax
	cmpq	%rax, %r11
	ja	.Lwipe_r11_loop
	ret
	.size	bsaes_wipe_r11, .-bsaes_wipe_r11
	.globl	bsaes_wipe_rbp
	.type	bsaes_wipe_rbp, @function
bsaes_wipe_rbp:                 ;! void(ptr, ptr)
	# %rdi = base, %rsi = end (the frame-top stand-in). Gas spelling.
	pushq	%rbp
	movq	%rdi, %rax
	pxor	%xmm0, %xmm0
	movq	%rsi, %rbp
.Lwipe_rbp_loop:
	movdqa	%xmm0, 0x00(%rax)
	movdqa	%xmm0, 0x10(%rax)
	leaq	0x20(%rax), %rax
	cmpq	%rax, %rbp
	jb	.Lwipe_rbp_loop
	popq	%rbp
	ret
	.size	bsaes_wipe_rbp, .-bsaes_wipe_rbp
	.section	.note.GNU-stack,"",@progbits
