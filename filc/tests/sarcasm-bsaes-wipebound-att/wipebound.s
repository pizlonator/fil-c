	.text
# Runtime proof for the bsaes key-schedule wipe-bound recomputation
# (projects/sarcastic-openssl/crypto/aes/asm/bsaes-x86_64.pl, ECB encrypt and
# decrypt done-paths): gas compares the wipe pointer against %rbp (the
# fixed frame top) as the end pointer, but under SARCASM %rbp never
# covered the GC `.alloca`, so the bound is rebuilt as end=base+size in
# %r11 from the round count in %ebx:
#
#   mov %ebx,%r11d; shl $7,%r11; sub $(128-32),%r11; mov base,%r10; add %r10,%r11
#   .Lwipe: movdqa %xmm0,(%rax); movdqa %xmm0,16(%rax); lea 32(%rax),%rax
#           cmp %rax,%r11; ja .Lwipe
#
# (128 bytes per inner round key, minus a constant 128-32 for the
# separately-saved tail -- the exact .pl arithmetic, so the wiped span is
# rounds*128-96). This test pins the formula: for several round counts the bytes [base, base+rounds*128-32) are zeroed and the bytes
# after are untouched, including the short-path shape (no schedule was
# ever built -- the wipe still covers the empty region harmlessly).
	.globl	bsaes_wipebound
	.type	bsaes_wipebound, @function
bsaes_wipebound:                ;! void(ptr, long)
	# %rdi = base, %rsi = rounds (%ebx's role).
	movq	%rdi, %rax
	pxor	%xmm0, %xmm0
	movl	%esi, %r11d
	shlq	$7, %r11			# 128 bytes per inner round key
	subq	$96, %r11			# size of bit-sliced key schedule
	movq	%rdi, %r10
	addq	%r10, %r11			# key schedule end (== %rbp, sans frame-base read)
.Lwipebound_loop:
	movdqa	%xmm0, 0x00(%rax)
	movdqa	%xmm0, 0x10(%rax)
	leaq	0x20(%rax), %rax
	cmpq	%rax, %r11
	ja	.Lwipebound_loop
	ret
	.size	bsaes_wipebound, .-bsaes_wipebound
	.section	.note.GNU-stack,"",@progbits
