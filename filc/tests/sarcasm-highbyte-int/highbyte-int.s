	.intel_syntax noprefix
# Intel-syntax twin of sarcasm-highbyte-att (see it for the semantics notes):
# high-byte reads extract bits 8-15 of the enclosing register's web; writes
# merge into bits 8-15, preserving every other bit.
	.text
	.globl	hb_ops
	.type	hb_ops, @function
hb_ops:                         ;! void(ptr)
	endbr64
	mov	r12, 0x1111111111111111
	mov	r13, 0x2222222222222222
	mov	r14, 0x3333333333333333
	mov	r15, 0x4444444444444444
	mov	rbp, 0x5555555555555555
	mov	rax, 0x1122334455667788
	mov	rcx, 0x2233445566778899
	mov	rdx, 0x33445566778899AA
	mov	rbx, 0x445566778899AABB
	movzx	esi, ah
	mov	DWORD PTR [rdi], esi
	movzx	esi, ch
	mov	DWORD PTR [rdi + 4], esi
	movzx	esi, dh
	mov	DWORD PTR [rdi + 8], esi
	movzx	esi, bh
	mov	DWORD PTR [rdi + 12], esi
	mov	ah, cl
	mov	QWORD PTR [rdi + 16], rax
	mov	al, dh
	mov	QWORD PTR [rdi + 24], rax
	mov	QWORD PTR [rdi + 32], rcx
	mov	ch, bh
	mov	QWORD PTR [rdi + 40], rcx
	mov	dh, 0x5A
	mov	QWORD PTR [rdi + 48], rdx
	and	ah, 0x0F
	mov	QWORD PTR [rdi + 56], rax
	or	ch, 0x10
	mov	QWORD PTR [rdi + 64], rcx
	cmp	ah, 0x09
	setne	r11b
	movzx	eax, r11b
	mov	QWORD PTR [rdi + 72], rax
	test	ch, 0x04
	setz	r11b
	movzx	eax, r11b
	mov	QWORD PTR [rdi + 80], rax
	mov	rax, 0x0102030405060708
	inc	ah
	mov	QWORD PTR [rdi + 88], rax
	dec	dh
	mov	QWORD PTR [rdi + 96], rdx
	neg	bh
	mov	QWORD PTR [rdi + 104], rbx
	mov	rax, 0x1111111111112233
	mov	rcx, 0x2222222222224455
	xchg	ah, ch
	mov	QWORD PTR [rdi + 112], rax
	mov	QWORD PTR [rdi + 120], rcx
	movzx	esi, ch
	mov	DWORD PTR [rdi + 128], esi
	mov	QWORD PTR [rdi + 136], r12
	mov	QWORD PTR [rdi + 144], r13
	mov	QWORD PTR [rdi + 152], r14
	mov	QWORD PTR [rdi + 160], r15
	mov	QWORD PTR [rdi + 168], rbp
	ret
	.size	hb_ops, .-hb_ops

	# Intel-syntax twin of hb_xor_ah (see sarcasm-highbyte-att).
	.globl	hb_xor_ah
	.type	hb_xor_ah, @function
hb_xor_ah:                      ;! long(long)
	endbr64
	mov	rcx, rdi
	mov	rax, 0x1122334455667788
	xor	rax, rax
	mov	ah, cl
	ret
	.size	hb_xor_ah, .-hb_xor_ah
	.section	.note.GNU-stack,"",@progbits
