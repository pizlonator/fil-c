# Testcase for cfg creation.
# There is at least one bb here with a single GINSN_TYPE_SYMBOL instruction
# for a user-defined label.  This ginsn is visited in the fallthrough path of
# another bb.
	.text
	.global main
	.type   main, %function
main:
	.cfi_startproc
	stp     x29, x30, [sp, -32]!
	.cfi_def_cfa_offset 32
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	mov     x29, sp
	.cfi_def_cfa_register 29
	cmp     w0, 0
	bne     .L2
#	ldr	x0, [sp, 24]
#	bl	fclose
	cmp     w0, 0
	beq     .L3
.L2:
	mov     w0, 1
	b       .L5
.L3:
	mov     w0, 0
.L5:
	ldp     x29, x30, [sp], 32
	.cfi_def_cfa_register 31
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
