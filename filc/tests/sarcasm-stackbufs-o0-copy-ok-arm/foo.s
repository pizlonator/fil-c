	.arch armv8-a
	.file	"stackbufs.c"
	.text
	.align	2
	.global	foo
	.type	foo, %function
foo:                            ;! void(ptr, size_t)
.LFB6:
	.cfi_startproc
	sub	sp, sp, #848
	.cfi_def_cfa_offset 848
	str	x0, [sp, 8]
	str	x1, [sp]
	add	x0, sp, 424     ;! alloca result size=400
	add	x0, sp, 24      ;! alloca result size=400
	ldr	x0, [sp]
	str	x0, [sp, 840]
	b	.L2
.L3:
	ldr	x0, [sp, 840]
	lsl	x0, x0, 2
	ldr	x1, [sp, 8]
	add	x0, x1, x0
	ldr	w2, [x0]
	ldr	x0, [sp, 840]
	lsl	x0, x0, 2
	add	x1, sp, 424
	str	w2, [x1, x0]
.L2:
	ldr	x0, [sp, 840]
	sub	x1, x0, #1
	str	x1, [sp, 840]
	cmp	x0, 0
	bne	.L3
	ldr	x0, [sp]
	str	x0, [sp, 832]
	b	.L4
.L5:
	ldr	x0, [sp, 832]
	lsl	x0, x0, 2
	add	x1, sp, 424
	ldr	w2, [x1, x0]
	ldr	x0, [sp, 832]
	lsl	x0, x0, 2
	add	x1, sp, 24
	str	w2, [x1, x0]
.L4:
	ldr	x0, [sp, 832]
	sub	x1, x0, #1
	str	x1, [sp, 832]
	cmp	x0, 0
	bne	.L5
	ldr	x0, [sp]
	str	x0, [sp, 824]
	b	.L6
.L7:
	ldr	x0, [sp, 824]
	lsl	x0, x0, 2
	ldr	x1, [sp, 8]
	add	x0, x1, x0
	ldr	x1, [sp, 824]
	lsl	x1, x1, 2
	add	x2, sp, 24
	ldr	w1, [x2, x1]
	str	w1, [x0]
.L6:
	ldr	x0, [sp, 824]
	sub	x1, x0, #1
	str	x1, [sp, 824]
	cmp	x0, 0
	bne	.L7
	nop
	nop
	add	sp, sp, 848
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE6:
	.size	foo, .-foo
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
