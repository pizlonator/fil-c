# Testcase for forward flow of SCFI information
# and CFG creation as well.  This testcase has two backward edges
# (one of which is a loop) and one exit path.
	.type   foo, %function
foo:
	.cfi_startproc
	stp    x29, x30, [sp, -48]!
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	mov     x29, sp
	.cfi_def_cfa_register 29
	str     x0, [sp, 24]
	adrp    x0, :got:xyz
	str     x0, [sp, 40]
	b       .L7
.L10:
	ldr     x0, [sp, 40]
	ldr     x0, [x0]
	mov     x1, x0
	ldr     x0, [sp, 24]
	bl      strcmp
	cmp     w0, 0
	bne     .L8
	ldr     x0, [sp, 40]
	ldr     w0, [x0, 8]
	b       .L9
.L8:
	ldr     x0, [sp, 40]
	add     x0, x0, 24
	str     x0, [sp, 40]
.L7:
	ldr     x0, [sp, 40]
	ldr     w0, [x0, 8]
	cmp     w0, 0
	bne     .L10
	mov     w0, 0
.L9:
	ldp     x29, x30, [sp], 48
	.cfi_def_cfa_register 31
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
