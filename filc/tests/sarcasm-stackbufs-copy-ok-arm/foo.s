	.arch armv8-a
	.file	"stackbufs.c"
	.text
	.align	2
	.p2align 4,,11
	.global	foo
	.type	foo, %function
foo:                            ;! void(ptr, size_t)
.LFB16:
	.cfi_startproc
	sub	sp, sp, #800
	.cfi_def_cfa_offset 800
	mov	x4, sp          ;! alloca result size=400
	add	x5, sp, 400     ;! alloca result size=400
	cbz	x1, .L1
	sub	x2, x1, #2
	add	x3, x0, 4
	mov	x0, x2
	.p2align 3,,7
.L3:
	add	x1, x4, x0, lsl 2
	ldr	w6, [x3, x0, lsl 2]
	str	w6, [x1, 4]
	sub	x0, x0, #1
	cmn	x0, #2
	bne	.L3
	mov	x1, x2
	.p2align 3,,7
.L4:
	lsl	x0, x1, 2
	add	x6, x4, x0
	add	x0, x5, x0
	ldr	w6, [x6, 4]
	str	w6, [x0, 4]
	sub	x1, x1, #1
	cmn	x1, #2
	bne	.L4
	.p2align 3,,7
.L5:
	add	x0, x5, x2, lsl 2
	ldr	w0, [x0, 4]
	str	w0, [x3, x2, lsl 2]
	sub	x2, x2, #1
	cmn	x2, #2
	bne	.L5
.L1:
	add	sp, sp, 800
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE16:
	.size	foo, .-foo
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
