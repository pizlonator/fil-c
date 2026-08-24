	.arch armv8-a
	.file	"alloca.c"
	.text
	.align	2
	.global	foo
	.type	foo, %function
foo:                            ;! void(ptr, size_t)
.LFB6:
	.cfi_startproc
	stp	x29, x30, [sp, -64]!
	.cfi_def_cfa_offset 64
	.cfi_offset 29, -64
	.cfi_offset 30, -56
	mov	x29, sp
	.cfi_def_cfa_register 29
	str	x0, [x29, 24]
	str	x1, [x29, 16]
	ldr	x0, [x29, 16]
	lsl	x0, x0, 2
	add	x0, x0, 15
	lsr	x0, x0, 4
	lsl	x0, x0, 4
	sub	sp, sp, x0      ;! alloca size (x)
	mov	x0, sp          ;! alloca result (x)
	add	x0, x0, 15
	lsr	x0, x0, 4
	lsl	x0, x0, 4
	str	x0, [x29, 40]
	ldr	x0, [x29, 16]
	str	x0, [x29, 56]
	b	.L2
.L3:
	ldr	x0, [x29, 56]
	lsl	x0, x0, 2
	ldr	x1, [x29, 24]
	add	x1, x1, x0
	ldr	x0, [x29, 56]
	lsl	x0, x0, 2
	ldr	x2, [x29, 40]
	add	x0, x2, x0
	ldr	w1, [x1]
	str	w1, [x0]
.L2:
	ldr	x0, [x29, 56]
	sub	x1, x0, #1
	str	x1, [x29, 56]
	cmp	x0, 0
	bne	.L3
	ldr	x0, [x29, 16]
	str	x0, [x29, 48]
	b	.L4
.L5:
	ldr	x0, [x29, 48]
	lsl	x0, x0, 2
	ldr	x1, [x29, 40]
	add	x1, x1, x0
	ldr	x0, [x29, 48]
	lsl	x0, x0, 2
	ldr	x2, [x29, 24]
	add	x0, x2, x0
	ldr	w1, [x1]
	str	w1, [x0]
.L4:
	ldr	x0, [x29, 48]
	sub	x1, x0, #1
	str	x1, [x29, 48]
	cmp	x0, 0
	bne	.L5
	nop
	nop
	mov	sp, x29
	ldp	x29, x30, [sp], 64
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa 31, 0
	ret
	.cfi_endproc
.LFE6:
	.size	foo, .-foo
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
