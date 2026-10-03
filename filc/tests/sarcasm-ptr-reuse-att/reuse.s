# The OpenSSL aesni CBC-decrypt loop8 shape (.Lcbc_decrypt_body and
# .Lcbc_dec_loop8_enter in crypto/aes/asm/aesni-x86_64.pl): %rbp first backs
# up the key-schedule pointer (mov $key,$key_ with $key_=$inp_=%rbp), a
# backend dispatch (cmp/je to the loop6 enter, jmp to the loop8 enter)
# selects the path, and at the multiply-entered loop8 point -- reached both
# from the dispatch and via the loop top -- %rbp is reused as integer
# scratch (mov $-1), folded with the ahead-count test (cmp/adc/and), and
# re-derived from the input pointer (add $inp,$inp_). The loads through the
# re-derived pointer must then check against the input buffer, not the key:
# the key/integer/input defs share one web, so no static lower fits and
# sarcasm widens it to a dynamic (lockstep) lower with the input web as the
# def's source. The loop6 path mirrors the real loop6 (which never reuses
# %rbp): a plain input load, proving the dispatch selects paths correctly.
	.text
	.globl	reuse_alias
	.type	reuse_alias, @function
reuse_alias:                    ;! long(ptr,long,ptr,long,long)
	endbr64
	movq	%rdi, %rbp	# [re-]backup $key [after reassignment]
	xorq	%rax, %rax	# accumulator
	cmpq	$0, %rsi
	je	.Lloop6	# backend dispatch (real: je .Lcbc_dec_loop6_enter)
	jmp	.Lloop8_enter	# (real: jmp .Lcbc_dec_loop8_enter)
	.align	16
.Lloop8:	# loop top (real: store output / advance input, falls through)
	addq	$128, %rdx	# advance input (real: lea 0x80($inp),$inp)
	subq	$1, %r8	# blocks remaining
	jz	.Ldone
.Lloop8_enter:	# dispatch edge + loop-top fallthrough (real: _enter)
	movq	$-1, %rbp
	cmpq	$0x70, %rcx	# is there at least 0x60 bytes ahead (real: cmp $0x70,$len)
	adcq	$0, %rbp
	andq	$128, %rbp
	addq	%rdx, %rbp
	movq	(%rbp), %r10	# checked against the input object, not the key
	orq	%r10, %rax
	subq	$128, %rcx	# consume the ahead-count (exit is on the block count:
	jmp	.Lloop8	# real code branches on the biased len, which needs 3+ blocks
.Lloop6:	# no rbp reuse here, like the real loop6
	movq	(%rdx), %rax	# plain input load
	ret
.Ldone:
	ret
	.size	reuse_alias, .-reuse_alias
	.section	.note.GNU-stack,"",@progbits
