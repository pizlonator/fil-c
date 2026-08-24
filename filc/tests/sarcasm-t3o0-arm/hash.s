	.arch armv8-a
	.file	"test3.c"
	.text
	.align	2
	.global	hash
	.type	hash, %function
hash:   ;! unsigned(ptr)
.LFB6:
	.cfi_startproc
	stp	x29, x30, [sp, -64]!
	.cfi_def_cfa_offset 64
	.cfi_offset 29, -64
	.cfi_offset 30, -56
	mov	x29, sp
	str	x19, [sp, 16]
	.cfi_offset 19, -48
	str	x0, [sp, 40]
	mov	x0, 5381
	str	x0, [sp, 48]
	str	xzr, [sp, 56]
	b	.L2
.L3:
	ldr	x0, [sp, 48]
	lsl	x1, x0, 5
	ldr	x0, [sp, 48]
	add	x19, x1, x0
	ldr	x0, [sp, 40]
	ldr	x0, [x0]        ;! load ptr
	ldr	x1, [sp, 56]
	bl	foo             ;! int(ptr, size_t)
	sxtw	x0, w0
	add	x0, x19, x0
	str	x0, [sp, 48]
	ldr	x0, [sp, 56]
	add	x0, x0, 1
	str	x0, [sp, 56]
.L2:
	ldr	x0, [sp, 40]
	ldr	x0, [x0, 8]
	ldr	x1, [sp, 56]
	cmp	x1, x0
	bcc	.L3
	ldr	x0, [sp, 48]
	ldr	x19, [sp, 16]
	ldp	x29, x30, [sp], 64
	.cfi_restore 30
	.cfi_restore 29
	.cfi_restore 19
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE6:
	.size	hash, .-hash
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
