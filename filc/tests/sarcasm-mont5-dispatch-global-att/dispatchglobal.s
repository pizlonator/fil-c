# The bn_mul_mont_gather5 dispatch flavor with the REAL global load: the
# caller reads the dispatch word from a global (OPENSSL_ia32cap_P+8 style,
# rip-relative with a dispSym) into %r11d and unconditionally joins mg_4x's
# region; the clone does and/cmp on that web and conditionally exits (je)
# into mg_x4x's entry-adjacent label. Additional jumpers exercise the jne
# polarity against other global words (first-level sites).
	.text
	.globl	mg_top
	.type	mg_top, @function
mg_top:                         ;! long(long,long,long)
	# the exact bn_mul_mont_gather5 shape: global word +8 selects.
	movl	mg_cap+8(%rip), %r11d
	jmp	.Lmg_4x_enter
	.size	mg_top, .-mg_top
	.globl	mg_top4
	.type	mg_top4, @function
mg_top4:                        ;! long(long,long,long)
	# same join, but the loaded word is 0: the clone's je is NOT taken.
	movl	mg_cap3(%rip), %r11d
	jmp	.Lmg_4x_enter
	.size	mg_top4, .-mg_top4
	.globl	mg_top_jne
	.type	mg_top_jne, @function
mg_top_jne:                     ;! long(long,long,long)
	# first-level jne against a matching global word: NOT taken, then the
	# inline je dispatch fires into the x4x region.
	movl	mg_cap2(%rip), %r11d
	cmpl	$0x80108, %r11d
	jne	.Lmg_x4x_enter
	andl	$0x80108, %r11d
	cmpl	$0x80108, %r11d
	je	.Lmg_x4x_enter
	# dead: both dispatches decided above
	movq	$777, %rax
	ret
	.size	mg_top_jne, .-mg_top_jne
	.globl	mg_4x
	.type	mg_4x, @function
mg_4x:                          ;! long(long,long,long)
	nop
.Lmg_4x_enter:
	andl	$0x80108, %r11d
	cmpl	$0x80108, %r11d
	je	.Lmg_x4x_enter
	# 4x body: a*4 + b
	movq	%rsi, %rax
	shlq	$2, %rax
	addq	%rdx, %rax
	ret
	.size	mg_4x, .-mg_4x
	.globl	mg_x4x
	.type	mg_x4x, @function
mg_x4x:                         ;! long(long,long,long)
	movq	%rsp, %rax
.Lmg_x4x_enter:
	# x4x body: a*8 + b + 1000
	movq	%rsi, %rax
	shlq	$3, %rax
	addq	%rdx, %rax
	addq	$1000, %rax
	ret
	.size	mg_x4x, .-mg_x4x
	.data
	.p2align	3
mg_cap:
	.long	0x11111111
	.long	0x22222222
	.long	0x80108		# the dispatch word at +8
	.long	0x44444444
	.p2align	2
mg_cap2:
	.long	0x80108
	.p2align	2
mg_cap3:
	.long	0
	.section	.note.GNU-stack,"",@progbits
