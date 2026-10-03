# Intel-syntax twin of sarcasm-b2-nested-je-att.
	.intel_syntax noprefix
	.text
	.globl	cn_top
	.type	cn_top, @function
cn_top:                         ;! long(long,long,long)
	mov	r10, rsi
	mov	r11, rdx
	jmp	.Lcn_b
	.size	cn_top, .-cn_top
	.globl	cn_b
	.type	cn_b, @function
cn_b:                           ;! long(long,long,long)
	nop
.Lcn_b:
	cmp	r10, r11
	je	.Lcn_c
	# b != c: a + 2*b
	lea	rax, [rdi + r10*2]
	ret
	.size	cn_b, .-cn_b
	.globl	cn_c
	.type	cn_c, @function
cn_c:                           ;! long(long,long,long)
	mov	rax, rsp
.Lcn_c:
	cmp	r10, rdi
	je	.Lcn_d
	cmp	r10, 11
	je	cn_e
	# neither: a + b
	mov	rax, rdi
	add	rax, r10
	ret
	.size	cn_c, .-cn_c
	.globl	cn_d
	.type	cn_d, @function
cn_d:                           ;! long(long,long,long)
	nop
.Lcn_d:
	# 4*c + a - b
	lea	rax, [r11 + r11*2]
	add	rax, r11
	add	rax, rdi
	sub	rax, r10
	ret
	.size	cn_d, .-cn_d
	.globl	cn_e
	.type	cn_e, @function
cn_e:                           ;! long(long,long,long)
	# a + 77 + c
	mov	rax, rdx
	add	rax, 77
	add	rax, rdi
	ret
	.size	cn_e, .-cn_e
	.section	.note.GNU-stack,"",@progbits
