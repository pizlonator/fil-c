# Intel-syntax twin of sarcasm-b2-reg-web-att.
	.intel_syntax noprefix
	.text
	.globl	rw_top
	.type	rw_top, @function
rw_top:                         ;! long(long,long,long)
	mov	r12, rsi
	mov	r13, rdx
	mov	r11d, edi
	jmp	.Lrw_mid
	.size	rw_top, .-rw_top
	.globl	rw_top2
	.type	rw_top2, @function
rw_top2:                        ;! long(long,long,long)
	mov	r12, rsi
	add	r12, 10
	mov	r13, rdx
	mov	r11d, edi
	jmp	.Lrw_mid
	.size	rw_top2, .-rw_top2
	.globl	rw_owner
	.type	rw_owner, @function
rw_owner:                       ;! long(long,long,long)
	nop
.Lrw_mid:
	add	r12, 1
	add	r11d, 5
	movslq	r10, r11d
	lea	rax, [r12 + r13*2]
	add	rax, r10
	add	rax, rdi
	ret
	.size	rw_owner, .-rw_owner
	.section	.note.GNU-stack,"",@progbits
