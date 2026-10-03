# Intel-syntax twin of sarcasm-b2-flags-att: flags defined in the CALLER,
# consumed by the clone's first branch (je), plus a conditional first-level
# B2 (jne) of the opposite polarity into the same region.
	.intel_syntax noprefix
	.text
	.globl	fb_jmp
	.type	fb_jmp, @function
fb_jmp:                         ;! long(long,long)
	cmp	rdi, rsi
	jmp	.Lfb_mid
	.size	fb_jmp, .-fb_jmp
	.globl	fb_jne
	.type	fb_jne, @function
fb_jne:                         ;! long(long,long)
	cmp	rdi, rsi
	jne	.Lfb_mid
	# fallthrough (equal): a*3 + b
	mov	rax, rdi
	lea	rax, [rax + rax*2]
	add	rax, rsi
	ret
	.size	fb_jne, .-fb_jne
	.globl	fb_owner
	.type	fb_owner, @function
fb_owner:                       ;! long(long,long)
	nop
.Lfb_mid:
	je	.Lfb_eq
	# not equal: a*2 + b
	mov	rax, rdi
	add	rax, rax
	add	rax, rsi
	ret
.Lfb_eq:
	# equal: a*3 + b
	mov	rax, rdi
	lea	rax, [rax + rax*2]
	add	rax, rsi
	ret
	.size	fb_owner, .-fb_owner
	.section	.note.GNU-stack,"",@progbits
