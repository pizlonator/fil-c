# Intel-syntax twin of sarcasm-mont5-dispatch-att: the pristine
# bn_mul_mont_gather5 -> .Lmul4x_enter -> (and/cmp/je) .Lmulx4x_enter dispatch,
# with the caller-defined %r11d web consumed inside the clone.
	.intel_syntax noprefix
	.text
	.globl	m5_top
	.type	m5_top, @function
m5_top:                         ;! long(long,long,long)
	mov	r11d, edi
	jmp	.Lm5_4x_enter
	.size	m5_top, .-m5_top
	.globl	m5_4x
	.type	m5_4x, @function
m5_4x:                          ;! long(long,long,long)
	nop
.Lm5_4x_enter:
	and	r11d, 0x80108
	cmp	r11d, 0x80108
	je	.Lm5_x4x_enter
	# 4x body: a*4 + b
	mov	rax, rsi
	shl	rax, 2
	add	rax, rdx
	ret
	.size	m5_4x, .-m5_4x
	.globl	m5_x4x
	.type	m5_x4x, @function
m5_x4x:                         ;! long(long,long,long)
	mov	rax, rsp
.Lm5_x4x_enter:
	# x4x body: a*8 + b + 1000
	mov	rax, rsi
	shl	rax, 3
	add	rax, rdx
	add	rax, 1000
	ret
	.size	m5_x4x, .-m5_x4x
	.section	.note.GNU-stack,"",@progbits
